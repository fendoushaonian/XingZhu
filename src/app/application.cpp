#include "app/application.h"
#include "app/config.h"
#include "ui/theme.h"
#include "ui/iconfonts.h"
#include "core/texture_manager.h"
#include "core/steam_store.h"
#include "core/video_player.h"
#include "core/database.h"
#include "core/auth.h"
#include "core/navigation.h"
#include "core/steam_detect.h"
#include "core/install_record.h"
#include "core/game_process.h"
#include "core/steam_emu.h"
#include "ui/toast.h"
#include "ui/widgets/floating_ball.h"
#include "utils/logger.h"
#include <filesystem>

#include <dwmapi.h>
#include <imm.h>
#include <shellapi.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "windowscodecs.lib")

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ── IME state ──
static POINT g_imePos = {0, 0};
static LONG  g_imeLineH = 20;
static bool  g_imeWantVisible = false;

// IME candidate list (self-rendered because Win11 TextInputHost.exe doesn't work with IMM32 apps)
struct ImeCandidateState {
    bool visible = false;
    std::vector<std::string> items; // UTF-8
    int selection = 0;
    int pageStart = 0;
    int pageSize = 0;
    std::string compositionStr; // UTF-8 composition text
};
static ImeCandidateState g_imeCand;

// Helper: wchar_t* → UTF-8 std::string
static std::string WideToUtf8(const wchar_t* w, int len = -1) {
    if (!w) return "";
    int need = WideCharToMultiByte(CP_UTF8, 0, w, len, NULL, 0, NULL, NULL);
    if (need <= 0) return "";
    std::string s(need, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, len, &s[0], need, NULL, NULL);
    // Remove trailing null if len was -1
    while (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

// Helper: UTF-8 std::string → wstring (用于 filesystem 操作支持中文路径)
static std::wstring Utf8ToWideApp(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// 安全检查路径是否存在（支持中文路径）
static bool PathExistsSafeApp(const std::string& path) {
    if (path.empty()) return false;  // 空路径直接返回 false
    try {
        std::wstring wpath = Utf8ToWideApp(path);
        if (wpath.empty()) return false;
        return std::filesystem::exists(wpath) && std::filesystem::is_directory(wpath);
    } catch (...) {
        return false;
    }
}

// Helper: UTF-8 std::string → wstring
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    // Remove trailing null
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// Read candidate list from IME context
static void ReadImeCandidates(HWND hwnd) {
    HIMC himc = ::ImmGetContext(hwnd);
    if (!himc) return;

    DWORD bufSize = ::ImmGetCandidateListW(himc, 0, NULL, 0);
    if (bufSize > 0) {
        std::vector<BYTE> buf(bufSize);
        CANDIDATELIST* cl = (CANDIDATELIST*)buf.data();
        if (::ImmGetCandidateListW(himc, 0, cl, bufSize)) {
            g_imeCand.items.clear();
            g_imeCand.selection = (int)cl->dwSelection;
            g_imeCand.pageStart = (int)cl->dwPageStart;
            g_imeCand.pageSize  = (int)cl->dwPageSize;
            if (g_imeCand.pageSize <= 0) g_imeCand.pageSize = 5;

            DWORD end = cl->dwPageStart + cl->dwPageSize;
            if (end > cl->dwCount) end = cl->dwCount;

            for (DWORD i = cl->dwPageStart; i < end; i++) {
                const wchar_t* str = (const wchar_t*)((const BYTE*)cl + cl->dwOffset[i]);
                g_imeCand.items.push_back(WideToUtf8(str));
            }
            g_imeCand.visible = !g_imeCand.items.empty();
        }
    }

    // Read composition string
    LONG compLen = ::ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
    if (compLen > 0) {
        std::vector<wchar_t> compBuf(compLen / sizeof(wchar_t) + 1, 0);
        ::ImmGetCompositionStringW(himc, GCS_COMPSTR, compBuf.data(), compLen);
        g_imeCand.compositionStr = WideToUtf8(compBuf.data());
    } else {
        g_imeCand.compositionStr.clear();
    }

    ::ImmReleaseContext(hwnd, himc);
}

// Render IME candidate popup using ImGui foreground draw list
static void RenderImeCandidatePopup() {
    if (!g_imeCand.visible || g_imeCand.items.empty() || !g_imeWantVisible)
        return;

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    float fontSize = font->FontSize;

    float x = (float)g_imePos.x;
    float y = (float)(g_imePos.y + g_imeLineH + 6);
    float padX = 10.0f;
    float padY = 6.0f;
    float itemH = fontSize + 8.0f;
    float numW = fontSize * 1.2f; // width for "1." prefix

    // Calculate max item width
    float maxTextW = 0;
    for (size_t i = 0; i < g_imeCand.items.size(); i++) {
        char prefix[8];
        snprintf(prefix, sizeof(prefix), "%d.", (int)(i + 1));
        ImVec2 prefSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0, prefix);
        ImVec2 textSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0, g_imeCand.items[i].c_str());
        float w = prefSz.x + 6.0f + textSz.x;
        if (w > maxTextW) maxTextW = w;
    }

    float popupW = maxTextW + padX * 2;
    if (popupW < 180.0f) popupW = 180.0f;
    float popupH = padY * 2 + itemH * g_imeCand.items.size();

    // Clamp to screen
    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    if (x + popupW > dispSize.x) x = dispSize.x - popupW - 4;
    if (y + popupH > dispSize.y) y = (float)(g_imePos.y - popupH - 4); // flip above
    if (x < 0) x = 4;
    if (y < 0) y = 4;

    // Background
    ImU32 bgCol = IM_COL32(30, 30, 35, 245);
    ImU32 borderCol = IM_COL32(70, 80, 100, 180);
    fg->AddRectFilled(ImVec2(x, y), ImVec2(x + popupW, y + popupH), bgCol, 6.0f);
    fg->AddRect(ImVec2(x, y), ImVec2(x + popupW, y + popupH), borderCol, 6.0f);

    // Items
    int selIdx = g_imeCand.selection - g_imeCand.pageStart;
    for (size_t i = 0; i < g_imeCand.items.size(); i++) {
        float iy = y + padY + i * itemH;
        bool isSel = ((int)i == selIdx);

        // Selection highlight
        if (isSel) {
            fg->AddRectFilled(ImVec2(x + 3, iy), ImVec2(x + popupW - 3, iy + itemH),
                              IM_COL32(26, 120, 220, 100), 4.0f);
        }

        // Number prefix
        char prefix[8];
        snprintf(prefix, sizeof(prefix), "%d.", (int)(i + 1));
        ImU32 numCol = IM_COL32(26, 159, 255, 220);
        fg->AddText(font, fontSize, ImVec2(x + padX, iy + 4), numCol, prefix);

        // Candidate text
        ImVec2 prefSz = font->CalcTextSizeA(fontSize, FLT_MAX, 0, prefix);
        ImU32 textCol = isSel ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 220, 235, 240);
        fg->AddText(font, fontSize, ImVec2(x + padX + prefSz.x + 6.0f, iy + 4),
                    textCol, g_imeCand.items[i].c_str());
    }
}

// ImGui IME callback — positions composition window
static void CustomImeCallback(ImGuiContext*, ImGuiViewport* viewport, ImGuiPlatformImeData* data) {
    HWND hwnd = (HWND)viewport->PlatformHandleRaw;
    if (!hwnd) hwnd = (HWND)viewport->PlatformHandle;
    if (!hwnd) return;

    g_imeWantVisible = data->WantVisible;

    if (data->WantVisible) {
        g_imePos.x = (LONG)data->InputPos.x;
        g_imePos.y = (LONG)data->InputPos.y;
        g_imeLineH = (LONG)data->InputLineHeight;

        HIMC himc = ::ImmGetContext(hwnd);
        if (himc) {
            COMPOSITIONFORM cf = {};
            cf.dwStyle = CFS_FORCE_POSITION;
            cf.ptCurrentPos = g_imePos;
            ::ImmSetCompositionWindow(himc, &cf);
            ::ImmReleaseContext(hwnd, himc);
        }
    } else {
        g_imeCand.visible = false;
        g_imeCand.items.clear();
    }
}

namespace sf {

// Static app pointer for WndProc
static Application* g_app = nullptr;
Application* Application::s_instance = nullptr;

// 全局函数：更新游戏安装状态
void NotifyGameInstalled(const std::string& appId) {
    if (g_app) {
        g_app->UpdateGameInstallStatus(appId, true);
    }
}

// 全局函数：更新游戏卸载状态
void NotifyGameUninstalled(const std::string& appId) {
    if (g_app) {
        g_app->UpdateGameInstallStatus(appId, false);
    }
}

// 全局函数：添加游戏到侧边栏
void NotifyGameAddedToLibrary(const std::string& appId, const std::string& name) {
    if (g_app) {
        g_app->AddGameToSidebar(appId, name);
    }
}

Application::Application() {
    g_app = this;
    s_instance = this;
}

Application::~Application() {
    Shutdown();
    g_app = nullptr;
    s_instance = nullptr;
}

bool Application::Init(HINSTANCE hInstance) {
    // Register window class
    wc_.cbSize        = sizeof(WNDCLASSEXW);
    wc_.style         = CS_CLASSDC;
    wc_.lpfnWndProc   = WndProc;
    wc_.hInstance      = hInstance;
    wc_.lpszClassName  = L"SteamForgeClass";
    wc_.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wc_.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc_);

    // Calculate center position on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - APP_WIDTH) / 2;
    int posY = (screenH - APP_HEIGHT) / 2;

    // Create window
    hwnd_ = CreateWindowExW(
        0,
        wc_.lpszClassName,
        L"\u661F\u94F8\u7545\u73A9",  // 星铸畅玩
        WS_OVERLAPPEDWINDOW,
        posX, posY,
        APP_WIDTH, APP_HEIGHT,
        NULL, NULL,
        wc_.hInstance,
        NULL
    );

    if (!hwnd_) {
        Log(LogLevel::Error, "Failed to create window");
        return false;
    }

    // Load and set window icon from login.png using WIC
    {
        IWICImagingFactory* pFactory = nullptr;
        CoInitialize(nullptr);
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&pFactory));
        if (SUCCEEDED(hr) && pFactory) {
            IWICBitmapDecoder* pDecoder = nullptr;
            hr = pFactory->CreateDecoderFromFilename(L"resources/icons/login.png", nullptr,
                                                      GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
            if (SUCCEEDED(hr) && pDecoder) {
                IWICBitmapFrameDecode* pFrame = nullptr;
                hr = pDecoder->GetFrame(0, &pFrame);
                if (SUCCEEDED(hr) && pFrame) {
                    // Scale to icon size (32x32 for big, 16x16 for small)
                    auto createIconFromScaled = [&](UINT iconSize) -> HICON {
                        IWICBitmapScaler* pScaler = nullptr;
                        hr = pFactory->CreateBitmapScaler(&pScaler);
                        if (FAILED(hr) || !pScaler) return nullptr;

                        hr = pScaler->Initialize(pFrame, iconSize, iconSize, WICBitmapInterpolationModeHighQualityCubic);
                        if (FAILED(hr)) { pScaler->Release(); return nullptr; }

                        IWICFormatConverter* pConverter = nullptr;
                        hr = pFactory->CreateFormatConverter(&pConverter);
                        if (FAILED(hr) || !pConverter) { pScaler->Release(); return nullptr; }

                        hr = pConverter->Initialize(pScaler, GUID_WICPixelFormat32bppBGRA,
                                                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
                        if (FAILED(hr)) { pConverter->Release(); pScaler->Release(); return nullptr; }

                        std::vector<BYTE> pixels(iconSize * iconSize * 4);
                        pConverter->CopyPixels(nullptr, iconSize * 4, (UINT)pixels.size(), pixels.data());

                        BITMAPV5HEADER bi = {};
                        bi.bV5Size = sizeof(BITMAPV5HEADER);
                        bi.bV5Width = iconSize;
                        bi.bV5Height = -(LONG)iconSize;
                        bi.bV5Planes = 1;
                        bi.bV5BitCount = 32;
                        bi.bV5Compression = BI_BITFIELDS;
                        bi.bV5RedMask = 0x00FF0000;
                        bi.bV5GreenMask = 0x0000FF00;
                        bi.bV5BlueMask = 0x000000FF;
                        bi.bV5AlphaMask = 0xFF000000;

                        HDC hdc = GetDC(nullptr);
                        void* bits = nullptr;
                        HBITMAP hBitmap = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
                        HICON hIcon = nullptr;
                        if (hBitmap && bits) {
                            memcpy(bits, pixels.data(), pixels.size());
                            HBITMAP hMask = CreateBitmap(iconSize, iconSize, 1, 1, nullptr);
                            ICONINFO ii = {};
                            ii.fIcon = TRUE;
                            ii.hbmMask = hMask;
                            ii.hbmColor = hBitmap;
                            hIcon = CreateIconIndirect(&ii);
                            DeleteObject(hMask);
                            DeleteObject(hBitmap);
                        }
                        ReleaseDC(nullptr, hdc);
                        pConverter->Release();
                        pScaler->Release();
                        return hIcon;
                    };

                    HICON hIconBig = createIconFromScaled(32);
                    HICON hIconSmall = createIconFromScaled(16);
                    if (hIconBig) SendMessage(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
                    if (hIconSmall) SendMessage(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

                    pFrame->Release();
                }
                pDecoder->Release();
            }
            pFactory->Release();
        }
    }

    // Dark title bar (Windows 11)
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd_, 20, &useDarkMode, sizeof(useDarkMode));

    COLORREF captionColor = 0x00211A17; // BGR for #171A21
    DwmSetWindowAttribute(hwnd_, 35, &captionColor, sizeof(captionColor));

    // Create D3D device
    if (!CreateDeviceD3D(hwnd_)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc_.lpszClassName, wc_.hInstance);
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // Apply Steam theme
    ApplySteamTheme();

    // Load fonts (icon fonts + CJK)
    float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd_);
    LoadAllFonts(dpiScale);

    // Init platform/renderer backends
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(d3dDevice_, d3dDeviceCtx_);

    // Override IME callback — we render candidates ourselves
    ImGui::GetPlatformIO().Platform_SetImeDataFn = CustomImeCallback;

    // Init MySQL database in background thread to avoid blocking startup
    std::thread([]() {
        if (!InitDatabase()) {
            Log(LogLevel::Error, "Database connection failed — continuing without auth");
        }
    }).detach();

    // Init texture manager (downloads game images from Steam CDN)
    InitTextures(d3dDevice_);

    // Init store data fetcher (async Steam store API)
    SetStoreApiKey("281B554C78FFC10E43A506B29CB19FEE");
    InitStoreData();

    // Init video player (Media Foundation + D3D11)
    InitVideoPlayer(d3dDevice_);

    // Detect Steam installation
    DetectSteam();
    if (!IsSteamInstalled()) {
        ShowToast(u8"\u672A\u68C0\u6D4B\u5230 Steam\uFF0C\u90E8\u5206\u529F\u80FD\u4E0D\u53EF\u7528\u3002\u8BF7\u5148\u5B89\u88C5 Steam \u5BA2\u6237\u7AEF\u3002",
                  ToastType::Warning, 8.0f);
        Log(LogLevel::Warn, "Steam not detected — some features disabled");
    } else if (!IsSteamRunning()) {
        ShowToast(u8"Steam \u672A\u8FD0\u884C\uFF0C\u542F\u52A8\u6E38\u620F\u7B49\u529F\u80FD\u9700\u8981\u5148\u6253\u5F00 Steam\u3002",
                  ToastType::Info, 5.0f);
        Log(LogLevel::Info, "Steam installed but not running");
    } else {
        Log(LogLevel::Info, "Steam detected and running");
    }

    // 初始化悬浮球（独立窗口）
    GetFloatingBall().Init(wc_.hInstance);
    GetFloatingBall().Show();

    Log(LogLevel::Info, "SteamForge initialized (DPI scale: %.2f)", dpiScale);
    return true;
}

int Application::Run() {
    MSG msg = {};
    bool running = true;

    while (running) {
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }
        if (!running) break;

        if (swapChainOccluded_ && swapChain_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        swapChainOccluded_ = false;

        if (resizeWidth_ != 0 && resizeHeight_ != 0) {
            CleanupRenderTarget();
            swapChain_->ResizeBuffers(0, resizeWidth_, resizeHeight_, DXGI_FORMAT_UNKNOWN, 0);
            resizeWidth_ = resizeHeight_ = 0;
            CreateRenderTarget();
        }

        RenderFrame();
    }

    return (int)msg.wParam;
}

void Application::RenderFrame() {
    // Process completed texture downloads
    UpdateTextures();

    // Update video player (transfer video frames to texture)
    UpdateVideoPlayer();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Get window size
    RECT rect;
    GetClientRect(hwnd_, &rect);
    float winW = (float)(rect.right - rect.left);
    float winH = (float)(rect.bottom - rect.top);

    // Show login screen if not authenticated
    if (!loginPanel_.IsAuthenticated()) {
        loginPanel_.Render(winW, winH);
    } else {
        // Process deferred logout at start of frame (before any rendering)
        if (pendingLogout_) {
            Log(LogLevel::Info, "Application: processing pending logout...");
            pendingLogout_ = false;

            // Clear sidebar first (before AuthLogout clears the library data)
            Log(LogLevel::Info, "Application: clearing sidebar...");
            sidebar_.Clear();

            // Now do the actual logout
            Log(LogLevel::Info, "Application: calling AuthLogout...");
            AuthLogout();

            // Set UI state
            Log(LogLevel::Info, "Application: setting authenticated to false...");
            loginPanel_.SetAuthenticated(false);

            Log(LogLevel::Info, "Application: logout complete, rendering login screen");
            // Render login screen this frame instead
            loginPanel_.Render(winW, winH);
            goto render_frame;
        }

        // Layout constants - responsive scaling based on window size
        float scale = std::max(0.8f, std::min(1.5f, winH / 720.0f));
        float headerH    = 48.0f * scale;   // Single header bar
        float statusBarH = 36.0f * scale;   // Status bar at bottom
        float sidebarW   = std::max(200.0f, std::min(320.0f, winW * 0.2f));  // 20% of width, clamped

        float contentY = headerH;
        float contentH = winH - headerH - statusBarH;

        // 1) Header
        Log(LogLevel::Debug, "Application: rendering header...");
        header_.Render(winW, headerH);

        // Handle header actions
        if (header_.WasProfileClicked()) {
            profilePanel_.Open();
        }
        if (header_.WasVipClicked()) {
            vipPanel_.Open();
        }
        if (header_.WasLogoutClicked()) {
            // Defer logout to next frame to avoid accessing cleared data mid-render
            pendingLogout_ = true;
        }

        // 2) Content — sidebar only on Library tab
        MainTab activeTab = header_.GetActiveTab();
        Log(LogLevel::Debug, "Application: rendering content, activeTab=%d", (int)activeTab);
        switch (activeTab) {
            case MainTab::Library:
                Log(LogLevel::Debug, "Application: rendering sidebar...");
                sidebar_.Render(0, headerH, sidebarW, winH - headerH - statusBarH);
                Log(LogLevel::Debug, "Application: sidebar done, rendering library content...");
                // Show download panel only if selected game is currently downloading (not completed)
                {
                    bool showDownloadPanel = false;
                    if (gameInstallPanel_.IsDownloading()) {
                        std::string downloadingAppId = gameInstallPanel_.GetDownloadingAppId();
                        const GameInfo* selectedGame = sidebar_.GetSelectedGame();
                        if (selectedGame) {
                            showDownloadPanel = (selectedGame->appId == downloadingAppId);
                        }
                    }
                    if (showDownloadPanel) {
                        gameInstallPanel_.RenderDownloadPanel(sidebarW, contentY, winW - sidebarW, contentH);
                    } else {
                        libraryContent_.Render(sidebarW, contentY, winW - sidebarW, contentH, sidebar_.GetGames(), sidebar_.GetSelectedGame());
                    }
                }
                Log(LogLevel::Debug, "Application: library content done");
                break;
            case MainTab::Store:
                storePage_.Render(0, contentY, winW, contentH);
                break;
            case MainTab::Community:
                communityPage_.Render(0, contentY, winW, contentH);
                break;
            case MainTab::Tools:
                // 工具箱页面 - 始终显示Hub
                toolsPanel_.Render(0, contentY, winW, contentH);
                break;
        }

        // 3) Handle in-app navigation actions
        {
            NavAction nav = ConsumeNavAction();
            if (nav.type != NavAction::None) {
                Log(LogLevel::Info, "ProcessNavAction: type=%d, appId=%d", (int)nav.type, nav.appId);
                auto openSteamUrl = [](const std::string& url) {
                    std::wstring w(url.begin(), url.end());
                    ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                };

                // Actions that require Steam
                bool needsSteam = (nav.type == NavAction::LaunchGame ||
                                   nav.type == NavAction::InstallGame ||
                                   nav.type == NavAction::UninstallGame);

                if (needsSteam && !IsSteamInstalled()) {
                    ShowToast(u8"\u8BE5\u529F\u80FD\u9700\u8981\u5B89\u88C5 Steam \u5BA2\u6237\u7AEF",
                              ToastType::Error, 4.0f);
                } else {

                switch (nav.type) {
                case NavAction::LaunchGame: {
                    // Check if game was installed by SteamForge
                    std::string sfPath = InstallRecord::Get().GetInstallPath(nav.appId);
                    std::wstring sfPathW = Utf8ToWideApp(sfPath);  // 转换为 wstring 支持中文路径
                    bool pathExists = PathExistsSafeApp(sfPath);
                    Log(LogLevel::Info, "LaunchGame: appId=%s, sfPath=%s, pathExists=%d",
                        nav.appId.c_str(), sfPath.c_str(), pathExists ? 1 : 0);

                    // 检查是否是 SteamForge 下载的游戏（有 .DepotDownloader 目录）
                    bool isSteamForgeDownload = false;
                    if (!sfPath.empty() && pathExists) {
                        std::string depotDir = sfPath + "\\.DepotDownloader";
                        if (PathExistsSafeApp(depotDir)) {
                            isSteamForgeDownload = true;
                            Log(LogLevel::Info, "LaunchGame: Found .DepotDownloader, this is a SteamForge download");
                        }
                    }

                    // 如果是 SteamForge 下载的游戏，检查是否已登录 Steam 账号
                    if (isSteamForgeDownload && !gameInstallPanel_.HasSavedCredentials()) {
                        // 没有保存的凭据，显示登录弹窗
                        Log(LogLevel::Info, "LaunchGame: No saved credentials, showing login dialog");
                        NavAction navCopy = nav;  // 复制 nav 用于回调
                        gameInstallPanel_.ShowLoginForLaunch([navCopy]() {
                            // 登录成功后重新触发启动游戏
                            Navigate(navCopy);
                        });
                        break;
                    }

                    if (!sfPath.empty() && pathExists) {
                        // SteamForge installed game - find and launch exe directly
                        Log(LogLevel::Info, "LaunchGame: SteamForge game at %s", sfPath.c_str());
                        std::wstring exePathW;  // Use wstring to preserve Unicode paths
                        uintmax_t bestSize = 0;
                        int bestScore = -1;  // 用于评分选择最佳exe

                        // Helper to check if exe should be skipped
                        auto shouldSkipExe = [](const std::string& fnameLower) {
                            // Skip common non-game executables
                            static const char* skipPatterns[] = {
                                "unins", "setup", "redist", "vcredist", "dxsetup", "directx",
                                "dotnet", "crashhandler", "crashreport", "reporter", "updater",
                                "ue4prereq", "ue3redist", "physx", "easyanticheat", "battleye",
                                "dxwebsetup", "oalinst", "7z", "winrar", "installer", "patch",
                                "config", "settings", "launcher_helper", "steamclient", "steam_api",
                                "cef", "subprocess", "helper", "browser", "webhelper", "cefprocess",
                                "dedicated_server", "nullrenderer", "server"  // 跳过专用服务器
                            };
                            for (const char* pat : skipPatterns) {
                                if (fnameLower.find(pat) != std::string::npos) return true;
                            }
                            return false;
                        };

                        // Helper to score an exe (higher = better candidate)
                        // 使用 wstring 避免中文路径崩溃
                        auto scoreExe = [&](const std::wstring& fnameLowerW, const std::wstring& fullPathW,
                                           uintmax_t fsize, int depth) -> int {
                            int score = 0;

                            // Prefer exe in root or first level
                            if (depth == 1) score += 100;
                            else if (depth == 2) score += 50;
                            else score += std::max(0, 30 - depth * 5);

                            // Prefer larger files (likely main game exe)
                            if (fsize > 100 * 1024 * 1024) score += 40;      // > 100MB
                            else if (fsize > 50 * 1024 * 1024) score += 30;  // > 50MB
                            else if (fsize > 10 * 1024 * 1024) score += 20;  // > 10MB
                            else if (fsize > 1 * 1024 * 1024) score += 10;   // > 1MB

                            // Prefer common game exe naming patterns
                            if (fnameLowerW.find(L"game") != std::wstring::npos) score += 25;
                            if (fnameLowerW.find(L"play") != std::wstring::npos) score += 20;
                            if (fnameLowerW.find(L"start") != std::wstring::npos) score += 15;
                            if (fnameLowerW.find(L"launch") != std::wstring::npos) score += 15;
                            if (fnameLowerW.find(L"win64") != std::wstring::npos) score += 10;
                            if (fnameLowerW.find(L"win32") != std::wstring::npos) score += 5;
                            if (fnameLowerW.find(L"-win64") != std::wstring::npos) score += 10;
                            if (fnameLowerW.find(L"shipping") != std::wstring::npos) score += 15;  // UE4 shipping build

                            // Check if in common game binary directories
                            std::wstring pathLowerW = fullPathW;
                            std::transform(pathLowerW.begin(), pathLowerW.end(), pathLowerW.begin(), ::towlower);
                            if (pathLowerW.find(L"\\bin\\") != std::wstring::npos ||
                                pathLowerW.find(L"\\binaries\\") != std::wstring::npos ||
                                pathLowerW.find(L"\\win64\\") != std::wstring::npos ||
                                pathLowerW.find(L"\\x64\\") != std::wstring::npos) {
                                score += 20;
                            }

                            // Penalize if in engine/tool directories
                            if (pathLowerW.find(L"\\engine\\") != std::wstring::npos) score -= 30;
                            if (pathLowerW.find(L"\\tools\\") != std::wstring::npos) score -= 20;
                            if (pathLowerW.find(L"\\editor\\") != std::wstring::npos) score -= 20;
                            if (pathLowerW.find(L"\\sdk\\") != std::wstring::npos) score -= 20;

                            return score;
                        };

                        // Recursively search for exe files (up to 5 levels deep)
                        try {
                            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                                     sfPathW, std::filesystem::directory_options::skip_permission_denied)) {
                                // Limit depth
                                auto relPath = std::filesystem::relative(entry.path(), sfPathW);
                                int depth = 0;
                                for (auto& p : relPath) { depth++; }
                                if (depth > 5) continue;

                                if (entry.is_regular_file()) {
                                    // 使用 wstring 避免中文路径崩溃
                                    std::wstring extW = entry.path().extension().wstring();
                                    std::transform(extW.begin(), extW.end(), extW.begin(), ::towlower);
                                    if (extW == L".exe") {
                                        std::wstring fnameW = entry.path().filename().wstring();
                                        std::wstring fnameLowerW = fnameW;
                                        std::transform(fnameLowerW.begin(), fnameLowerW.end(), fnameLowerW.begin(), ::towlower);

                                        // 转换为 UTF-8 用于 shouldSkipExe 检查
                                        std::string fnameLower = WideToUtf8(fnameLowerW.c_str());
                                        if (shouldSkipExe(fnameLower)) continue;

                                        // Get file size
                                        uintmax_t fsize = 0;
                                        try { fsize = entry.file_size(); } catch (...) {}

                                        // Skip very small exe (likely tools)
                                        if (fsize < 500 * 1024) continue;  // < 500KB

                                        // Score this exe - 使用 wstring 版本
                                        std::wstring fullPathW = entry.path().wstring();
                                        int score = scoreExe(fnameLowerW, fullPathW, fsize, depth);

                                        if (score > bestScore) {
                                            exePathW = entry.path().wstring();  // Use wstring to preserve Unicode
                                            bestSize = fsize;
                                            bestScore = score;
                                            // 转换为 UTF-8 用于日志
                                            std::string fnameUtf8 = WideToUtf8(fnameW.c_str());
                                            Log(LogLevel::Info, "LaunchGame: Candidate %s (score=%d, size=%lluMB)",
                                                fnameUtf8.c_str(), score, fsize / (1024 * 1024));
                                        }
                                    }
                                }
                            }
                        } catch (const std::exception& e) {
                            Log(LogLevel::Error, "LaunchGame: Error scanning directory: %s", e.what());
                        }

                        if (!exePathW.empty()) {
                            // Convert wstring to UTF-8 for logging
                            std::string exePathUtf8;
                            {
                                int len = WideCharToMultiByte(CP_UTF8, 0, exePathW.c_str(), -1, nullptr, 0, nullptr, nullptr);
                                if (len > 0) {
                                    exePathUtf8.resize(len - 1);
                                    WideCharToMultiByte(CP_UTF8, 0, exePathW.c_str(), -1, &exePathUtf8[0], len, nullptr, nullptr);
                                }
                            }
                            Log(LogLevel::Info, "LaunchGame: Selected %s (score=%d)", exePathUtf8.c_str(), bestScore);

                            std::wstring exeDirW = std::filesystem::path(exePathW).parent_path().wstring();
                            std::string exeDirUtf8;
                            {
                                int len = WideCharToMultiByte(CP_UTF8, 0, exeDirW.c_str(), -1, nullptr, 0, nullptr, nullptr);
                                if (len > 0) {
                                    exeDirUtf8.resize(len - 1);
                                    WideCharToMultiByte(CP_UTF8, 0, exeDirW.c_str(), -1, &exeDirUtf8[0], len, nullptr, nullptr);
                                }
                            }

                            // 检测游戏是否需要 Steam DRM，如果需要则应用 Steam Emulator
                            if (SteamEmu::Get().GameNeedsSteamDRM(sfPath)) {
                                Log(LogLevel::Info, "LaunchGame: Game requires Steam DRM, applying emulator");
                                if (!SteamEmu::Get().IsAvailable()) {
                                    // Emulator 还没安装，先安装
                                    ShowToast(u8"正在准备 Steam 模拟器...", ToastType::Info, 3.0f);
                                    SteamEmu::Get().EnsureInstalled([sfPath, nav](bool success, const std::string& error) {
                                        if (success) {
                                            SteamEmu::Get().ApplyToGame(sfPath, nav.appId, nav.gameName);
                                            ShowToast(u8"Steam 模拟器已就绪，请重新启动游戏", ToastType::Success, 3.0f);
                                        } else {
                                            ShowToast(u8"Steam 模拟器安装失败", ToastType::Error, 4.0f);
                                            Log(LogLevel::Error, "LaunchGame: Failed to install Steam emulator: %s", error.c_str());
                                        }
                                    });
                                    break;  // 等待安装完成后用户重新点击启动
                                }
                                // 应用 emulator
                                SteamEmu::Get().ApplyToGame(sfPath, nav.appId, nav.gameName);
                            }

                            // 记录游戏启动时间
                            InstallRecord::Get().RecordGameLaunch(nav.appId);

                            // 创建 steam_appid.txt（游戏需要这个文件来识别 AppID）
                            std::string appidFile = sfPath + "\\steam_appid.txt";
                            std::ofstream ofs(appidFile);
                            if (ofs.is_open()) {
                                ofs << nav.appId;
                                ofs.close();
                            }
                            if (exeDirUtf8 != sfPath) {
                                std::string appidFile2 = exeDirUtf8 + "\\steam_appid.txt";
                                std::ofstream ofs2(appidFile2);
                                if (ofs2.is_open()) {
                                    ofs2 << nav.appId;
                                    ofs2.close();
                                }
                            }

                            // 使用 CreateProcess 直接启动游戏
                            STARTUPINFOW si = { sizeof(STARTUPINFOW) };
                            PROCESS_INFORMATION pi = {};
                            const std::wstring& wExe = exePathW;
                            const std::wstring& wDir = exeDirW;

                            // Debug: log the converted paths
                            Log(LogLevel::Info, "LaunchGame: exePath=%s, exeDir=%s", exePathUtf8.c_str(), exeDirUtf8.c_str());
                            Log(LogLevel::Info, "LaunchGame: wExe.length=%zu, wDir.length=%zu", wExe.length(), wDir.length());

                            DWORD createFlags = 0;
                            if (CreateProcessW(wExe.c_str(), nullptr, nullptr, nullptr, FALSE,
                                               createFlags, nullptr, wDir.c_str(), &si, &pi)) {
                                GameProcessManager::Get().RegisterProcess(nav.appId, pi.dwProcessId);
                                CloseHandle(pi.hProcess);
                                CloseHandle(pi.hThread);
                                Log(LogLevel::Info, "LaunchGame: Started process %lu", pi.dwProcessId);
                            } else {
                                DWORD err = GetLastError();
                                Log(LogLevel::Warn, "LaunchGame: CreateProcess failed (error=%lu), trying ShellExecute", err);

                                HINSTANCE result = ShellExecuteW(nullptr, L"open", wExe.c_str(),
                                                                  nullptr, wDir.c_str(), SW_SHOWNORMAL);
                                if ((intptr_t)result <= 32) {
                                    Log(LogLevel::Warn, "LaunchGame: ShellExecute failed, trying runas");
                                    result = ShellExecuteW(nullptr, L"runas", wExe.c_str(),
                                                           nullptr, wDir.c_str(), SW_SHOWNORMAL);
                                    if ((intptr_t)result <= 32) {
                                        Log(LogLevel::Error, "LaunchGame: All launch methods failed");
                                        ShowToast(u8"启动游戏失败，请尝试手动运行", ToastType::Error, 4.0f);
                                    }
                                }
                            }
                        } else {
                            Log(LogLevel::Error, "LaunchGame: No exe found in %s", sfPath.c_str());
                            ShowToast(u8"找不到游戏可执行文件", ToastType::Error, 3.0f);
                        }
                    } else {
                        // Steam installed game - use steam.exe -applaunch
                        // 记录游戏启动时间（Steam 游戏也要记录）
                        InstallRecord::Get().RecordGameLaunch(nav.appId);

                        const auto& st = GetSteamStatus();
                        if (!st.steamExe.empty()) {
                            std::wstring exe = Utf8ToWide(st.steamExe);
                            std::wstring args = L"-applaunch " + std::wstring(nav.appId.begin(), nav.appId.end());
                            ShellExecuteW(nullptr, L"open", exe.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
                        } else {
                            openSteamUrl("steam://rungameid/" + nav.appId);
                        }
                    }
                    break;
                }
                case NavAction::StopGame: {
                    // 停止游戏进程
                    if (GameProcessManager::Get().IsGameRunning(nav.appId)) {
                        if (GameProcessManager::Get().StopGame(nav.appId)) {
                            ShowToast(u8"游戏已停止", ToastType::Info, 2.0f);
                            Log(LogLevel::Info, "StopGame: Stopped game %s", nav.appId.c_str());
                        } else {
                            ShowToast(u8"无法停止游戏", ToastType::Error, 3.0f);
                            Log(LogLevel::Error, "StopGame: Failed to stop game %s", nav.appId.c_str());
                        }
                    }
                    break;
                }
                case NavAction::InstallGame: {
                    // Open in-app install dialog with path selection
                    std::string gameName = nav.gameName;  // Use name from NavAction first
                    float gameSize = nav.gameSize;

                    // If not provided in NavAction, try to find from sidebar
                    if (gameName.empty() || gameSize <= 0) {
                        for (int i = 0; i < (int)sidebar_.GetGames().size(); i++) {
                            if (sidebar_.GetGames()[i].appId == nav.appId) {
                                if (gameName.empty()) gameName = sidebar_.GetGames()[i].name;
                                if (gameSize <= 0) gameSize = sidebar_.GetGames()[i].size;
                                break;
                            }
                        }
                    }
                    if (gameName.empty()) gameName = "App " + nav.appId;
                    gameInstallPanel_.Open(nav.appId, gameName, gameSize);
                    break;
                }
                case NavAction::UninstallGame: {
                    // Check if game was installed by SteamForge
                    std::string sfPath = InstallRecord::Get().GetInstallPath(nav.appId);
                    if (!sfPath.empty()) {
                        // SteamForge installed game - use our uninstall panel
                        std::string gameName;
                        for (int i = 0; i < (int)sidebar_.GetGames().size(); i++) {
                            if (sidebar_.GetGames()[i].appId == nav.appId) {
                                gameName = sidebar_.GetGames()[i].name;
                                break;
                            }
                        }
                        if (gameName.empty()) gameName = "App " + nav.appId;
                        gameUninstallPanel_.Open(nav.appId, gameName);
                    } else {
                        // Steam installed game - use Steam's uninstall
                        openSteamUrl("steam://uninstall/" + nav.appId);
                    }
                    break;
                }
                case NavAction::ValidateGame:
                    // Open in-app game verify panel with progressive scanning
                    for (int i = 0; i < (int)sidebar_.GetGames().size(); i++) {
                        if (sidebar_.GetGames()[i].appId == nav.appId) {
                            gameVerifyPanel_.Open(sidebar_.GetGames()[i]);
                            break;
                        }
                    }
                    break;
                case NavAction::ViewStorePage:
                    // Switch to Store tab and open game detail page in-app
                    header_.SetActiveTab(MainTab::Store);
                    storePage_.SelectGame(nav.appId);
                    break;
                case NavAction::ViewProperties:
                    // Open game properties popup
                    for (int i = 0; i < (int)sidebar_.GetGames().size(); i++) {
                        if (sidebar_.GetGames()[i].appId == nav.appId) {
                            gamePropsPanel_.Open(sidebar_.GetGames()[i]);
                            break;
                        }
                    }
                    break;
                case NavAction::BrowseLocalFiles: {
                    // Find game install directory and open in Explorer
                    std::string dir;
                    // Read Steam install path from registry
                    auto getSteamPath = []() -> std::string {
                        HKEY hKey; char buf[512] = {}; DWORD sz = sizeof(buf);
                        if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                            if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
                                RegCloseKey(hKey);
                                std::string p(buf); for (auto& c : p) if (c == '/') c = '\\'; return p;
                            }
                            RegCloseKey(hKey);
                        }
                        return "";
                    };
                    std::string steamPath = getSteamPath();
                    if (!steamPath.empty()) {
                        // Check all library folders for this game's manifest
                        std::vector<std::string> libs = { steamPath };
                        std::string vdfPath = steamPath + "\\steamapps\\libraryfolders.vdf";
                        std::ifstream vf(vdfPath);
                        if (vf.is_open()) {
                            std::string vc((std::istreambuf_iterator<char>(vf)), std::istreambuf_iterator<char>());
                            vf.close();
                            size_t sp = 0;
                            while (true) {
                                size_t p = vc.find("\"path\"", sp);
                                if (p == std::string::npos) break;
                                p = vc.find('"', p + 6); if (p == std::string::npos) break; p++;
                                size_t e = vc.find('"', p); if (e == std::string::npos) break;
                                std::string lp;
                                for (size_t i = p; i < e; i++) {
                                    if (vc[i] == '\\' && i+1 < e && vc[i+1] == '\\') { lp += '\\'; i++; }
                                    else lp += vc[i];
                                }
                                libs.push_back(lp);
                                sp = e + 1;
                            }
                        }
                        for (auto& lib : libs) {
                            std::string mf = lib + "\\steamapps\\appmanifest_" + nav.appId + ".acf";
                            std::ifstream af(mf);
                            if (!af.is_open()) continue;
                            std::string ac((std::istreambuf_iterator<char>(af)), std::istreambuf_iterator<char>());
                            af.close();
                            std::string key = "\"installdir\"";
                            size_t pos = ac.find(key);
                            if (pos == std::string::npos) continue;
                            pos = ac.find('"', pos + key.size()); if (pos == std::string::npos) continue; pos++;
                            size_t end = ac.find('"', pos); if (end == std::string::npos) continue;
                            dir = lib + "\\steamapps\\common\\" + ac.substr(pos, end - pos);
                            break;
                        }
                    }
                    if (!dir.empty()) {
                        std::wstring wd(dir.begin(), dir.end());
                        ShellExecuteW(nullptr, L"explore", wd.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    } else {
                        openSteamUrl("steam://nav/games/details/" + nav.appId);
                    }
                    break;
                }
                case NavAction::ViewLibraryDownload:
                    // Switch to Library tab and select the downloading game
                    header_.SetActiveTab(MainTab::Library);
                    if (!nav.appId.empty()) {
                        sidebar_.SelectGameByAppId(nav.appId);
                    }
                    break;
                case NavAction::ViewLibraryGame:
                    // Switch to Library tab and select the specific game
                    header_.SetActiveTab(MainTab::Library);
                    if (!nav.appId.empty()) {
                        sidebar_.SelectGameByAppId(nav.appId);
                    }
                    break;
                default: break;
                }
                } // else (Steam installed)
            }
        }

        // 4) Status bar at bottom
        statusBar_.Render(winH - statusBarH, winW, statusBarH);

        // 5) Profile popup (modal overlay)
        profilePanel_.Render(winW, winH);

        // 6) VIP popup (modal overlay)
        vipPanel_.Render(winW, winH);

        // 7) Game properties popup (modal overlay)
        gamePropsPanel_.Render(winW, winH);

        // 8) Game verify popup (modal overlay)
        gameVerifyPanel_.Render(winW, winH);

        // 9) Game install dialog (modal overlay)
        gameInstallPanel_.Render(winW, winH);

        // 10) Game uninstall dialog (modal overlay)
        gameUninstallPanel_.Render(winW, winH);

        // 11) Check-in popup (modal overlay)
        checkInPanel_.Render(winW, winH);

        // 12) Lottery popup (modal overlay)
        lotteryPanel_.Render(winW, winH);

        // 13) Task popup (modal overlay)
        taskPanel_.Render(winW, winH);

        // 14) Shop popup (modal overlay)
        shopPanel_.Render(winW, winH);

        // 15) Floating ball (独立窗口，在主循环中更新)
        GetFloatingBall().Update();

        // 16) Memory modifier (独立窗口)
        memoryModifierPanel_.Render();
    }

    // Toast notifications (on top of panels)
    {
        RECT rect2;
        GetClientRect(hwnd_, &rect2);
        RenderToasts((float)(rect2.right - rect2.left), (float)(rect2.bottom - rect2.top));
        RenderGameCardNotifications((float)(rect2.right - rect2.left), (float)(rect2.bottom - rect2.top));
    }

    // Render our own IME candidate popup (on top of everything)
    RenderImeCandidatePopup();

render_frame:
    // Render
    ImGui::Render();
    const float clearColor[4] = { 0.055f, 0.078f, 0.102f, 1.00f };
    d3dDeviceCtx_->OMSetRenderTargets(1, &mainRTV_, nullptr);
    d3dDeviceCtx_->ClearRenderTargetView(mainRTV_, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT hr = swapChain_->Present(1, 0);
    swapChainOccluded_ = (hr == DXGI_STATUS_OCCLUDED);
}

void Application::Shutdown() {
    // 关闭悬浮球
    GetFloatingBall().Shutdown();

    // 停止所有通过平台启动的游戏
    GameProcessManager::Get().StopAllGames();

    // Stop OAuth callback server first (it runs in a detached thread)
    StopOAuthCallbackServer();

    ShutdownVideoPlayer();
    ShutdownStoreData();
    ShutdownTextures();
    ShutdownDatabase();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClassW(wc_.lpszClassName, wc_.hInstance);
}

bool Application::CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount        = 2;
    sd.BufferDesc.Width   = 0;
    sd.BufferDesc.Height  = 0;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow       = hWnd;
    sd.SampleDesc.Count   = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevels, 2, D3D11_SDK_VERSION,
        &sd, &swapChain_, &d3dDevice_, &featureLevel, &d3dDeviceCtx_
    );

    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevels, 2, D3D11_SDK_VERSION,
            &sd, &swapChain_, &d3dDevice_, &featureLevel, &d3dDeviceCtx_
        );
    }

    if (FAILED(hr)) {
        Log(LogLevel::Error, "D3D11CreateDeviceAndSwapChain failed: 0x%08X", hr);
        return false;
    }

    CreateRenderTarget();
    return true;
}

void Application::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        d3dDevice_->CreateRenderTargetView(pBackBuffer, nullptr, &mainRTV_);
        pBackBuffer->Release();
    }
}

void Application::CleanupRenderTarget() {
    if (mainRTV_) { mainRTV_->Release(); mainRTV_ = nullptr; }
}

void Application::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (swapChain_)    { swapChain_->Release();    swapChain_    = nullptr; }
    if (d3dDeviceCtx_) { d3dDeviceCtx_->Release(); d3dDeviceCtx_ = nullptr; }
    if (d3dDevice_)    { d3dDevice_->Release();     d3dDevice_    = nullptr; }
}

LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_app && wParam != SIZE_MINIMIZED) {
            g_app->resizeWidth_  = LOWORD(lParam);
            g_app->resizeHeight_ = HIWORD(lParam);
        }
        return 0;
    case WM_IME_SETCONTEXT:
        // Hide system candidate window — we render our own in ImGui
        lParam &= ~0x0F; // Clear ISC_SHOWUICANDIDATEWINDOW bits
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    case WM_IME_STARTCOMPOSITION:
        if (g_imeWantVisible) {
            HIMC himc = ::ImmGetContext(hWnd);
            if (himc) {
                COMPOSITIONFORM cf = {};
                cf.dwStyle = CFS_FORCE_POSITION;
                cf.ptCurrentPos = g_imePos;
                ::ImmSetCompositionWindow(himc, &cf);
                ::ImmReleaseContext(hWnd, himc);
            }
        }
        break;
    case WM_IME_COMPOSITION:
        // Update composition string on every change
        if (g_imeWantVisible) {
            HIMC himc = ::ImmGetContext(hWnd);
            if (himc) {
                if (lParam & GCS_COMPSTR) {
                    LONG compLen = ::ImmGetCompositionStringW(himc, GCS_COMPSTR, NULL, 0);
                    if (compLen > 0) {
                        std::vector<wchar_t> buf(compLen / sizeof(wchar_t) + 1, 0);
                        ::ImmGetCompositionStringW(himc, GCS_COMPSTR, buf.data(), compLen);
                        g_imeCand.compositionStr = WideToUtf8(buf.data());
                    } else {
                        g_imeCand.compositionStr.clear();
                    }
                }
                ::ImmReleaseContext(hWnd, himc);
            }
        }
        break; // Let DefWindowProc handle it
    case WM_IME_NOTIFY:
        switch (wParam) {
        case IMN_OPENCANDIDATE:
        case IMN_CHANGECANDIDATE:
            ReadImeCandidates(hWnd);
            break;
        case IMN_CLOSECANDIDATE:
            g_imeCand.visible = false;
            g_imeCand.items.clear();
            g_imeCand.compositionStr.clear();
            break;
        }
        break; // Let DefWindowProc handle it
    case WM_IME_ENDCOMPOSITION:
        g_imeCand.visible = false;
        g_imeCand.items.clear();
        g_imeCand.compositionStr.clear();
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace sf
