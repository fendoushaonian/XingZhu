#include "core/texture_manager.h"

#include <wincodec.h>
#include <winhttp.h>
#include <urlmon.h>
#include <shlobj.h>
#include <filesystem>
#include <deque>
#include <queue>
#include <set>
#include <cstdio>
#include <cmath>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace sf {

// ---- Internal state ----
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_deviceCtx = nullptr;
static ID3D11SamplerState* g_anisoSampler = nullptr;

struct TexEntry {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 0, h = 0;
};

static std::unordered_map<std::string, TexEntry> g_textures;
static std::set<std::string> g_requested; // all ever-requested IDs

// Threading — CDN downloads
static std::mutex g_mutex;
static std::deque<std::string> g_priorityQueue;   // library textures (drain first)
static std::queue<std::string> g_downloadQueue;    // normal textures
static std::vector<std::pair<std::string, std::wstring>> g_completed; // appId -> filePath
static constexpr int NUM_WORKERS = 8;
static std::thread g_workers[NUM_WORKERS];
static std::atomic<bool> g_running{false};

// Threading — URL downloads (multi-threaded for maximum throughput)
struct UrlDownloadRequest {
    std::string key;
    std::string url;
};
static std::mutex g_urlMutex;
static std::queue<UrlDownloadRequest> g_urlQueue;
static std::set<std::string> g_urlRequested;
static std::set<std::string> g_urlFailed;  // Track failed downloads for fallback
static std::vector<std::pair<std::string, std::wstring>> g_urlCompleted;
static constexpr int NUM_URL_WORKERS = 12;  // More workers for URL downloads (about images, screenshots, etc.)
static std::thread g_urlWorkers[NUM_URL_WORKERS];
static std::atomic<bool> g_urlRunning{false};
static std::atomic<int> g_urlActiveWorkers{0};

static std::wstring g_cacheDir;

// ---- Helpers ----

static std::wstring GetCacheDir() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        std::wstring dir = std::wstring(path) + L"\\SteamForge\\cache";
        std::filesystem::create_directories(dir);
        return dir;
    }
    // Fallback: next to exe
    std::filesystem::create_directories(L"cache");
    return L"cache";
}

static bool IsValidJpeg(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    unsigned char magic[3] = {};
    fread(magic, 1, 3, f);
    fclose(f);
    return (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF);
}

static bool IsValidPng(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    unsigned char magic[4] = {};
    fread(magic, 1, 4, f);
    fclose(f);
    return (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G');
}

// Fast WinHTTP download with short timeouts (replaces slow URLDownloadToFileW)
static bool WinHttpDownloadToFile(const wchar_t* fullUrl, const std::wstring& outPath) {
    // Parse URL: skip "https://"
    const wchar_t* p = fullUrl;
    bool isHttps = true;
    if (wcsncmp(p, L"https://", 8) == 0) p += 8;
    else if (wcsncmp(p, L"http://", 7) == 0) { p += 7; isHttps = false; }

    const wchar_t* slash = wcschr(p, L'/');
    if (!slash) {
        fprintf(stderr, "[URL] No slash in URL\n");
        return false;
    }
    std::wstring host(p, slash);
    std::wstring path(slash);

    HINTERNET hSession = WinHttpOpen(L"SteamForge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        fprintf(stderr, "[URL] WinHttpOpen failed\n");
        return false;
    }

    // Longer timeouts for large images: 5s resolve, 5s connect, 10s send, 30s receive
    WinHttpSetTimeouts(hSession, 5000, 5000, 10000, 30000);

    INTERNET_PORT port = isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        fprintf(stderr, "[URL] WinHttpConnect failed for host: %ls\n", host.c_str());
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        fprintf(stderr, "[URL] WinHttpOpenRequest failed\n");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        fprintf(stderr, "[URL] WinHttpSendRequest failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        fprintf(stderr, "[URL] WinHttpReceiveResponse failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Check HTTP status
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &statusCode, &statusSize, NULL);
    if (statusCode != 200) {
        fprintf(stderr, "[URL] HTTP status %lu\n", statusCode);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read body to file
    FILE* f = _wfopen(outPath.c_str(), L"wb");
    if (!f) {
        fprintf(stderr, "[URL] Cannot open output file: %ls\n", outPath.c_str());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    BYTE buf[8192];
    DWORD bytesRead;
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0)
        fwrite(buf, 1, bytesRead, f);
    fclose(f);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

static bool DownloadSteamImage(const std::string& appId, const std::wstring& outPath) {
    std::wstring appIdW(appId.begin(), appId.end());

    // Try high-res images first for better quality, then fall back to common ones
    const wchar_t* urlPatterns[] = {
        L"https://cdn.akamai.steamstatic.com/steam/apps/%s/capsule_616x353.jpg",   // 616x353 (good quality, very common)
        L"https://cdn.akamai.steamstatic.com/steam/apps/%s/header.jpg",            // 460x215 (most common fallback)
        L"https://cdn.akamai.steamstatic.com/steam/apps/%s/library_600x900.jpg",   // 600x900 (portrait, high-res)
        L"https://cdn.akamai.steamstatic.com/steam/apps/%s/library_hero.jpg",      // 1920x620 (banner, not all games have)
        L"https://cdn.akamai.steamstatic.com/steam/apps/%s/capsule_231x87.jpg",    // 231x87 (small fallback)
        nullptr
    };

    for (int i = 0; urlPatterns[i]; i++) {
        wchar_t url[512];
        _snwprintf(url, 512, urlPatterns[i], appIdW.c_str());

        if (WinHttpDownloadToFile(url, outPath) &&
            (IsValidJpeg(outPath) || IsValidPng(outPath)))
            return true;

        _wremove(outPath.c_str());
    }
    return false;
}

// Cached WIC factory — creating this per-image was the #1 bottleneck
static IWICImagingFactory* g_wicFactory = nullptr;

static IWICImagingFactory* GetWicFactory() {
    if (!g_wicFactory) {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&g_wicFactory));
    }
    return g_wicFactory;
}

static bool LoadImageWIC(const std::wstring& path, ID3D11Device* device, TexEntry& entry) {
    IWICImagingFactory* factory = GetWicFactory();
    if (!factory) return false;

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), NULL, GENERIC_READ,
                                             WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) return false;

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);
    if (!frame) { decoder->Release(); return false; }

    IWICFormatConverter* converter = nullptr;
    factory->CreateFormatConverter(&converter);
    if (!converter) { frame->Release(); decoder->Release(); return false; }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                          WICBitmapDitherTypeNone, NULL, 0.0,
                          WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); return false; }

    UINT w, h;
    hr = converter->GetSize(&w, &h);
    if (FAILED(hr) || w == 0 || h == 0) { converter->Release(); frame->Release(); decoder->Release(); return false; }

    BYTE* pixels = new BYTE[w * h * 4];
    memset(pixels, 0, w * h * 4);  // Initialize to black/transparent
    hr = converter->CopyPixels(NULL, w * 4, w * h * 4, pixels);
    if (FAILED(hr)) { delete[] pixels; converter->Release(); frame->Release(); decoder->Release(); return false; }

    // Create D3D11 texture — single mip level, pass data directly (fast path)
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    hr = device->CreateTexture2D(&desc, &initData, &tex);
    delete[] pixels;

    bool ok = false;
    if (SUCCEEDED(hr) && tex) {
        ID3D11ShaderResourceView* srv = nullptr;
        hr = device->CreateShaderResourceView(tex, nullptr, &srv);
        if (SUCCEEDED(hr) && srv) {
            entry.srv = srv;
            entry.w = (int)w;
            entry.h = (int)h;
            ok = true;
        }
        tex->Release();
    }

    converter->Release();
    frame->Release();
    decoder->Release();
    return ok;
}

// ---- Background worker ----

static void WorkerThread() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    while (g_running) {
        std::string appId;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_priorityQueue.empty()) {
                appId = g_priorityQueue.front();
                g_priorityQueue.pop_front();
            } else if (!g_downloadQueue.empty()) {
                appId = g_downloadQueue.front();
                g_downloadQueue.pop();
            }
        }

        if (appId.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        std::wstring appIdW(appId.begin(), appId.end());
        std::wstring filePath = g_cacheDir + L"\\" + appIdW + L".jpg";

        // Check cache first - verify it's a valid image file
        bool ok = false;
        if (std::filesystem::exists(filePath)) {
            // Verify file is a valid JPEG (not corrupted or wrong format)
            if (IsValidJpeg(filePath) || IsValidPng(filePath)) {
                ok = true;
            } else {
                // Invalid cache file - delete and re-download
                _wremove(filePath.c_str());
            }
        }
        if (!ok)
            ok = DownloadSteamImage(appId, filePath);

        if (ok) {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_completed.push_back({appId, filePath});
        }
    }

    CoUninitialize();
}

// ---- Public API ----

void InitTextures(ID3D11Device* device) {
    g_device = device;
    device->GetImmediateContext(&g_deviceCtx);

    // Create anisotropic sampler for high-quality image scaling
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    sampDesc.MaxAnisotropy = 8;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sampDesc, &g_anisoSampler);

    g_cacheDir = GetCacheDir();
    g_running = true;
    for (int i = 0; i < NUM_WORKERS; i++)
        g_workers[i] = std::thread(WorkerThread);
    fprintf(stderr, "[TEX]  Texture manager initialized (%d workers), cache: %ls\n", NUM_WORKERS, g_cacheDir.c_str());
}

void UpdateTextures() {
    if (!g_device) return;

    // Bind anisotropic sampler for high-quality image rendering
    if (g_anisoSampler && g_deviceCtx) {
        g_deviceCtx->PSSetSamplers(0, 1, &g_anisoSampler);
    }

    // Load completed textures (CDN + URL) on main thread, limited per frame.
    // Load from FRONT so the first-requested (most visible) textures appear first.
    constexpr int MAX_LOADS_PER_FRAME = 16;
    int loaded = 0;

    // CDN textures
    {
        std::vector<std::pair<std::string, std::wstring>> cdnBatch;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            int n = std::min((int)g_completed.size(), MAX_LOADS_PER_FRAME);
            if (n > 0) {
                cdnBatch.assign(std::make_move_iterator(g_completed.begin()),
                                std::make_move_iterator(g_completed.begin() + n));
                g_completed.erase(g_completed.begin(), g_completed.begin() + n);
            }
        }
        for (auto& [appId, path] : cdnBatch) {
            if (g_textures.count(appId)) continue;
            TexEntry entry;
            if (LoadImageWIC(path, g_device, entry)) {
                g_textures[appId] = entry;
                loaded++;
            } else {
                // Failed to load - delete corrupted cache file so it can be re-downloaded
                _wremove(path.c_str());
                // Remove from requested set so it can be requested again
                g_requested.erase(appId);
            }
        }
    }

    // URL textures (share the per-frame budget with CDN)
    {
        int urlBudget = MAX_LOADS_PER_FRAME - loaded;
        if (urlBudget > 0) {
            std::vector<std::pair<std::string, std::wstring>> urlBatch;
            {
                std::lock_guard<std::mutex> lock(g_urlMutex);
                int n = std::min((int)g_urlCompleted.size(), (int)urlBudget);
                if (n > 0) {
                    urlBatch.assign(std::make_move_iterator(g_urlCompleted.begin()),
                                    std::make_move_iterator(g_urlCompleted.begin() + n));
                    g_urlCompleted.erase(g_urlCompleted.begin(), g_urlCompleted.begin() + n);
                }
            }
            for (auto& [k, path] : urlBatch) {
                if (g_textures.count(k)) continue;
                TexEntry entry;
                if (LoadImageWIC(path, g_device, entry)) {
                    g_textures[k] = entry;
                } else {
                    // Failed to load - delete corrupted cache file
                    _wremove(path.c_str());
                    // Remove from requested set so it can be requested again
                    std::lock_guard<std::mutex> lock(g_urlMutex);
                    g_urlRequested.erase(k);
                }
            }
        }
    }
}

void ShutdownTextures() {
    g_running = false;
    g_urlRunning = false;

    // Join CDN workers
    for (int i = 0; i < NUM_WORKERS; i++)
        if (g_workers[i].joinable()) g_workers[i].join();

    // Join URL workers
    for (int i = 0; i < NUM_URL_WORKERS; i++)
        if (g_urlWorkers[i].joinable()) g_urlWorkers[i].join();

    for (auto& [id, entry] : g_textures) {
        if (entry.srv) entry.srv->Release();
    }
    g_textures.clear();
    g_requested.clear();
    g_urlRequested.clear();
    g_urlFailed.clear();
    if (g_wicFactory) { g_wicFactory->Release(); g_wicFactory = nullptr; }
    if (g_anisoSampler) { g_anisoSampler->Release(); g_anisoSampler = nullptr; }
    if (g_deviceCtx) { g_deviceCtx->Release(); g_deviceCtx = nullptr; }
    g_device = nullptr;
}

ImTextureID GetGameTexture(const std::string& appId) {
    auto it = g_textures.find(appId);
    if (it != g_textures.end()) return (ImTextureID)it->second.srv;

    // Auto-request on first access
    RequestGameTexture(appId);
    return (ImTextureID)0;
}

void RequestGameTexture(const std::string& appId) {
    if (appId.empty()) return;
    if (g_requested.count(appId)) return;
    g_requested.insert(appId);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_downloadQueue.push(appId);
}

void RequestGameTextures(const std::vector<std::string>& appIds) {
    for (auto& id : appIds) RequestGameTexture(id);
}

void RequestGameTexturePriority(const std::string& appId) {
    if (appId.empty()) return;
    if (g_requested.count(appId)) return;
    g_requested.insert(appId);

    std::lock_guard<std::mutex> lock(g_mutex);
    g_priorityQueue.push_back(appId);
}

void RequestGameTexturesPriority(const std::vector<std::string>& appIds) {
    for (auto& id : appIds) RequestGameTexturePriority(id);
}

// ---- Generic URL-based texture download ----

static void UrlWorkerThread() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // Ensure cache dir is initialized
    if (g_cacheDir.empty()) {
        g_cacheDir = GetCacheDir();
    }

    while (g_urlRunning) {
        UrlDownloadRequest req;
        {
            std::lock_guard<std::mutex> lock(g_urlMutex);
            if (!g_urlQueue.empty()) {
                req = g_urlQueue.front();
                g_urlQueue.pop();
            }
        }

        if (req.key.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Generate cache filename from key
        std::string safeKey = req.key;
        for (char& c : safeKey) {
            if (c == '/' || c == '\\' || c == ':' || c == '?' || c == '*' || c == '"' || c == '<' || c == '>' || c == '|')
                c = '_';
        }
        std::wstring safeKeyW(safeKey.begin(), safeKey.end());
        std::wstring filePath = g_cacheDir + L"\\url_" + safeKeyW + L".jpg";

        bool exists = std::filesystem::exists(filePath);
        // If file exists, verify it's a valid format WIC can decode
        if (exists) {
            FILE* f = _wfopen(filePath.c_str(), L"rb");
            if (f) {
                unsigned char magic[12] = {};
                size_t read = fread(magic, 1, 12, f);
                fclose(f);
                bool validFormat = false;
                if (read >= 3) {
                    if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF) validFormat = true; // JPEG
                    else if (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E) validFormat = true; // PNG
                    else if (magic[0] == 'B' && magic[1] == 'M') validFormat = true; // BMP
                    else if (magic[0] == 'G' && magic[1] == 'I' && magic[2] == 'F') validFormat = true; // GIF
                    else if (magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F') validFormat = true; // WebP
                    // AVIF/HEIF: ftyp box at offset 4 with brand avif/heic/mif1
                    else if (read >= 12 && magic[4] == 'f' && magic[5] == 't' && magic[6] == 'y' && magic[7] == 'p') {
                        if ((magic[8] == 'a' && magic[9] == 'v' && magic[10] == 'i' && magic[11] == 'f') ||
                            (magic[8] == 'h' && magic[9] == 'e' && magic[10] == 'i' && magic[11] == 'c') ||
                            (magic[8] == 'm' && magic[9] == 'i' && magic[10] == 'f' && magic[11] == '1'))
                            validFormat = true; // AVIF/HEIF
                    }
                }
                if (!validFormat) {
                    // Invalid format - delete and re-download
                    _wremove(filePath.c_str());
                    exists = false;
                }
            }
        }
        if (!exists) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, req.url.c_str(), -1, nullptr, 0);
            std::wstring urlW(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, req.url.c_str(), -1, &urlW[0], wlen);
            if (!urlW.empty() && urlW.back() == L'\0') urlW.pop_back();

            // Use faster WinHttp instead of slow URLDownloadToFileW
            bool downloaded = WinHttpDownloadToFile(urlW.c_str(), filePath);
            if (downloaded) {
                // Verify image format - accept formats WIC can decode (including AVIF on Win10+)
                FILE* f = _wfopen(filePath.c_str(), L"rb");
                if (f) {
                    unsigned char magic[12] = {};
                    size_t read = fread(magic, 1, 12, f);
                    fclose(f);

                    bool validImage = false;
                    if (read >= 3) {
                        if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF) validImage = true; // JPEG
                        else if (magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E) validImage = true; // PNG
                        else if (magic[0] == 'B' && magic[1] == 'M') validImage = true; // BMP
                        else if (magic[0] == 'G' && magic[1] == 'I' && magic[2] == 'F') validImage = true; // GIF
                        else if (magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F') validImage = true; // WebP
                        // AVIF/HEIF: ftyp box at offset 4 with brand avif/heic/mif1
                        else if (read >= 12 && magic[4] == 'f' && magic[5] == 't' && magic[6] == 'y' && magic[7] == 'p') {
                            if ((magic[8] == 'a' && magic[9] == 'v' && magic[10] == 'i' && magic[11] == 'f') ||
                                (magic[8] == 'h' && magic[9] == 'e' && magic[10] == 'i' && magic[11] == 'c') ||
                                (magic[8] == 'm' && magic[9] == 'i' && magic[10] == 'f' && magic[11] == '1'))
                                validImage = true; // AVIF/HEIF (requires AV1 Video Extension on Win10)
                        }
                    }

                    if (validImage) {
                        exists = true;
                    } else {
                        fprintf(stderr, "[URL] Invalid format for %s: %02X %02X %02X %02X\n",
                                req.key.c_str(), magic[0], magic[1], magic[2], magic[3]);
                        _wremove(filePath.c_str());
                    }
                }
            } else {
                fprintf(stderr, "[URL] Download failed for %s\n", req.key.c_str());
            }
        }

        if (exists) {
            std::lock_guard<std::mutex> lock(g_urlMutex);
            g_urlCompleted.push_back({req.key, filePath});
            fprintf(stderr, "[URL] Downloaded: %s\n", req.key.c_str());
        } else {
            // Mark as failed so GetBestTexture can fall back to CDN texture
            std::lock_guard<std::mutex> lock(g_urlMutex);
            g_urlFailed.insert(req.key);
            fprintf(stderr, "[URL] Failed: %s\n", req.key.c_str());
        }
    }

    CoUninitialize();
}

void RequestTextureFromUrl(const std::string& key, const std::string& url) {
    if (key.empty() || url.empty()) return;

    {
        std::lock_guard<std::mutex> lock(g_urlMutex);
        // Skip if already loaded
        if (g_textures.count(key)) return;
        // Skip if already in queue or being processed (but not failed)
        if (g_urlRequested.count(key) && !g_urlFailed.count(key)) return;
        // If previously failed, allow retry
        if (g_urlFailed.count(key)) {
            g_urlFailed.erase(key);
            fprintf(stderr, "[URL] Retrying failed key: %s\n", key.c_str());
        }
        g_urlRequested.insert(key);
        g_urlQueue.push({key, url});
        fprintf(stderr, "[URL] Queued: %s -> %s\n", key.c_str(), url.substr(0, 80).c_str());
    }

    // Start all worker threads if not running
    if (!g_urlRunning.exchange(true)) {
        fprintf(stderr, "[URL] Starting %d worker threads\n", NUM_URL_WORKERS);
        for (int i = 0; i < NUM_URL_WORKERS; i++) {
            g_urlWorkers[i] = std::thread(UrlWorkerThread);
        }
    }
}

ImTextureID GetTextureByKey(const std::string& key) {
    auto it = g_textures.find(key);
    if (it != g_textures.end()) return (ImTextureID)it->second.srv;
    return (ImTextureID)0;
}

bool IsUrlTextureFailed(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_urlMutex);
    return g_urlFailed.count(key) > 0;
}

ImTextureID LoadTextureFromLocalFile(const std::string& key, const std::string& filePath) {
    if (!g_device) return (ImTextureID)0;

    // Already loaded?
    auto it = g_textures.find(key);
    if (it != g_textures.end() && it->second.srv)
        return (ImTextureID)it->second.srv;

    // Try to resolve relative path from executable directory
    std::wstring wpath;
    if (filePath.size() > 2 && filePath[1] == ':') {
        // Absolute path
        int wlen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
        wpath.resize(wlen);
        MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wpath[0], wlen);
    } else {
        // Relative path - resolve from executable directory
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring exeDir(exePath);
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = exeDir.substr(0, lastSlash + 1);
        }
        // Convert relative path to wide string
        int wlen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
        std::wstring relPath(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &relPath[0], wlen);

        // Try exe directory first
        wpath = exeDir + relPath;
        if (!std::filesystem::exists(wpath)) {
            // Try going up directories (build/Release -> project root)
            for (int i = 0; i < 3; i++) {
                size_t slash = exeDir.find_last_of(L"\\/", exeDir.size() - 2);
                if (slash != std::wstring::npos) {
                    exeDir = exeDir.substr(0, slash + 1);
                    wpath = exeDir + relPath;
                    if (std::filesystem::exists(wpath)) break;
                }
            }
        }
    }

    TexEntry entry;
    if (LoadImageWIC(wpath, g_device, entry)) {
        g_textures[key] = entry;
        return (ImTextureID)entry.srv;
    }
    return (ImTextureID)0;
}

// ---- Texture size queries ----

bool GetTextureSize(const std::string& key, int& outW, int& outH) {
    auto it = g_textures.find(key);
    if (it == g_textures.end() || !it->second.srv) return false;
    outW = it->second.w;
    outH = it->second.h;
    return true;
}

bool GetGameTextureSize(const std::string& appId, int& outW, int& outH) {
    return GetTextureSize(appId, outW, outH);
}

// ---- Aspect-ratio-aware image drawing ----

void DrawImageFit(ImDrawList* dl, ImTextureID tex, int texW, int texH,
                  float x, float y, float w, float h, int mode,
                  float rounding, ImU32 tint) {
    if (!tex || texW <= 0 || texH <= 0 || w <= 0 || h <= 0) return;

    float srcAspect = (float)texW / (float)texH;
    float dstAspect = w / h;

    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;

    if (mode == 0) {
        // Cover: fill the target rect, crop the excess from the source
        if (srcAspect > dstAspect) {
            // Source is wider — crop left/right
            float visibleFrac = dstAspect / srcAspect;
            float offset = (1.0f - visibleFrac) * 0.5f;
            u0 = offset;
            u1 = 1.0f - offset;
        } else {
            // Source is taller — crop top/bottom
            float visibleFrac = srcAspect / dstAspect;
            float offset = (1.0f - visibleFrac) * 0.5f;
            v0 = offset;
            v1 = 1.0f - offset;
        }

        if (rounding > 0.0f)
            dl->AddImageRounded(tex, ImVec2(x, y), ImVec2(x + w, y + h),
                                ImVec2(u0, v0), ImVec2(u1, v1), tint, rounding);
        else
            dl->AddImage(tex, ImVec2(x, y), ImVec2(x + w, y + h),
                         ImVec2(u0, v0), ImVec2(u1, v1), tint);
    } else {
        // Contain: fit inside the target rect, center (no crop)
        float drawW, drawH;
        if (srcAspect > dstAspect) {
            drawW = w;
            drawH = w / srcAspect;
        } else {
            drawH = h;
            drawW = h * srcAspect;
        }
        float dx = x + (w - drawW) * 0.5f;
        float dy = y + (h - drawH) * 0.5f;

        if (rounding > 0.0f)
            dl->AddImageRounded(tex, ImVec2(dx, dy), ImVec2(dx + drawW, dy + drawH),
                                ImVec2(0, 0), ImVec2(1, 1), tint, rounding);
        else
            dl->AddImage(tex, ImVec2(dx, dy), ImVec2(dx + drawW, dy + drawH),
                         ImVec2(0, 0), ImVec2(1, 1), tint);
    }
}

} // namespace sf
