#include "core/video_player.h"
#include "utils/logger.h"

#include <windows.h>
#include <d3d11_4.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <mfidl.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <atomic>
#include <string>
#include <vector>
#include <limits>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

namespace sf {

// ============================================================
// IMFMediaEngineNotify — single persistent instance (never destroyed until shutdown)
// ============================================================
class MediaEngineNotify : public IMFMediaEngineNotify {
public:
    MediaEngineNotify() : refCount_(1) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IMFMediaEngineNotify) {
            *ppv = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refCount_); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG c = InterlockedDecrement(&refCount_);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR param1, DWORD param2) override {
        switch (event) {
        case MF_MEDIA_ENGINE_EVENT_CANPLAY:
            canPlay_.store(true);
            break;
        case MF_MEDIA_ENGINE_EVENT_PLAYING:
            playing_.store(true);
            break;
        case MF_MEDIA_ENGINE_EVENT_PAUSE:
            playing_.store(false);
            break;
        case MF_MEDIA_ENGINE_EVENT_ENDED:
            ended_.store(true);
            playing_.store(false);
            break;
        case MF_MEDIA_ENGINE_EVENT_ERROR:
            Log(LogLevel::Error, "MediaEngine error: param1=%lu param2=%lu",
                (unsigned long)param1, (unsigned long)param2);
            error_.store(true);
            playing_.store(false);
            break;
        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
            metadataLoaded_.store(true);
            break;
        }
        return S_OK;
    }

    std::atomic<bool> canPlay_{false};
    std::atomic<bool> playing_{false};
    std::atomic<bool> ended_{false};
    std::atomic<bool> error_{false};
    std::atomic<bool> metadataLoaded_{false};

    void Reset() {
        canPlay_ = false;
        playing_ = false;
        ended_ = false;
        error_ = false;
        metadataLoaded_ = false;
    }

private:
    LONG refCount_;
};

// ============================================================
// Module state
// ============================================================
static ID3D11Device*               g_rawDevice = nullptr;
static ComPtr<ID3D11DeviceContext>  g_ctx;
static ComPtr<IMFDXGIDeviceManager> g_dxgiManager;
static ComPtr<IMFMediaEngine>      g_engine;
static MediaEngineNotify*          g_notify = nullptr;  // persistent, never recreated
static ComPtr<ID3D11Texture2D>     g_videoTex;
static ComPtr<ID3D11ShaderResourceView> g_videoSRV;
// Retired SRVs: keep old SRVs alive for 2 frames so ImGui doesn't crash
static std::vector<std::pair<ComPtr<ID3D11ShaderResourceView>, int>> g_retiredSRVs;
static int g_videoW = 0;
static int g_videoH = 0;
static bool g_initialized = false;
static bool g_mfStarted = false;
static bool g_paused = false;
static bool g_active = false;
static UINT g_resetToken = 0;
static int g_frameCount = 0;

// ============================================================
// Retire old SRV (keep alive for 2 frames for ImGui safety)
// ============================================================
static void RetireSRV() {
    if (g_videoSRV) {
        g_retiredSRVs.push_back({g_videoSRV, g_frameCount});
        g_videoSRV.Reset();
    }
}

static void PurgeRetiredSRVs() {
    g_retiredSRVs.erase(
        std::remove_if(g_retiredSRVs.begin(), g_retiredSRVs.end(),
            [](const auto& p) { return g_frameCount - p.second >= 3; }),
        g_retiredSRVs.end());
}

// ============================================================
// Create engine (reuse same notify object)
// ============================================================
static bool CreateEngine() {
    if (!g_rawDevice || !g_dxgiManager || !g_notify) return false;

    // Shutdown old engine cleanly
    if (g_engine) {
        g_engine->Pause();
        g_engine->Shutdown();
        g_engine.Reset();
        // Give MF background threads time to drain
        Sleep(30);
    }

    // Reset notify state (but don't destroy the object!)
    g_notify->Reset();

    ComPtr<IMFAttributes> attrs;
    HRESULT hr = MFCreateAttributes(&attrs, 3);
    if (FAILED(hr)) return false;

    attrs->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, g_notify);
    attrs->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, g_dxgiManager.Get());
    attrs->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);

    ComPtr<IMFMediaEngineClassFactory> factory;
    hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        Log(LogLevel::Error, "CoCreateInstance MFMediaEngineClassFactory failed: 0x%08X", hr);
        return false;
    }

    hr = factory->CreateInstance(0, attrs.Get(), &g_engine);
    if (FAILED(hr)) {
        Log(LogLevel::Error, "CreateInstance MediaEngine failed: 0x%08X", hr);
        return false;
    }

    return true;
}

// ============================================================
// Init / Shutdown
// ============================================================
void InitVideoPlayer(ID3D11Device* device) {
    if (g_initialized) return;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        Log(LogLevel::Error, "MFStartup failed: 0x%08X", hr);
        return;
    }
    g_mfStarted = true;

    g_rawDevice = device;
    device->GetImmediateContext(&g_ctx);

    // Enable D3D11 multithread protection
    {
        ID3D11Multithread* mt = nullptr;
        hr = device->QueryInterface(__uuidof(ID3D11Multithread), (void**)&mt);
        if (SUCCEEDED(hr) && mt) {
            mt->SetMultithreadProtected(TRUE);
            mt->Release();
            Log(LogLevel::Info, "VideoPlayer: D3D11 multithread protection enabled");
        }
    }

    // Create DXGI device manager
    hr = MFCreateDXGIDeviceManager(&g_resetToken, &g_dxgiManager);
    if (FAILED(hr)) {
        Log(LogLevel::Error, "MFCreateDXGIDeviceManager failed: 0x%08X", hr);
        return;
    }

    hr = g_dxgiManager->ResetDevice(device, g_resetToken);
    if (FAILED(hr)) {
        Log(LogLevel::Error, "DXGIDeviceManager::ResetDevice failed: 0x%08X", hr);
        return;
    }

    // Create single persistent notify object
    g_notify = new MediaEngineNotify();
    // AddRef so it stays alive even when engine releases it
    g_notify->AddRef();

    // Create initial engine
    if (!CreateEngine()) return;

    g_initialized = true;
    Log(LogLevel::Info, "VideoPlayer initialized with Media Foundation");
}

void ShutdownVideoPlayer() {
    g_active = false;

    if (g_engine) {
        g_engine->Pause();
        g_engine->Shutdown();
        g_engine.Reset();
    }

    g_videoSRV.Reset();
    g_videoTex.Reset();
    g_retiredSRVs.clear();
    g_dxgiManager.Reset();
    g_ctx.Reset();
    g_rawDevice = nullptr;

    if (g_notify) {
        // Release our extra ref + the initial ref
        g_notify->Release();
        g_notify->Release();
        g_notify = nullptr;
    }

    if (g_mfStarted) {
        MFShutdown();
        g_mfStarted = false;
    }

    g_initialized = false;
    g_videoW = g_videoH = 0;
}

// ============================================================
// Ensure video texture
// ============================================================
static bool EnsureVideoTexture(int w, int h) {
    if (g_videoTex && g_videoW == w && g_videoH == h)
        return true;

    // Retire old SRV instead of destroying it immediately
    RetireSRV();
    g_videoTex.Reset();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    HRESULT hr = g_rawDevice->CreateTexture2D(&desc, nullptr, &g_videoTex);
    if (FAILED(hr)) return false;

    hr = g_rawDevice->CreateShaderResourceView(g_videoTex.Get(), nullptr, &g_videoSRV);
    if (FAILED(hr)) { g_videoTex.Reset(); return false; }

    g_videoW = w;
    g_videoH = h;
    return true;
}

// ============================================================
// Playback control
// ============================================================
bool PlayVideo(const std::string& url) {
    if (!g_initialized) {
        Log(LogLevel::Error, "VideoPlayer: not initialized");
        return false;
    }

    // Don't touch g_videoSRV/g_videoTex here!
    // ImGui may still be referencing the current SRV from its draw list.
    // The old texture will be retired naturally in EnsureVideoTexture when
    // the new video's first frame arrives with a new resolution.
    g_paused = false;
    g_active = false;

    // Recreate engine for clean state
    if (!CreateEngine()) {
        Log(LogLevel::Error, "VideoPlayer: failed to recreate engine");
        return false;
    }

    // Convert URL to wide string
    int len = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring wUrl(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wUrl[0], len);
    if (!wUrl.empty() && wUrl.back() == L'\0') wUrl.pop_back();

    BSTR bstr = SysAllocString(wUrl.c_str());
    if (!bstr) return false;

    HRESULT hr = g_engine->SetSource(bstr);
    SysFreeString(bstr);

    if (FAILED(hr)) {
        Log(LogLevel::Error, "MediaEngine SetSource failed: 0x%08X", hr);
        return false;
    }

    g_active = true;
    Log(LogLevel::Info, "VideoPlayer: loading %s", url.c_str());
    return true;
}

void StopVideo() {
    if (g_engine) {
        g_engine->Pause();
    }
    if (g_notify) g_notify->Reset();
    g_paused = false;
    g_active = false;
}

void PauseVideo() {
    if (g_engine && g_active) {
        g_engine->Pause();
        g_paused = true;
    }
}

void ResumeVideo() {
    if (g_engine && g_active && g_paused) {
        g_engine->Play();
        g_paused = false;
    }
}

bool IsVideoPlaying() {
    return g_notify && g_notify->playing_.load() && !g_paused;
}

bool IsVideoPaused() {
    return g_active && g_paused;
}

bool IsVideoActive() {
    return g_active;
}

double GetVideoDuration() {
    if (!g_engine) return 0.0;
    double d = g_engine->GetDuration();
    if (d != d || d == std::numeric_limits<double>::infinity()) return 0.0;
    return d;
}

double GetVideoPosition() {
    if (!g_engine) return 0.0;
    return g_engine->GetCurrentTime();
}

void SeekVideo(double seconds) {
    if (!g_engine) return;
    g_engine->SetCurrentTime(seconds);
}

// ============================================================
// Per-frame update
// ============================================================
void UpdateVideoPlayer() {
    g_frameCount++;
    PurgeRetiredSRVs();

    if (!g_initialized || !g_engine || !g_notify || !g_active) return;

    // If error occurred, stop gracefully
    if (g_notify->error_.load()) {
        Log(LogLevel::Error, "VideoPlayer: media error, stopping playback");
        g_active = false;
        g_notify->Reset();
        return;
    }

    // Auto-ended
    if (g_notify->ended_.load()) {
        g_active = false;
        return;
    }

    // Auto-play when ready
    if (g_notify->canPlay_.load()) {
        g_notify->canPlay_.store(false);
        if (!g_paused) {
            g_engine->Play();
        }
    }

    // Only transfer frames when engine has video
    if (g_engine->HasVideo() == FALSE) return;

    // Get native video size
    DWORD nativeW = 0, nativeH = 0;
    g_engine->GetNativeVideoSize(&nativeW, &nativeH);
    if (nativeW == 0 || nativeH == 0) return;

    // Check if engine has a new frame ready
    LONGLONG pts = 0;
    if (g_engine->OnVideoStreamTick(&pts) == S_FALSE) return;

    // Ensure texture
    if (!EnsureVideoTexture((int)nativeW, (int)nativeH)) return;

    // Transfer frame
    __try {
        MFVideoNormalizedRect srcRect = {0.0f, 0.0f, 1.0f, 1.0f};
        RECT dstRect = {0, 0, (LONG)nativeW, (LONG)nativeH};
        g_engine->TransferVideoFrame(g_videoTex.Get(), &srcRect, &dstRect, nullptr);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Error, "VideoPlayer: exception in TransferVideoFrame");
        g_notify->error_.store(true);
    }
}

ImTextureID GetVideoFrame(int* outWidth, int* outHeight) {
    if (!g_videoSRV) return (ImTextureID)0;
    if (outWidth) *outWidth = g_videoW;
    if (outHeight) *outHeight = g_videoH;
    return (ImTextureID)g_videoSRV.Get();
}

} // namespace sf
