#include "ui/panels/game_install.h"
#include "ui/iconfonts.h"
#include "ui/toast.h"
#include "core/steam_detect.h"
#include "core/texture_manager.h"
#include "core/depot_downloader.h"
#include "core/install_record.h"
#include "core/user_library.h"
#include "core/navigation.h"
#include "core/steam_store.h"
#include "core/steamcmd.h"
#include "utils/logger.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <wincodec.h>
#include <urlmon.h>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace sf {

// 声明全局函数（定义在 application.cpp）
void NotifyGameInstalled(const std::string& appId);
void NotifyGameAddedToLibrary(const std::string& appId, const std::string& name);

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

// ── Helpers ──

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

// Sanitize filename by removing/replacing illegal characters for Windows
static std::string SanitizeFileName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        // Windows illegal characters: \ / : * ? " < > |
        if (c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            // Replace with underscore or skip
            result += '_';
        } else if (c == '\\' || c == '/') {
            // Skip path separators
            result += '_';
        } else {
            result += c;
        }
    }
    // Trim trailing spaces and dots (Windows doesn't allow them at end of folder names)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }
    return result;
}

static std::string FormatBytes(int64_t bytes) {
    char buf[64];
    if (bytes >= (int64_t)1024 * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%lld B", (long long)bytes);
    return buf;
}

static std::string FormatSpeed(float bytesPerSec) {
    if (bytesPerSec >= 1024 * 1024)
        return FormatBytes((int64_t)bytesPerSec) + "/s";
    else if (bytesPerSec >= 1024) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.1f KB/s", bytesPerSec / 1024.0);
        return buf;
    }
    return "0 B/s";
}

// Chinese UI string constants (avoid u8 literal issues with MSVC 2019)
namespace ui_str {
    static const char* CALCULATING   = u8"计算中...";
    static const char* HOUR_MIN      = u8"%d小时 %d分钟";
    static const char* MIN_SEC       = u8"%d分钟 %d秒";
    static const char* SEC           = u8"%d秒";
    static const char* SELECT_PATH   = u8"选择安装位置";
    static const char* INSTALL_LOC   = u8"安装位置:";
    static const char* FREE_SPACE    = u8" 可用";
    static const char* INSTALL_BTN   = u8"安装";
    static const char* CANCEL_BTN    = u8"取消";
    static const char* START_INSTALL = u8" 开始安装...";
    static const char* DOWNLOADING   = u8"正在下载...";
    static const char* COMPLETED     = u8"安装完成";
    static const char* FAILED        = u8"安装失败";
    static const char* DL_SPEED      = u8"下载速度";
    static const char* PEAK_SPEED    = u8"峰值速度";
    static const char* TIME_LEFT     = u8"剩余时间";
    static const char* TIME_USED     = u8"已用时间";
    static const char* SPEED_GRAPH   = u8"下载速度图表";
    static const char* DISK_WRITE    = u8"磁盘写入";
    static const char* LAUNCH_GAME   = u8"启动游戏";
    static const char* NEED_STEAM    = u8"需要Steam";
    static const char* DESKTOP_SC    = u8"创建桌面快捷方式";
    static const char* STARTMENU_SC  = u8"创建开始菜单快捷方式";
}

static std::string FormatEta(float seconds) {
    if (seconds <= 0 || seconds > 360000) return ui_str::CALCULATING;
    int s = (int)seconds;
    int h = s / 3600; s %= 3600;
    int m = s / 60; s %= 60;
    char buf[64];
    if (h > 0) snprintf(buf, sizeof(buf), ui_str::HOUR_MIN, h, m);
    else if (m > 0) snprintf(buf, sizeof(buf), ui_str::MIN_SEC, m, s);
    else snprintf(buf, sizeof(buf), ui_str::SEC, s);
    return buf;
}

// Read a key from ACF content
static std::string AcfGet(const std::string& content, const std::string& key) {
    std::string sk = "\"" + key + "\"";
    size_t p = content.find(sk);
    if (p == std::string::npos) return "";
    p = content.find('"', p + sk.size());
    if (p == std::string::npos) return "";
    p++;
    size_t e = content.find('"', p);
    if (e == std::string::npos) return "";
    return content.substr(p, e - p);
}

// Get all Steam library paths
static std::vector<std::string> GetLibPaths() {
    std::vector<std::string> paths;
    auto& st = GetSteamStatus();
    if (st.steamPath.empty()) return paths;
    paths.push_back(st.steamPath);

    std::string vdf = st.steamPath + "\\steamapps\\libraryfolders.vdf";
    std::ifstream f(vdf);
    if (!f.is_open()) return paths;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    size_t s = 0;
    while (true) {
        size_t p = content.find("\"path\"", s);
        if (p == std::string::npos) break;
        p = content.find('"', p + 6); if (p == std::string::npos) break; p++;
        size_t e = content.find('"', p); if (e == std::string::npos) break;
        std::string lp;
        for (size_t i = p; i < e; i++) {
            if (content[i] == '\\' && i + 1 < e && content[i + 1] == '\\') { lp += '\\'; i++; }
            else lp += content[i];
        }
        // Deduplicate (case-insensitive for Windows paths)
        bool dup = false;
        for (auto& ep : paths) if (_stricmp(ep.c_str(), lp.c_str()) == 0) { dup = true; break; }
        if (!dup) paths.push_back(lp);
        s = e + 1;
    }
    return paths;
}

// ── Download game icon from Steam CDN and convert to .ico ──
static std::wstring DownloadGameIcon(const std::string& appId) {
    // Get cache directory
    wchar_t localAppData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData)))
        return L"";

    std::wstring iconDir = std::wstring(localAppData) + L"\\SteamForge\\icons";
    std::filesystem::create_directories(iconDir);

    std::wstring appIdW(appId.begin(), appId.end());
    std::wstring icoPath = iconDir + L"\\" + appIdW + L".ico";

    // Check if icon already exists
    if (std::filesystem::exists(icoPath))
        return icoPath;

    // Download icon from Steam CDN (try multiple URLs)
    std::wstring tempJpg = iconDir + L"\\" + appIdW + L"_temp.jpg";
    const wchar_t* urlPatterns[] = {
        L"https://cdn.akamai.steamstatic.com/steam/apps/%s/capsule_231x87.jpg",
        L"https://cdn.akamai.steamstatic.com/steam/apps/%s/header.jpg",
        nullptr
    };

    bool downloaded = false;
    for (int i = 0; urlPatterns[i] && !downloaded; i++) {
        wchar_t url[512];
        swprintf(url, 512, urlPatterns[i], appIdW.c_str());
        if (URLDownloadToFileW(nullptr, url, tempJpg.c_str(), 0, nullptr) == S_OK) {
            // Verify it's a valid image
            FILE* f = _wfopen(tempJpg.c_str(), L"rb");
            if (f) {
                unsigned char magic[3] = {};
                fread(magic, 1, 3, f);
                fclose(f);
                if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF)
                    downloaded = true;
            }
        }
    }

    if (!downloaded) {
        DeleteFileW(tempJpg.c_str());
        return L"";
    }

    // Convert JPG to ICO using WIC
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IWICImagingFactory* pFactory = nullptr;
    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConverter = nullptr;
    IWICBitmapScaler* pScaler = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        DeleteFileW(tempJpg.c_str());
        CoUninitialize();
        return L"";
    }

    hr = pFactory->CreateDecoderFromFilename(tempJpg.c_str(), nullptr, GENERIC_READ,
                                              WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr) || !pDecoder) {
        pFactory->Release();
        DeleteFileW(tempJpg.c_str());
        CoUninitialize();
        return L"";
    }

    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr) || !pFrame) {
        pDecoder->Release();
        pFactory->Release();
        DeleteFileW(tempJpg.c_str());
        CoUninitialize();
        return L"";
    }

    // Create scaler to resize to 256x256 (standard icon size)
    hr = pFactory->CreateBitmapScaler(&pScaler);
    if (FAILED(hr) || !pScaler) {
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        DeleteFileW(tempJpg.c_str());
        CoUninitialize();
        return L"";
    }

    hr = pScaler->Initialize(pFrame, 256, 256, WICBitmapInterpolationModeHighQualityCubic);
    if (FAILED(hr)) {
        pScaler->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        DeleteFileW(tempJpg.c_str());
        CoUninitialize();
        return L"";
    }

    // Convert to 32bpp BGRA
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr) || !pConverter) {
        pScaler->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        DeleteFileW(tempJpg.c_str());
        CoUninitialize();
        return L"";
    }

    hr = pConverter->Initialize(pScaler, GUID_WICPixelFormat32bppBGRA,
                                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        pConverter->Release();
        pScaler->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        DeleteFileW(tempJpg.c_str());
        CoUninitialize();
        return L"";
    }

    // Get pixel data
    UINT width = 256, height = 256;
    UINT stride = width * 4;
    UINT bufferSize = stride * height;
    std::vector<BYTE> pixels(bufferSize);
    hr = pConverter->CopyPixels(nullptr, stride, bufferSize, pixels.data());

    pConverter->Release();
    pScaler->Release();
    pFrame->Release();
    pDecoder->Release();
    pFactory->Release();
    DeleteFileW(tempJpg.c_str());

    if (FAILED(hr)) {
        CoUninitialize();
        return L"";
    }

    // Write ICO file manually
    FILE* ico = _wfopen(icoPath.c_str(), L"wb");
    if (!ico) {
        CoUninitialize();
        return L"";
    }

    // ICO header
    WORD reserved = 0, type = 1, count = 1;
    fwrite(&reserved, 2, 1, ico);
    fwrite(&type, 2, 1, ico);
    fwrite(&count, 2, 1, ico);

    // ICO directory entry
    BYTE bWidth = 0;  // 0 means 256
    BYTE bHeight = 0;
    BYTE bColorCount = 0;
    BYTE bReserved = 0;
    WORD wPlanes = 1;
    WORD wBitCount = 32;
    DWORD dwBytesInRes = 40 + bufferSize;  // BITMAPINFOHEADER + pixel data
    DWORD dwImageOffset = 22;  // 6 (header) + 16 (directory entry)

    fwrite(&bWidth, 1, 1, ico);
    fwrite(&bHeight, 1, 1, ico);
    fwrite(&bColorCount, 1, 1, ico);
    fwrite(&bReserved, 1, 1, ico);
    fwrite(&wPlanes, 2, 1, ico);
    fwrite(&wBitCount, 2, 1, ico);
    fwrite(&dwBytesInRes, 4, 1, ico);
    fwrite(&dwImageOffset, 4, 1, ico);

    // BITMAPINFOHEADER
    DWORD biSize = 40;
    LONG biWidth = 256;
    LONG biHeight = 512;  // Double height for ICO format (includes AND mask)
    WORD biPlanes = 1;
    WORD biBitCount = 32;
    DWORD biCompression = 0;
    DWORD biSizeImage = bufferSize;
    LONG biXPelsPerMeter = 0;
    LONG biYPelsPerMeter = 0;
    DWORD biClrUsed = 0;
    DWORD biClrImportant = 0;

    fwrite(&biSize, 4, 1, ico);
    fwrite(&biWidth, 4, 1, ico);
    fwrite(&biHeight, 4, 1, ico);
    fwrite(&biPlanes, 2, 1, ico);
    fwrite(&biBitCount, 2, 1, ico);
    fwrite(&biCompression, 4, 1, ico);
    fwrite(&biSizeImage, 4, 1, ico);
    fwrite(&biXPelsPerMeter, 4, 1, ico);
    fwrite(&biYPelsPerMeter, 4, 1, ico);
    fwrite(&biClrUsed, 4, 1, ico);
    fwrite(&biClrImportant, 4, 1, ico);

    // Write pixel data (bottom-up for BMP/ICO format)
    for (int y = height - 1; y >= 0; y--) {
        fwrite(pixels.data() + y * stride, 1, stride, ico);
    }

    fclose(ico);
    CoUninitialize();

    return icoPath;
}

// ── Create Windows shortcut (.lnk) for a Steam game ──
static void CreateGameShortcut(const std::string& appId, const std::string& gameName,
                               bool desktop, bool startMenu) {
    if (!desktop && !startMenu) return;

    // Download game icon first
    std::wstring iconPath = DownloadGameIcon(appId);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    auto createLnk = [&](const std::wstring& folder) {
        IShellLinkW* psl = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IShellLinkW, (void**)&psl);
        if (FAILED(hr) || !psl) return;

        std::wstring steamExe;
        auto& st = GetSteamStatus();
        if (!st.steamExe.empty())
            steamExe.assign(st.steamExe.begin(), st.steamExe.end());
        else
            steamExe = L"steam.exe";

        std::wstring args = L"steam://rungameid/" + std::wstring(appId.begin(), appId.end());
        psl->SetPath(steamExe.c_str());
        psl->SetArguments(args.c_str());

        std::wstring wName = Utf8ToWide(gameName);
        psl->SetDescription(wName.c_str());

        // Set game icon if available
        if (!iconPath.empty()) {
            psl->SetIconLocation(iconPath.c_str(), 0);
        }

        IPersistFile* ppf = nullptr;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr) && ppf) {
            std::wstring lnkPath = folder + L"\\" + wName + L".lnk";
            ppf->Save(lnkPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    };

    if (desktop) {
        wchar_t path[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path) == S_OK)
            createLnk(path);
    }
    if (startMenu) {
        wchar_t path[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, path) == S_OK)
            createLnk(path);
    }

    CoUninitialize();
}

// ── GameInstallPanel ──

GameInstallPanel::~GameInstallPanel() {
    DepotDownloader::Get().CancelDownload();
}

void GameInstallPanel::CollectInstallLocations() {
    locations_.clear();
    auto libs = GetLibPaths();
    for (const auto& lib : libs) {
        InstallLocation loc;
        loc.path = lib;

        // Create label from drive letter
        if (lib.size() >= 2 && lib[1] == ':') {
            loc.label = lib.substr(0, 2);
            // Extract folder name after drive
            size_t lastSlash = lib.find_last_of('\\');
            if (lastSlash != std::string::npos && lastSlash > 2)
                loc.label += " (" + lib.substr(lastSlash + 1) + ")";
            else
                loc.label += " (Steam)";
        } else {
            loc.label = lib;
        }

        // Get free disk space
        ULARGE_INTEGER freeBytes = {}, totalBytes = {};
        std::wstring wpath = Utf8ToWide(lib);
        if (GetDiskFreeSpaceExW(wpath.c_str(), &freeBytes, &totalBytes, nullptr)) {
            loc.freeBytes = (int64_t)freeBytes.QuadPart;
            loc.totalBytes = (int64_t)totalBytes.QuadPart;
        }
        locations_.push_back(std::move(loc));
    }

    if (locations_.empty()) {
        // Fallback: use Steam path directly
        auto& st = GetSteamStatus();
        if (!st.steamPath.empty()) {
            InstallLocation loc;
            loc.path = st.steamPath;
            loc.label = "Steam";
            locations_.push_back(std::move(loc));
        }
    }
    selectedLocation_ = 0;
}

void GameInstallPanel::Open(const std::string& appId, const std::string& gameName, float gameSize) {
    pendingAppId_ = appId;
    pendingGameName_ = gameName;
    pendingGameSize_ = gameSize;

    // Always request Store data - we need it for game size and header image
    RequestStoreData(appId);

    // Request game cover texture for dialog banner (CDN fallback)
    RequestGameTexture(appId);

    CollectInstallLocations();
    showDialog_ = true;
    dialogAnim_ = 0.0f;
}

void GameInstallPanel::StartInstall() {
    showDialog_ = false;

    // 检查是否已有保存的凭据
    LoadSavedCredentials();

    // 如果没有保存的账号，显示登录对话框
    if (username_[0] == '\0') {
        loginState_ = LoginState::ShowDialog;
        return;
    }

    // 有保存的账号，直接开始下载
    DoStartDownload();
}

// Helper to get current time in seconds (thread-safe, for speed calculation)
static double GetTimeSeconds() {
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

void GameInstallPanel::DoStartDownload() {
    // Check if DepotDownloader is available
    auto& dd = DepotDownloader::Get();
    if (!dd.IsAvailable()) {
        // Need to download DepotDownloader first
        ShowToast(u8"正在下载 DepotDownloader...", ToastType::Info, 3.0f);
        dd.EnsureInstalled([this](bool success, const std::string& error) {
            if (success) {
                // Now start the actual install
                DoStartDownload();
            } else {
                ShowToast(u8"DepotDownloader 下载失败: " + error, ToastType::Error, 5.0f);
            }
        });
        return;
    }

    // Check if this is a free game/demo that needs license request first
    const SteamStoreData* storeData = GetStoreData(pendingAppId_);
    bool isFreeGame = false;
    if (storeData && storeData->loaded) {
        isFreeGame = storeData->isFree || storeData->type == "demo";
    }

    // Get credentials
    std::string user = username_[0] ? username_ : "";
    std::string pass = password_[0] ? password_ : "";

    // For free games, request license via SteamCMD first
    if (isFreeGame && !user.empty() && !pass.empty()) {
        auto& cmd = SteamCMD::Get();
        if (!cmd.IsAvailable()) {
            // Need to install SteamCMD first
            ShowToast(u8"正在安装 SteamCMD...", ToastType::Info, 3.0f);
            cmd.EnsureInstalled([this](bool success, const std::string& error) {
                if (success) {
                    DoStartDownload();
                } else {
                    ShowToast(u8"SteamCMD 安装失败: " + error, ToastType::Error, 5.0f);
                }
            });
            return;
        }

        // Request free license
        ShowToast(u8"正在请求游戏许可证...", ToastType::Info, 3.0f);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            download_ = {};
            download_.appId = pendingAppId_;
            download_.gameName = pendingGameName_;
            download_.active = true;
            download_.statusText = u8"请求许可证中...";
        }

        std::string appIdCopy = pendingAppId_;
        std::string gameNameCopy = pendingGameName_;
        cmd.RequestFreeLicense(pendingAppId_, user, pass,
            [this, user, pass, appIdCopy, gameNameCopy](bool success, const std::string& error) {
                if (success) {
                    ShowToast(u8"许可证获取成功，开始下载...", ToastType::Success, 2.0f);
                    DoActualDownload(user, pass);
                } else if (error == "LICENSE_REQUIRED") {
                    // License request failed - likely region restricted or requires Steam activation
                    ShowToast(u8"此游戏可能在您所在地区不可用", ToastType::Warning, 5.0f);

                    // Update download state to show error
                    {
                        std::lock_guard<std::mutex> lock(mtx_);
                        download_.active = false;
                        download_.failed = true;
                        download_.errorMsg = u8"此游戏可能在您所在地区不可用，或需要先在 Steam 中激活";
                    }
                } else {
                    // Other error, try download anyway (user might already own it)
                    DoActualDownload(user, pass);
                }
            });
        return;
    }

    // Not a free game or no credentials, proceed directly
    DoActualDownload(user, pass);
}

void GameInstallPanel::DoActualDownload(const std::string& user, const std::string& pass) {
    // 立即将游戏加入用户库（确保游戏出现在侧边栏列表中）
    UserLibrary::Get().AddGame(pendingAppId_, pendingGameName_, "game", "downloading");
    NotifyGameAddedToLibrary(pendingAppId_, pendingGameName_);

    // 跳转到库的下载界面
    Navigate(NavAction::ViewLibraryDownload, pendingAppId_);

    // 确定安装目录 (sanitize game name to remove illegal characters)
    std::string safeName = SanitizeFileName(pendingGameName_);
    std::string installDir;
    if (!locations_.empty() && selectedLocation_ < (int)locations_.size()) {
        installDir = locations_[selectedLocation_].path + "\\steamapps\\common\\" + safeName;
    } else {
        auto& st = GetSteamStatus();
        installDir = st.steamPath + "\\steamapps\\common\\" + safeName;
    }

    // Initialize download state
    {
        std::lock_guard<std::mutex> lock(mtx_);
        download_ = {};
        download_.appId = pendingAppId_;
        download_.gameName = pendingGameName_;
        download_.installPath = installDir;
        download_.active = true;
        download_.statusText = u8"准备中...";
    }
    memset(speedHistory_, 0, sizeof(speedHistory_));
    speedHistoryIdx_ = 0;
    lastBytes_ = 0;
    downloadStartTime_ = (float)ImGui::GetTime();  // 使用 ImGui 时间以保持一致
    lastCheckTime_ = downloadStartTime_;

    ShowToast(pendingGameName_ + ui_str::START_INSTALL, ToastType::Info, 3.0f);

    // If we have a username from OAuth but no password, we need to ask for password
    if (!user.empty() && pass.empty()) {
        // Show login dialog to get password
        loginState_ = LoginState::ShowDialog;
        download_.statusText = u8"需要输入 Steam 密码";
        ShowToast(u8"请输入 Steam 密码以下载游戏", ToastType::Warning, 5.0f);
        return;
    }

    auto& dd = DepotDownloader::Get();
    dd.StartDownload(pendingAppId_, installDir, user, pass,
        [this](const DepotProgress& p) {
            OnDownloadProgress(p);
        });
}

void GameInstallPanel::OnDownloadProgress(const DepotProgress& p) {
    std::lock_guard<std::mutex> lock(mtx_);

    // Debug output
    char dbg[256];
    snprintf(dbg, sizeof(dbg), "[Progress] state=%s pct=%.2f bytes=%llu/%llu finished=%d success=%d\n",
             p.state.c_str(), p.percentage, (unsigned long long)p.bytesDownloaded,
             (unsigned long long)p.totalBytes, p.finished ? 1 : 0, p.success ? 1 : 0);
    OutputDebugStringA(dbg);

    // If already completed or failed, ignore further callbacks
    if (download_.completed || download_.failed) {
        OutputDebugStringA("[Progress] Ignoring callback - already completed/failed\n");
        return;
    }

    if (p.state == "steamguard") {
        // Need Steam Guard code
        loginState_ = LoginState::NeedSteamGuard;
        download_.statusText = u8"需要 Steam Guard 验证码";
        return;
    }

    if (p.state == "error") {
        download_.statusText = u8"错误";
        download_.errorMsg = p.error;
        download_.failed = true;
        // Check if it's a login error - might need credentials
        if (p.error.find("Login") != std::string::npos ||
            p.error.find("login") != std::string::npos ||
            p.error.find("authenticate") != std::string::npos) {
            loginState_ = LoginState::ShowDialog;
        }
        return;
    }

    // Update progress
    download_.bytesDownloaded = p.bytesDownloaded;
    download_.bytesToDownload = p.totalBytes;
    download_.isVerifying = p.isVerifying;
    download_.sizeIsEstimated = p.sizeIsEstimated;

    // 更新当前阶段
    if (p.isVerifying) {
        download_.currentPhase = "verify";
        download_.verifyProgress = (float)(p.percentage / 100.0);
        // 验证阶段也更新主进度条
        download_.progress = (float)(p.percentage / 100.0);
    } else {
        download_.currentPhase = "download";
        // 优先使用百分比（DepotDownloader 主要输出百分比）
        if (p.percentage > 0) {
            download_.progress = (float)(p.percentage / 100.0);
        } else if (p.totalBytes > 0) {
            download_.progress = (float)p.bytesDownloaded / (float)p.totalBytes;
        }
    }

    // Use speed from callback if available, otherwise calculate
    if (p.speed > 0) {
        download_.speed = p.speed;
        if (download_.speed > download_.peakSpeed) {
            download_.peakSpeed = download_.speed;
        }
        // Update speed history
        speedHistory_[speedHistoryIdx_] = download_.speed;
        speedHistoryIdx_ = (speedHistoryIdx_ + 1) % SPEED_HISTORY_SIZE;
    }

    // Calculate ETA based on percentage progress and elapsed time
    // This is more accurate than using totalBytes which may be estimated
    float currentProgress = download_.progress;
    if (currentProgress > 0.01f && currentProgress < 1.0f && download_.speed > 0) {
        // 基于当前进度和速度估算剩余时间
        // 方法1: 基于字节数（如果有）
        if (p.totalBytes > p.bytesDownloaded) {
            download_.eta = (float)(p.totalBytes - p.bytesDownloaded) / download_.speed;
        } else {
            // 方法2: 基于百分比和已用时间
            float now = (float)ImGui::GetTime();
            float elapsed = now - downloadStartTime_;
            if (elapsed > 1.0f) {
                // 剩余百分比 / 当前速度（百分比/秒）
                float remainingPercent = 1.0f - currentProgress;
                float percentPerSec = currentProgress / elapsed;
                if (percentPerSec > 0.0001f) {
                    download_.eta = remainingPercent / percentPerSec;
                }
            }
        }
    }

    // Update status text based on state
    if (p.state == "connecting") {
        download_.statusText = u8"连接 Steam...";
    } else if (p.state == "logging_in") {
        download_.statusText = u8"登录中...";
    } else if (p.state == "downloading") {
        download_.statusText = ui_str::DOWNLOADING;
        if (!p.currentFile.empty()) {
            // 只显示文件名，不显示完整路径
            size_t pos = p.currentFile.find_last_of("\\/");
            if (pos != std::string::npos) {
                download_.statusText = p.currentFile.substr(pos + 1);
            } else {
                download_.statusText = p.currentFile;
            }
        }
    } else if (p.state == "verifying") {
        // 验证阶段显示当前验证的文件
        if (!p.currentFile.empty()) {
            size_t pos = p.currentFile.find_last_of("\\/");
            if (pos != std::string::npos) {
                download_.statusText = u8"验证: " + p.currentFile.substr(pos + 1);
            } else {
                download_.statusText = u8"验证: " + p.currentFile;
            }
        } else {
            download_.statusText = u8"验证文件中...";
        }
    } else if (p.state == "completed" || p.finished) {
        if (p.success) {
            download_.completed = true;
            download_.progress = 1.0f;
            // 记录安装信息（包含游戏名称）
            InstallRecord::Get().AddInstalled(download_.appId, download_.installPath, download_.gameName);
            // 添加到用户库（确保游戏出现在侧边栏列表中）
            UserLibrary::Get().AddGame(download_.appId, download_.gameName, "game", "installed");
            // 通知 sidebar 更新游戏状态
            NotifyGameInstalled(download_.appId);
            // 强制刷新侧边栏游戏列表
            NotifyGameAddedToLibrary(download_.appId, download_.gameName);
            // 如果下载量为 0，说明文件已存在，验证通过
            if (p.bytesDownloaded == 0) {
                download_.verifyOnly = true;
                download_.statusText = u8"验证完成 - 文件已是最新";
                ShowGameCardNotification(download_.appId, download_.gameName, u8"验证完成", 4.0f);
            } else {
                download_.verifyOnly = false;
                download_.statusText = ui_str::COMPLETED;
                ShowGameCardNotification(download_.appId, download_.gameName, u8"安装完成", 4.0f);
            }
            // Create shortcuts
            CreateGameShortcut(download_.appId, download_.gameName,
                              createDesktopShortcut_, createStartMenuShortcut_);
        } else {
            download_.statusText = ui_str::FAILED;
            download_.failed = true;
            download_.errorMsg = p.error;
        }
    }
}

// ── Render Install Dialog (modal overlay) ──

void GameInstallPanel::Render(float winW, float winH) {
    // Render login dialog if needed
    RenderLoginDialog(winW, winH);

    if (!showDialog_) return;

    // Get Store data for game size and header image
    const SteamStoreData* storeData = GetStoreData(pendingAppId_);

    // Try to update game size from Store API if not yet known
    if (pendingGameSize_ <= 0) {
        if (storeData && storeData->loaded && storeData->diskSpaceBytes > 0) {
            pendingGameSize_ = (float)storeData->diskSpaceBytes;
        }
    }

    // Request header image from Store API if CDN texture not available
    if (!GetGameTexture(pendingAppId_) && storeData && storeData->loaded && !storeData->headerImage.empty()) {
        std::string headerKey = "header_" + pendingAppId_;
        if (!GetTextureByKey(headerKey)) {
            RequestTextureFromUrl(headerKey, storeData->headerImage);
        }
    }

    float dt = ImGui::GetIO().DeltaTime;
    dialogAnim_ = std::min(dialogAnim_ + dt * 6.0f, 1.0f);
    float alpha = dialogAnim_;
    int a255 = (int)(255 * alpha);

    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smFont = g_mainFontSmall ? g_mainFontSmall : font;
    ImFont* large = g_mainFontLarge ? g_mainFontLarge : font;

    // 1) Full-screen dark overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, (int)(180 * alpha)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags ofl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("##InstallOverlay", nullptr, ofl)) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && ImGui::IsMouseClicked(0))
            showDialog_ = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // 2) Dialog window - Steam style: wider, shorter
    float dlgW = 480.0f, dlgH = 420.0f;
    float dlgX = (winW - dlgW) * 0.5f;
    float dlgY = (winH - dlgH) * 0.5f + (1.0f - alpha) * 20.0f;

    ImGui::SetNextWindowPos(ImVec2(dlgX, dlgY));
    ImGui::SetNextWindowSize(ImVec2(dlgW, dlgH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(27, 40, 56, (int)(255 * alpha)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags pfl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("##InstallDialog", nullptr, pfl)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wMin = ImGui::GetWindowPos();
        ImVec2 wMax(wMin.x + dlgW, wMin.y + dlgH);

        // Drop shadow
        for (int s = 1; s <= 12; s++) {
            float f = s / 13.0f;
            ImU32 sc = IM_COL32(0, 0, 0, (int)(50.0f * alpha * (1.0f - f)));
            dl->AddRectFilled(ImVec2(wMin.x - s, wMin.y - s),
                              ImVec2(wMax.x + s, wMax.y + s), sc, 6.0f);
        }

        // Main background
        dl->AddRectFilled(wMin, wMax, IM_COL32(27, 40, 56, a255), 4.0f);

        // ── Header: Game banner image ──
        float bannerH = 120.0f;
        ImTextureID headerTex = GetGameTexture(pendingAppId_);
        // Fallback to Store API header image if CDN texture not available
        if (!headerTex) {
            std::string headerKey = "header_" + pendingAppId_;
            headerTex = GetTextureByKey(headerKey);
        }
        if (headerTex) {
            // Game header image as banner
            dl->AddImageRounded(headerTex, wMin, ImVec2(wMax.x, wMin.y + bannerH),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, a255), 4.0f, ImDrawFlags_RoundCornersTop);
            // Gradient overlay at bottom of banner
            dl->AddRectFilledMultiColor(
                ImVec2(wMin.x, wMin.y + bannerH - 40), ImVec2(wMax.x, wMin.y + bannerH),
                IM_COL32(27, 40, 56, 0), IM_COL32(27, 40, 56, 0),
                IM_COL32(27, 40, 56, a255), IM_COL32(27, 40, 56, a255));
        } else {
            // Placeholder gradient banner
            dl->AddRectFilledMultiColor(wMin, ImVec2(wMax.x, wMin.y + bannerH),
                IM_COL32(42, 71, 94, a255), IM_COL32(23, 42, 58, a255),
                IM_COL32(23, 42, 58, a255), IM_COL32(42, 71, 94, a255));
        }

        // Close button (X)
        {
            float closeX = wMax.x - 36;
            float closeY = wMin.y + 8;
            ImGui::SetCursorScreenPos(ImVec2(closeX, closeY));
            ImGui::PushID("close_x");
            ImGui::InvisibleButton("##x", ImVec2(28, 28));
            bool xhov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) showDialog_ = false;
            ImGui::PopID();
            if (xhov) {
                dl->AddCircleFilled(ImVec2(closeX + 14, closeY + 14), 14.0f,
                                    IM_COL32(0, 0, 0, (int)(100 * alpha)));
            }
            if (g_iconFont) {
                ImU32 xCol = xhov ? IM_COL32(255, 255, 255, a255)
                                  : IM_COL32(200, 200, 200, (int)(200 * alpha));
                dl->AddText(g_iconFont, 14.0f, ImVec2(closeX + 7, closeY + 7), xCol, icon::CLOSE);
            }
        }

        // Content area starts below banner
        float contentX = wMin.x + 24;
        float contentW = dlgW - 48;
        float cy = wMin.y + bannerH + 16;

        // Game title
        dl->AddText(large, large->FontSize, ImVec2(contentX, cy),
                    IM_COL32(255, 255, 255, a255), pendingGameName_.c_str());
        cy += large->FontSize + 20;

        // ── Install location dropdown style ──
        dl->AddText(smFont, smFont->FontSize, ImVec2(contentX, cy),
                    IM_COL32(139, 147, 157, a255), ui_str::INSTALL_LOC);
        cy += smFont->FontSize + 8;

        // Dropdown box
        float dropH = 36.0f;
        ImGui::SetCursorScreenPos(ImVec2(contentX, cy));
        ImGui::PushID("loc_dropdown");
        ImGui::InvisibleButton("##drop", ImVec2(contentW, dropH));
        bool dropHov = ImGui::IsItemHovered();
        ImGui::PopID();

        ImU32 dropBg = dropHov ? IM_COL32(42, 55, 71, a255) : IM_COL32(35, 48, 64, a255);
        dl->AddRectFilled(ImVec2(contentX, cy), ImVec2(contentX + contentW, cy + dropH),
                          dropBg, 3.0f);

        // Current selection text
        if (!locations_.empty() && selectedLocation_ < (int)locations_.size()) {
            auto& loc = locations_[selectedLocation_];
            std::string locText = loc.label + "  -  " + FormatBytes(loc.freeBytes) + ui_str::FREE_SPACE;
            dl->AddText(font, font->FontSize, ImVec2(contentX + 12, cy + (dropH - font->FontSize) * 0.5f),
                        IM_COL32(199, 213, 224, a255), locText.c_str());
        }

        // Dropdown arrow
        if (g_iconFont) {
            dl->AddText(g_iconFont, 12.0f, ImVec2(contentX + contentW - 24, cy + (dropH - 12) * 0.5f),
                        IM_COL32(139, 147, 157, a255), icon::CHEVDOWN);
        }
        cy += dropH + 12;

        // ── Location list (compact) ──
        if (locations_.size() > 1) {
            for (int i = 0; i < (int)locations_.size(); i++) {
                auto& loc = locations_[i];
                bool selected = (i == selectedLocation_);
                float locH = 32.0f;

                ImGui::PushID(i);
                ImGui::SetCursorScreenPos(ImVec2(contentX, cy));
                ImGui::InvisibleButton("##loc", ImVec2(contentW, locH));
                bool hov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) selectedLocation_ = i;
                ImGui::PopID();

                if (selected || hov) {
                    ImU32 bg = selected ? IM_COL32(42, 55, 71, a255) : IM_COL32(35, 48, 64, a255);
                    dl->AddRectFilled(ImVec2(contentX, cy), ImVec2(contentX + contentW, cy + locH), bg, 3.0f);
                }

                // Radio circle
                float radioX = contentX + 12;
                float radioY = cy + locH * 0.5f;
                if (selected) {
                    dl->AddCircleFilled(ImVec2(radioX, radioY), 6.0f, IM_COL32(103, 193, 245, a255));
                    dl->AddCircleFilled(ImVec2(radioX, radioY), 2.5f, IM_COL32(27, 40, 56, a255));
                } else {
                    dl->AddCircle(ImVec2(radioX, radioY), 6.0f, IM_COL32(103, 117, 131, a255), 0, 1.5f);
                }

                // Location text
                std::string locText = loc.label + "  (" + FormatBytes(loc.freeBytes) + ui_str::FREE_SPACE + ")";
                dl->AddText(font, font->FontSize, ImVec2(contentX + 28, cy + (locH - font->FontSize) * 0.5f),
                            selected ? IM_COL32(255, 255, 255, a255) : IM_COL32(180, 190, 200, a255),
                            locText.c_str());

                cy += locH + 2;
            }
            cy += 8;
        }

        // ── Shortcut checkboxes (Steam style - horizontal, centered) ──
        cy += 20;
        float cbSize = 18.0f;
        float cbGap = 8.0f;
        float cbTextGap = 32.0f;  // gap between two checkbox items

        // Calculate text widths for centering
        ImVec2 desktopTextSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, ui_str::DESKTOP_SC);
        ImVec2 startMenuTextSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, ui_str::STARTMENU_SC);
        float cb1W = cbSize + cbGap + desktopTextSz.x;
        float cb2W = cbSize + cbGap + startMenuTextSz.x;
        float totalCbW = cb1W + cbTextGap + cb2W;
        float cbStartX = wMin.x + (dlgW - totalCbW) * 0.5f;

        // Desktop shortcut checkbox
        {
            ImGui::SetCursorScreenPos(ImVec2(cbStartX, cy));
            ImGui::PushID("cb_desktop");
            ImGui::InvisibleButton("##cb", ImVec2(cb1W, cbSize + 6));
            bool hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) createDesktopShortcut_ = !createDesktopShortcut_;
            ImGui::PopID();

            ImVec2 cbPos(cbStartX, cy + 3);
            // Checkbox background with hover effect
            ImU32 cbBg = createDesktopShortcut_
                ? IM_COL32(26, 159, 255, a255)  // Steam blue when checked
                : (hov ? IM_COL32(50, 65, 85, a255) : IM_COL32(35, 48, 64, a255));
            dl->AddRectFilled(cbPos, ImVec2(cbPos.x + cbSize, cbPos.y + cbSize), cbBg, 3.0f);

            if (!createDesktopShortcut_) {
                // Border for unchecked state
                ImU32 borderCol = hov ? IM_COL32(103, 193, 245, a255) : IM_COL32(80, 95, 115, a255);
                dl->AddRect(cbPos, ImVec2(cbPos.x + cbSize, cbPos.y + cbSize), borderCol, 3.0f, 0, 1.5f);
            }

            if (createDesktopShortcut_ && g_iconFont) {
                // Checkmark icon centered in box
                dl->AddText(g_iconFont, cbSize - 4, ImVec2(cbPos.x + 3, cbPos.y + 1),
                            IM_COL32(255, 255, 255, a255), icon::CHECK);
            }

            // Label text
            ImU32 textCol = hov ? IM_COL32(255, 255, 255, a255) : IM_COL32(199, 213, 224, a255);
            dl->AddText(font, font->FontSize, ImVec2(cbStartX + cbSize + cbGap, cy + (cbSize - font->FontSize) * 0.5f + 3),
                        textCol, ui_str::DESKTOP_SC);
        }

        // Start menu shortcut checkbox
        {
            float cb2X = cbStartX + cb1W + cbTextGap;
            ImGui::SetCursorScreenPos(ImVec2(cb2X, cy));
            ImGui::PushID("cb_startmenu");
            ImGui::InvisibleButton("##cb", ImVec2(cb2W, cbSize + 6));
            bool hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) createStartMenuShortcut_ = !createStartMenuShortcut_;
            ImGui::PopID();

            ImVec2 cbPos(cb2X, cy + 3);
            // Checkbox background with hover effect
            ImU32 cbBg = createStartMenuShortcut_
                ? IM_COL32(26, 159, 255, a255)  // Steam blue when checked
                : (hov ? IM_COL32(50, 65, 85, a255) : IM_COL32(35, 48, 64, a255));
            dl->AddRectFilled(cbPos, ImVec2(cbPos.x + cbSize, cbPos.y + cbSize), cbBg, 3.0f);

            if (!createStartMenuShortcut_) {
                // Border for unchecked state
                ImU32 borderCol = hov ? IM_COL32(103, 193, 245, a255) : IM_COL32(80, 95, 115, a255);
                dl->AddRect(cbPos, ImVec2(cbPos.x + cbSize, cbPos.y + cbSize), borderCol, 3.0f, 0, 1.5f);
            }

            if (createStartMenuShortcut_ && g_iconFont) {
                // Checkmark icon centered in box
                dl->AddText(g_iconFont, cbSize - 4, ImVec2(cbPos.x + 3, cbPos.y + 1),
                            IM_COL32(255, 255, 255, a255), icon::CHECK);
            }

            // Label text
            ImU32 textCol = hov ? IM_COL32(255, 255, 255, a255) : IM_COL32(199, 213, 224, a255);
            dl->AddText(font, font->FontSize, ImVec2(cb2X + cbSize + cbGap, cy + (cbSize - font->FontSize) * 0.5f + 3),
                        textCol, ui_str::STARTMENU_SC);
        }
        cy += cbSize + 28;

        // ── Check region restriction and coming soon status ──
        bool isRegionRestricted = false;
        bool isComingSoon = false;
        if (storeData && storeData->loaded) {
            isRegionRestricted = storeData->regionRestricted;
            isComingSoon = storeData->comingSoon;
        }

        // ── Check if disk space is sufficient ──
        bool hasEnoughSpace = true;
        bool sizeUnknown = (pendingGameSize_ <= 0);
        int64_t requiredBytes = (int64_t)pendingGameSize_;  // pendingGameSize_ is already in bytes
        float requiredGB = pendingGameSize_ / (1024.0f * 1024.0f * 1024.0f);  // Convert to GB for display

        if (!sizeUnknown && !locations_.empty() && selectedLocation_ < (int)locations_.size()) {
            hasEnoughSpace = locations_[selectedLocation_].freeBytes >= requiredBytes;
        }

        // Can install only if has enough space AND not region restricted AND not coming soon
        bool canInstall = hasEnoughSpace && !isRegionRestricted && !isComingSoon;

        // ── Show coming soon warning ──
        if (isComingSoon) {
            const char* comingWarnText = u8"此游戏尚未发售";
            ImVec2 warnSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, comingWarnText);
            float warnX = wMin.x + (dlgW - warnSz.x) * 0.5f;
            dl->AddText(font, font->FontSize, ImVec2(warnX, wMax.y - 120),
                        IM_COL32(255, 180, 80, a255), comingWarnText);

            const char* comingHintText = u8"请等待游戏正式发售后再下载";
            ImVec2 hintSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, comingHintText);
            float hintX = wMin.x + (dlgW - hintSz.x) * 0.5f;
            dl->AddText(font, font->FontSize, ImVec2(hintX, wMax.y - 100),
                        IM_COL32(180, 180, 180, a255), comingHintText);
        }
        // ── Show region restriction warning ──
        else if (isRegionRestricted) {
            const char* regionWarnText = u8"此游戏在您所在地区不可用";
            ImVec2 warnSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, regionWarnText);
            float warnX = wMin.x + (dlgW - warnSz.x) * 0.5f;
            dl->AddText(font, font->FontSize, ImVec2(warnX, wMax.y - 120),
                        IM_COL32(255, 80, 80, a255), regionWarnText);

            const char* regionHintText = u8"无法下载地区限制的游戏";
            ImVec2 hintSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, regionHintText);
            float hintX = wMin.x + (dlgW - hintSz.x) * 0.5f;
            dl->AddText(font, font->FontSize, ImVec2(hintX, wMax.y - 100),
                        IM_COL32(180, 180, 180, a255), regionHintText);
        }
        // ── Show required space info ──
        else if (!locations_.empty() && selectedLocation_ < (int)locations_.size()) {
            char spaceBuf[128];
            if (sizeUnknown) {
                snprintf(spaceBuf, sizeof(spaceBuf), u8"可用空间: %s",
                         FormatBytes(locations_[selectedLocation_].freeBytes).c_str());
                ImVec2 spaceSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, spaceBuf);
                float spaceX = wMin.x + (dlgW - spaceSz.x) * 0.5f;
                dl->AddText(font, font->FontSize, ImVec2(spaceX, wMax.y - 100),
                            IM_COL32(139, 147, 157, a255), spaceBuf);
            } else if (!hasEnoughSpace) {
                // Show warning if not enough space
                const char* warnText = u8"磁盘空间不足！";
                ImVec2 warnSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, warnText);
                float warnX = wMin.x + (dlgW - warnSz.x) * 0.5f;
                dl->AddText(font, font->FontSize, ImVec2(warnX, wMax.y - 120),
                            IM_COL32(255, 80, 80, a255), warnText);

                snprintf(spaceBuf, sizeof(spaceBuf), u8"需要 %.1f GB，可用 %s",
                         requiredGB, FormatBytes(locations_[selectedLocation_].freeBytes).c_str());
                ImVec2 needSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, spaceBuf);
                float needX = wMin.x + (dlgW - needSz.x) * 0.5f;
                dl->AddText(font, font->FontSize, ImVec2(needX, wMax.y - 100),
                            IM_COL32(180, 180, 180, a255), spaceBuf);
            } else {
                // Show required space
                snprintf(spaceBuf, sizeof(spaceBuf), u8"需要 %.1f GB，可用 %s",
                         requiredGB, FormatBytes(locations_[selectedLocation_].freeBytes).c_str());
                ImVec2 spaceSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, spaceBuf);
                float spaceX = wMin.x + (dlgW - spaceSz.x) * 0.5f;
                dl->AddText(font, font->FontSize, ImVec2(spaceX, wMax.y - 100),
                            IM_COL32(139, 147, 157, a255), spaceBuf);
            }
        }

        // ── Buttons: Install + Cancel (Steam style - centered, blue) ──
        float btnW = 120.0f, btnH = 36.0f, btnGap = 16.0f;
        float btnY = wMax.y - btnH - 24;
        float btnsW = btnW * 2 + btnGap;
        float btnX = wMin.x + (dlgW - btnsW) * 0.5f;

        // Install button (left) - Steam blue or gray if disabled
        {
            ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
            ImGui::PushID("go_install");
            ImGui::InvisibleButton("##btn", ImVec2(btnW, btnH));
            bool hov = canInstall && ImGui::IsItemHovered();
            bool clk = canInstall && ImGui::IsItemClicked();
            ImGui::PopID();

            // Steam blue gradient or gray if disabled
            ImU32 bgL, bgR;
            if (canInstall) {
                bgL = hov ? IM_COL32(47, 137, 197, a255) : IM_COL32(26, 159, 255, a255);
                bgR = hov ? IM_COL32(37, 127, 187, a255) : IM_COL32(16, 149, 245, a255);
            } else {
                bgL = IM_COL32(80, 80, 80, a255);
                bgR = IM_COL32(60, 60, 60, a255);
            }
            dl->AddRectFilledMultiColor(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH),
                                        bgL, bgR, bgR, bgL);

            const char* installText = ui_str::INSTALL_BTN;
            ImVec2 tsz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, installText);
            ImU32 textCol = canInstall ? IM_COL32(255, 255, 255, a255) : IM_COL32(120, 120, 120, a255);
            dl->AddText(font, font->FontSize,
                        ImVec2(btnX + (btnW - tsz.x) * 0.5f, btnY + (btnH - tsz.y) * 0.5f),
                        textCol, installText);

            if (clk) StartInstall();
        }

        btnX += btnW + btnGap;

        // Cancel button (right) - darker
        {
            ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
            ImGui::PushID("go_cancel");
            ImGui::InvisibleButton("##btn", ImVec2(btnW, btnH));
            bool hov = ImGui::IsItemHovered();
            bool clk = ImGui::IsItemClicked();
            ImGui::PopID();

            ImU32 bgCol = hov ? IM_COL32(60, 75, 92, a255) : IM_COL32(50, 65, 82, a255);
            dl->AddRectFilled(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH), bgCol, 2.0f);

            const char* cancelText = ui_str::CANCEL_BTN;
            ImVec2 tsz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, cancelText);
            dl->AddText(font, font->FontSize,
                        ImVec2(btnX + (btnW - tsz.x) * 0.5f, btnY + (btnH - tsz.y) * 0.5f),
                        IM_COL32(199, 213, 224, a255), cancelText);

            if (clk) showDialog_ = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// ── Render Download Progress Panel (right side, replaces library content) ──

bool GameInstallPanel::RenderDownloadPanel(float x, float y, float width, float height) {
    DownloadState ds;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        ds = download_;
    }
    if (!ds.active) return false;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(27, 40, 56, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##download_panel", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus)) {

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        ImFont* smFont = g_mainFontSmall ? g_mainFontSmall : font;
        ImFont* large = g_mainFontLarge ? g_mainFontLarge : font;
        float dt = ImGui::GetIO().DeltaTime;

        float pad = 24.0f;
        float cx = x + pad;
        float cy = y + pad;
        float contentW = width - pad * 2;

        // ═══ 新布局：封面图独占一行，全宽大图 ═══
        // 游戏封面图 - 全宽显示，更高
        ImTextureID iconTex = GetGameTexture(ds.appId);
        float iconW = contentW;  // 占满整个内容宽度
        float iconH = 260.0f;    // 更高的封面
        if (iconTex) {
            dl->AddImageRounded(iconTex, ImVec2(cx, cy), ImVec2(cx + iconW, cy + iconH),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 8.0f);
        } else {
            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + iconW, cy + iconH),
                              IM_COL32(42, 55, 71, 255), 8.0f);
        }
        cy += iconH + 20;

        // ═══ 游戏名和状态在同一行，字体更大 ═══
        // 停止安装按钮（仅在下载中显示）
        float stopBtnW = 130.0f;
        float stopBtnH = 42.0f;
        bool showStopBtn = !ds.completed && !ds.failed;

        if (showStopBtn) {
            ImGui::SetCursorScreenPos(ImVec2(cx, cy));
            ImGui::PushID("stop_install");
            ImGui::InvisibleButton("##stop", ImVec2(stopBtnW, stopBtnH));
            bool stopHov = ImGui::IsItemHovered();
            bool stopClk = ImGui::IsItemClicked();
            ImGui::PopID();

            // 红色停止按钮
            ImU32 stopBg = stopHov ? IM_COL32(180, 50, 50, 255) : IM_COL32(140, 40, 40, 255);
            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + stopBtnW, cy + stopBtnH),
                              stopBg, 6.0f);

            const char* stopText = u8"停止安装";
            ImVec2 stopSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, stopText);
            dl->AddText(font, font->FontSize,
                        ImVec2(cx + (stopBtnW - stopSz.x) * 0.5f, cy + (stopBtnH - stopSz.y) * 0.5f),
                        IM_COL32(255, 255, 255, 255), stopText);

            if (stopClk) {
                // 停止安装并清理资源
                DepotDownloader::Get().CancelDownload();
                // 清理下载状态
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    download_.active = false;
                    download_.failed = true;
                }
                // 从用户库中移除正在下载的游戏
                UserLibrary::Get().RemoveGame(ds.appId);
            }
        }

        // 游戏名 - 更大字体，在停止按钮右边
        float nameX = showStopBtn ? (cx + stopBtnW + 16) : cx;
        float nameFontSize = large->FontSize * 1.5f;  // 更大的字体
        dl->AddText(large, nameFontSize, ImVec2(nameX, cy + (stopBtnH - nameFontSize) * 0.5f),
                    IM_COL32(199, 213, 224, 255), ds.gameName.c_str());

        // 状态标签 - 在游戏名右边，更精致的设计
        const char* statusText = ds.completed ? (ds.verifyOnly ? u8"已验证" : u8"已完成")
                                : ds.failed   ? u8"失败"
                                : ds.isVerifying ? u8"验证中"
                                              : u8"下载中";

        // 状态颜色 - 使用渐变效果
        ImU32 statusBgStart, statusBgEnd;
        if (ds.completed) {
            statusBgStart = IM_COL32(76, 140, 50, 255);
            statusBgEnd = IM_COL32(56, 107, 34, 255);
        } else if (ds.failed) {
            statusBgStart = IM_COL32(180, 60, 60, 255);
            statusBgEnd = IM_COL32(140, 40, 40, 255);
        } else if (ds.isVerifying) {
            statusBgStart = IM_COL32(220, 180, 60, 255);
            statusBgEnd = IM_COL32(180, 140, 40, 255);
        } else {
            // 下载中 - 使用更亮眼的蓝色渐变
            statusBgStart = IM_COL32(80, 180, 255, 255);
            statusBgEnd = IM_COL32(47, 137, 197, 255);
        }

        ImVec2 nameSz = large->CalcTextSizeA(nameFontSize, FLT_MAX, 0, ds.gameName.c_str());
        ImVec2 statusSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, statusText);
        float statusPadX = 14.0f;
        float statusPadY = 6.0f;
        float statusX = nameX + nameSz.x + 20;
        float statusY = cy + (stopBtnH - font->FontSize - statusPadY * 2) * 0.5f;
        float statusW = statusSz.x + statusPadX * 2;
        float statusH = font->FontSize + statusPadY * 2;

        // 渐变背景
        dl->AddRectFilledMultiColor(
            ImVec2(statusX, statusY),
            ImVec2(statusX + statusW, statusY + statusH),
            statusBgStart, statusBgEnd, statusBgEnd, statusBgStart);

        // 圆角边框
        dl->AddRect(ImVec2(statusX, statusY),
                    ImVec2(statusX + statusW, statusY + statusH),
                    IM_COL32(255, 255, 255, 40), 5.0f, 0, 1.5f);

        // 下载中时添加动态效果
        if (!ds.completed && !ds.failed && !ds.isVerifying) {
            // 呼吸灯效果
            float pulse = (sinf((float)ImGui::GetTime() * 3.0f) + 1.0f) * 0.5f;
            ImU32 glowCol = IM_COL32(102, 192, 244, (int)(60 * pulse));
            dl->AddRectFilled(ImVec2(statusX - 2, statusY - 2),
                              ImVec2(statusX + statusW + 2, statusY + statusH + 2),
                              glowCol, 7.0f);
        }

        dl->AddText(font, font->FontSize, ImVec2(statusX + statusPadX, statusY + statusPadY),
                    IM_COL32(255, 255, 255, 255), statusText);

        cy += stopBtnH + 16;

        // ═══ Progress Section ═══
        float progress = std::clamp(ds.progress, 0.0f, 1.0f);

        // Progress percentage - large display
        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), "%.1f%%", progress * 100.0f);
        dl->AddText(large, large->FontSize * 1.5f, ImVec2(cx, cy),
                    IM_COL32(199, 213, 224, 255), pctBuf);
        cy += large->FontSize * 1.5f + 12;

        // Steam-style progress bar
        float barH = 8.0f;
        float barY = cy;

        // Bar background - darker
        dl->AddRectFilled(ImVec2(cx, barY), ImVec2(cx + contentW, barY + barH),
                          IM_COL32(0, 0, 0, 100), 0.0f);

        // Bar fill - Steam blue/green gradient, yellow for verifying
        if (progress > 0.001f) {
            float fillW = contentW * progress;
            ImU32 barCol = ds.completed ? IM_COL32(90, 145, 50, 255)
                         : ds.isVerifying ? IM_COL32(200, 160, 50, 255)  // 黄色表示验证
                                        : IM_COL32(102, 192, 244, 255);
            dl->AddRectFilled(ImVec2(cx, barY), ImVec2(cx + fillW, barY + barH),
                              barCol, 0.0f);

            // Animated shine effect
            if (!ds.completed && !ds.failed) {
                float t = fmodf((float)ImGui::GetTime() * 0.5f, 1.0f);
                float shineX = cx + t * contentW;
                float shineW = 60.0f;
                if (shineX < cx + fillW) {
                    dl->AddRectFilledMultiColor(
                        ImVec2(shineX, barY), ImVec2(shineX + shineW, barY + barH),
                        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 40),
                        IM_COL32(255, 255, 255, 40), IM_COL32(255, 255, 255, 0));
                }
            }
        }

        cy = barY + barH + 16;

        // 当前状态文本（显示正在处理的文件）
        if (!ds.statusText.empty()) {
            // 截断过长的文件名
            std::string statusDisplay = ds.statusText;
            ImVec2 textSz = smFont->CalcTextSizeA(smFont->FontSize, FLT_MAX, 0, statusDisplay.c_str());
            if (textSz.x > contentW) {
                // 截断并添加省略号
                while (textSz.x > contentW - 20 && statusDisplay.length() > 10) {
                    statusDisplay = statusDisplay.substr(0, statusDisplay.length() - 4) + "...";
                    textSz = smFont->CalcTextSizeA(smFont->FontSize, FLT_MAX, 0, statusDisplay.c_str());
                }
            }
            dl->AddText(smFont, smFont->FontSize, ImVec2(cx, cy),
                        IM_COL32(180, 190, 200, 255), statusDisplay.c_str());
            cy += smFont->FontSize + 8;
        }

        // Downloaded / Total - Steam style (只在有真实数据时显示)
        if (ds.bytesToDownload > 0 && !ds.isVerifying && !ds.sizeIsEstimated) {
            std::string dlStr = FormatBytes(ds.bytesDownloaded) + " / " + FormatBytes(ds.bytesToDownload);
            dl->AddText(smFont, smFont->FontSize, ImVec2(cx, cy),
                        IM_COL32(139, 147, 157, 255), dlStr.c_str());
        }
        cy += 28;

        // ═══ Stats Section - Steam style cards ═══
        float speed = ds.speed;
        float eta = ds.eta;
        float elapsedTime = (float)ImGui::GetTime() - downloadStartTime_;

        // Update speed history for graph
        static int64_t lastGraphBytes = 0;
        static float lastGraphTime = 0;
        float now = (float)ImGui::GetTime();
        if (now - lastGraphTime > 0.5f && ds.bytesDownloaded > lastGraphBytes && !ds.completed) {
            speedHistory_[speedHistoryIdx_ % SPEED_HISTORY_SIZE] = speed;
            speedHistoryIdx_++;
            lastGraphBytes = ds.bytesDownloaded;
            lastGraphTime = now;
        }

        // Stats in Steam-style boxes
        float boxW = (contentW - 12) / 2;
        float boxH = 56.0f;

        struct StatItem { const char* label; std::string value; };
        StatItem stats[] = {
            { u8"下载速度", FormatSpeed(speed) },
            { u8"剩余时间", FormatEta(eta) },
            { u8"峰值速度", FormatSpeed(ds.peakSpeed) },
            { u8"已用时间", FormatEta(elapsedTime) },
        };

        for (int i = 0; i < 4; i++) {
            float bx = cx + (i % 2) * (boxW + 12);
            float by = cy + (i / 2) * (boxH + 8);

            // Box background
            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + boxW, by + boxH),
                              IM_COL32(23, 34, 48, 255), 4.0f);

            // Label
            dl->AddText(smFont, smFont->FontSize, ImVec2(bx + 12, by + 8),
                        IM_COL32(139, 147, 157, 255), stats[i].label);

            // Value - larger and highlighted for speed
            ImU32 valCol = (i == 0) ? IM_COL32(102, 192, 244, 255) : IM_COL32(199, 213, 224, 255);
            dl->AddText(font, font->FontSize, ImVec2(bx + 12, by + 28),
                        valCol, stats[i].value.c_str());
        }

        cy += (boxH + 8) * 2 + 16;

        // ═══ Speed Graph - Steam style ═══
        dl->AddText(smFont, smFont->FontSize, ImVec2(cx, cy),
                    IM_COL32(139, 147, 157, 255), u8"网络使用情况");
        cy += smFont->FontSize + 8;

        float graphH = 80.0f;
        float graphW = contentW;

        // Graph background - darker
        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + graphW, cy + graphH),
                          IM_COL32(23, 34, 48, 255), 4.0f);

        // Subtle grid lines
        for (int g = 1; g < 4; g++) {
            float gy = cy + graphH * g / 4.0f;
            dl->AddLine(ImVec2(cx, gy), ImVec2(cx + graphW, gy),
                        IM_COL32(40, 50, 65, 100));
        }

        // Find max for scaling
        float maxSpeed = 1.0f;
        for (int i = 0; i < SPEED_HISTORY_SIZE; i++) {
            if (speedHistory_[i] > maxSpeed) maxSpeed = speedHistory_[i];
        }

        // Draw speed area fill and line
        int count = std::min(speedHistoryIdx_, SPEED_HISTORY_SIZE);
        if (count > 1) {
            float stepX = graphW / (float)(SPEED_HISTORY_SIZE - 1);

            // Build path for filled area
            std::vector<ImVec2> points;
            points.push_back(ImVec2(cx, cy + graphH));

            for (int i = 0; i < count; i++) {
                int idx = (speedHistoryIdx_ - count + i + SPEED_HISTORY_SIZE) % SPEED_HISTORY_SIZE;
                float val = speedHistory_[idx] / maxSpeed;
                points.push_back(ImVec2(cx + i * stepX, cy + graphH * (1.0f - val)));
            }
            points.push_back(ImVec2(cx + (count - 1) * stepX, cy + graphH));

            // Fill area with gradient
            for (size_t i = 1; i < points.size() - 1; i++) {
                dl->AddRectFilledMultiColor(
                    ImVec2(points[i].x, points[i].y),
                    ImVec2(points[i + 1].x, cy + graphH),
                    IM_COL32(102, 192, 244, 40), IM_COL32(102, 192, 244, 40),
                    IM_COL32(102, 192, 244, 5), IM_COL32(102, 192, 244, 5));
            }

            // Draw line
            for (size_t i = 1; i < points.size() - 2; i++) {
                dl->AddLine(points[i], points[i + 1], IM_COL32(102, 192, 244, 255), 2.0f);
            }
        }

        // Max speed label - top right
        {
            std::string maxLabel = FormatSpeed(maxSpeed);
            ImVec2 labelSz = smFont->CalcTextSizeA(smFont->FontSize, FLT_MAX, 0, maxLabel.c_str());
            dl->AddText(smFont, smFont->FontSize,
                ImVec2(cx + graphW - labelSz.x - 8, cy + 4),
                IM_COL32(139, 147, 157, 200), maxLabel.c_str());
        }

        cy += graphH + 16;

        // ═══ Disk Activity - Steam style ═══
        if (ds.bytesToStage > 0) {
            dl->AddText(smFont, smFont->FontSize, ImVec2(cx, cy),
                        IM_COL32(139, 147, 157, 255), u8"磁盘使用情况");
            cy += smFont->FontSize + 8;

            float stageProgress = (float)ds.bytesStaged / (float)std::max(ds.bytesToStage, (int64_t)1);
            float sBarH = 4.0f;

            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + contentW, cy + sBarH),
                              IM_COL32(0, 0, 0, 100), 0.0f);
            dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + contentW * stageProgress, cy + sBarH),
                              IM_COL32(90, 145, 50, 255), 0.0f);

            std::string stageStr = FormatBytes(ds.bytesStaged) + " / " + FormatBytes(ds.bytesToStage);
            cy += sBarH + 6;
            dl->AddText(smFont, smFont->FontSize, ImVec2(cx, cy),
                        IM_COL32(139, 147, 157, 200), stageStr.c_str());
            cy += smFont->FontSize + 16;
        }

        // ═══ Completed State - Steam style Play button ═══
        if (ds.completed) {
            cy += 16;

            // Steam-style green gradient play button
            float playBtnW = 220.0f;
            float playBtnH = 48.0f;
            float playBtnX = cx + (contentW - playBtnW) * 0.5f;

            ImGui::SetCursorScreenPos(ImVec2(playBtnX, cy));
            ImGui::PushID("play_installed");
            ImGui::InvisibleButton("##play", ImVec2(playBtnW, playBtnH));
            bool playHov = ImGui::IsItemHovered();
            bool playClk = ImGui::IsItemClicked();
            ImGui::PopID();

            // Steam green gradient
            ImU32 btnT = playHov ? IM_COL32(110, 165, 60, 255) : IM_COL32(90, 145, 50, 255);
            ImU32 btnB = playHov ? IM_COL32(80, 130, 40, 255) : IM_COL32(70, 115, 35, 255);
            dl->AddRectFilledMultiColor(
                ImVec2(playBtnX, cy), ImVec2(playBtnX + playBtnW, cy + playBtnH),
                btnT, btnT, btnB, btnB);

            // Play icon + text
            const char* playText = u8"开始游戏";
            ImVec2 pts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, playText);
            float textX = playBtnX + (playBtnW - pts.x) * 0.5f;
            float textY = cy + (playBtnH - pts.y) * 0.5f;

            // Play triangle icon
            if (g_iconFont) {
                dl->AddText(g_iconFont, 16.0f, ImVec2(textX - 24, textY),
                            IM_COL32(255, 255, 255, 255), icon::PLAY);
            }

            dl->AddText(font, font->FontSize, ImVec2(textX, textY),
                        IM_COL32(255, 255, 255, 255), playText);

            if (playClk) {
                // Launch game using unified logic
                Navigate(NavAction::LaunchGame, ds.appId);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    return true;
}

// ── Login Dialog for Steam credentials ──

void GameInstallPanel::RenderLoginDialog(float winW, float winH) {
    if (loginState_ != LoginState::ShowDialog && loginState_ != LoginState::NeedSteamGuard)
        return;

    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smFont = g_mainFontSmall ? g_mainFontSmall : font;
    ImFont* large = g_mainFontLarge ? g_mainFontLarge : font;

    // Dark overlay with blur effect simulation
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 180));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("##LoginOverlay", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Steam-style dialog dimensions
    float dlgW = 450.0f;
    float dlgH = (loginState_ == LoginState::NeedSteamGuard) ? 320.0f : 440.0f;
    float dlgX = (winW - dlgW) * 0.5f;
    float dlgY = (winH - dlgH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(dlgX, dlgY));
    ImGui::SetNextWindowSize(ImVec2(dlgW, dlgH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##LoginDialog", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wMin = ImGui::GetWindowPos();
        ImVec2 wMax(wMin.x + dlgW, wMin.y + dlgH);

        // Steam-style shadow
        for (int s = 1; s <= 8; s++) {
            float f = s / 9.0f;
            ImU32 sc = IM_COL32(0, 0, 0, (int)(80 * (1.0f - f)));
            dl->AddRectFilled(ImVec2(wMin.x - s, wMin.y - s),
                              ImVec2(wMax.x + s, wMax.y + s), sc);
        }

        // Main background - Steam dark blue
        dl->AddRectFilled(wMin, wMax, IM_COL32(27, 40, 56, 255));

        // Top gradient bar (Steam blue accent)
        dl->AddRectFilledMultiColor(
            wMin, ImVec2(wMax.x, wMin.y + 4),
            IM_COL32(102, 192, 244, 255), IM_COL32(102, 192, 244, 255),
            IM_COL32(47, 137, 197, 255), IM_COL32(47, 137, 197, 255));

        float cy = 28.0f;
        float inputW = dlgW - 60;
        float inputX = 30.0f;

        if (loginState_ == LoginState::NeedSteamGuard) {
            // ═══ Steam Guard Dialog ═══

            // Shield icon area
            float iconSize = 64.0f;
            float iconX = (dlgW - iconSize) * 0.5f;
            dl->AddRectFilled(
                ImVec2(wMin.x + iconX, wMin.y + cy),
                ImVec2(wMin.x + iconX + iconSize, wMin.y + cy + iconSize),
                IM_COL32(47, 137, 197, 40), 8.0f);
            dl->AddRect(
                ImVec2(wMin.x + iconX, wMin.y + cy),
                ImVec2(wMin.x + iconX + iconSize, wMin.y + cy + iconSize),
                IM_COL32(102, 192, 244, 100), 8.0f);

            // Shield icon text (using icon font if available)
            if (g_iconFont) {
                dl->AddText(g_iconFont, 32.0f,
                    ImVec2(wMin.x + iconX + 16, wMin.y + cy + 16),
                    IM_COL32(102, 192, 244, 255), icon::SHIELD);
            }
            cy += iconSize + 20;

            // Title
            const char* title = u8"Steam Guard";
            ImVec2 titleSz = large->CalcTextSizeA(large->FontSize, FLT_MAX, 0, title);
            dl->AddText(large, large->FontSize,
                ImVec2(wMin.x + (dlgW - titleSz.x) * 0.5f, wMin.y + cy),
                IM_COL32(255, 255, 255, 255), title);
            cy += large->FontSize + 12;

            // Subtitle
            const char* subtitle = u8"请输入发送到您邮箱的验证码";
            ImVec2 subSz = smFont->CalcTextSizeA(smFont->FontSize, FLT_MAX, 0, subtitle);
            dl->AddText(smFont, smFont->FontSize,
                ImVec2(wMin.x + (dlgW - subSz.x) * 0.5f, wMin.y + cy),
                IM_COL32(139, 147, 157, 255), subtitle);
            cy += smFont->FontSize + 24;

            // Code input - centered, larger
            float codeInputW = 200.0f;
            float codeInputX = (dlgW - codeInputW) * 0.5f;

            // Input background
            dl->AddRectFilled(
                ImVec2(wMin.x + codeInputX, wMin.y + cy),
                ImVec2(wMin.x + codeInputX + codeInputW, wMin.y + cy + 44),
                IM_COL32(32, 32, 32, 255), 3.0f);
            dl->AddRect(
                ImVec2(wMin.x + codeInputX, wMin.y + cy),
                ImVec2(wMin.x + codeInputX + codeInputW, wMin.y + cy + 44),
                IM_COL32(62, 62, 62, 255), 3.0f);

            ImGui::SetCursorPos(ImVec2(codeInputX + 8, cy + 8));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(codeInputW - 16);
            ImGui::PushFont(large);
            ImGui::InputText("##steamguard", steamGuardCode_, sizeof(steamGuardCode_),
                ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AutoSelectAll);
            ImGui::PopFont();
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            cy += 60;

        } else {
            // ═══ Login Dialog ═══

            // Steam logo area (placeholder)
            float logoH = 50.0f;
            const char* logoText = u8"STEAM";
            ImVec2 logoSz = large->CalcTextSizeA(large->FontSize * 1.5f, FLT_MAX, 0, logoText);
            dl->AddText(large, large->FontSize * 1.5f,
                ImVec2(wMin.x + (dlgW - logoSz.x) * 0.5f, wMin.y + cy),
                IM_COL32(255, 255, 255, 255), logoText);
            cy += logoH + 10;

            // Subtitle
            const char* subtitle = u8"登录您的 Steam 账户";
            ImVec2 subSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, subtitle);
            dl->AddText(font, font->FontSize,
                ImVec2(wMin.x + (dlgW - subSz.x) * 0.5f, wMin.y + cy),
                IM_COL32(139, 147, 157, 255), subtitle);
            cy += font->FontSize + 24;

            // Username label
            dl->AddText(smFont, smFont->FontSize,
                ImVec2(wMin.x + inputX, wMin.y + cy),
                IM_COL32(139, 147, 157, 255), u8"Steam 账户名称");
            cy += smFont->FontSize + 8;

            // Username input - Steam style dark input
            dl->AddRectFilled(
                ImVec2(wMin.x + inputX, wMin.y + cy),
                ImVec2(wMin.x + inputX + inputW, wMin.y + cy + 40),
                IM_COL32(32, 32, 32, 255), 3.0f);
            dl->AddRect(
                ImVec2(wMin.x + inputX, wMin.y + cy),
                ImVec2(wMin.x + inputX + inputW, wMin.y + cy + 40),
                IM_COL32(62, 62, 62, 255), 3.0f);

            ImGui::SetCursorPos(ImVec2(inputX + 12, cy + 10));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(inputW - 24);
            ImGui::InputText("##user", username_, sizeof(username_));
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            cy += 52;

            // Password label
            dl->AddText(smFont, smFont->FontSize,
                ImVec2(wMin.x + inputX, wMin.y + cy),
                IM_COL32(139, 147, 157, 255), u8"密码");
            cy += smFont->FontSize + 8;

            // Password input
            dl->AddRectFilled(
                ImVec2(wMin.x + inputX, wMin.y + cy),
                ImVec2(wMin.x + inputX + inputW, wMin.y + cy + 40),
                IM_COL32(32, 32, 32, 255), 3.0f);
            dl->AddRect(
                ImVec2(wMin.x + inputX, wMin.y + cy),
                ImVec2(wMin.x + inputX + inputW, wMin.y + cy + 40),
                IM_COL32(62, 62, 62, 255), 3.0f);

            ImGui::SetCursorPos(ImVec2(inputX + 12, cy + 10));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushItemWidth(inputW - 24);
            ImGui::InputText("##pass", password_, sizeof(password_), ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            cy += 52;

            // Remember checkbox - Steam style
            ImGui::SetCursorPos(ImVec2(inputX, cy));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(32, 32, 32, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(42, 42, 42, 255));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(102, 192, 244, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(139, 147, 157, 255));
            ImGui::Checkbox(u8"记住我的密码", &rememberLogin_);
            ImGui::PopStyleColor(4);
            cy += 36;
        }

        // Error message
        if (!loginError_.empty()) {
            ImVec2 errSz = smFont->CalcTextSizeA(smFont->FontSize, FLT_MAX, 0, loginError_.c_str());
            dl->AddText(smFont, smFont->FontSize,
                ImVec2(wMin.x + (dlgW - errSz.x) * 0.5f, wMin.y + cy),
                IM_COL32(205, 51, 51, 255), loginError_.c_str());
            cy += 24;
        }

        // ═══ Buttons - Steam gradient style ═══
        float btnW = 180.0f, btnH = 40.0f;
        float btnY = dlgH - btnH - 24;
        float btnX = (dlgW - btnW * 2 - 16) * 0.5f;

        // Login button - Steam blue gradient
        ImGui::SetCursorPos(ImVec2(btnX, btnY));
        ImGui::PushID("login_btn");
        ImGui::InvisibleButton("##login", ImVec2(btnW, btnH));
        bool loginHov = ImGui::IsItemHovered();
        bool loginClk = ImGui::IsItemClicked();
        ImGui::PopID();

        ImU32 loginBgT = loginHov ? IM_COL32(71, 164, 228, 255) : IM_COL32(47, 137, 197, 255);
        ImU32 loginBgB = loginHov ? IM_COL32(47, 137, 197, 255) : IM_COL32(22, 101, 155, 255);
        dl->AddRectFilledMultiColor(
            ImVec2(wMin.x + btnX, wMin.y + btnY),
            ImVec2(wMin.x + btnX + btnW, wMin.y + btnY + btnH),
            loginBgT, loginBgT, loginBgB, loginBgB);
        dl->AddRect(
            ImVec2(wMin.x + btnX, wMin.y + btnY),
            ImVec2(wMin.x + btnX + btnW, wMin.y + btnY + btnH),
            IM_COL32(102, 192, 244, 100), 2.0f);

        const char* loginText = (loginState_ == LoginState::NeedSteamGuard) ? u8"提交" : u8"登录";
        ImVec2 loginSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, loginText);
        dl->AddText(font, font->FontSize,
            ImVec2(wMin.x + btnX + (btnW - loginSz.x) * 0.5f, wMin.y + btnY + (btnH - loginSz.y) * 0.5f),
            IM_COL32(255, 255, 255, 255), loginText);

        if (loginClk) TryLogin();

        // Cancel button - Steam dark style
        float cancelX = btnX + btnW + 16;
        ImGui::SetCursorPos(ImVec2(cancelX, btnY));
        ImGui::PushID("cancel_btn");
        ImGui::InvisibleButton("##cancel", ImVec2(btnW, btnH));
        bool cancelHov = ImGui::IsItemHovered();
        bool cancelClk = ImGui::IsItemClicked();
        ImGui::PopID();

        ImU32 cancelBg = cancelHov ? IM_COL32(60, 60, 60, 255) : IM_COL32(42, 42, 42, 255);
        dl->AddRectFilled(
            ImVec2(wMin.x + cancelX, wMin.y + btnY),
            ImVec2(wMin.x + cancelX + btnW, wMin.y + btnY + btnH),
            cancelBg, 2.0f);
        dl->AddRect(
            ImVec2(wMin.x + cancelX, wMin.y + btnY),
            ImVec2(wMin.x + cancelX + btnW, wMin.y + btnY + btnH),
            IM_COL32(80, 80, 80, 255), 2.0f);

        const char* cancelText = u8"取消";
        ImVec2 cancelSz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, cancelText);
        dl->AddText(font, font->FontSize,
            ImVec2(wMin.x + cancelX + (btnW - cancelSz.x) * 0.5f, wMin.y + btnY + (btnH - cancelSz.y) * 0.5f),
            IM_COL32(255, 255, 255, 255), cancelText);

        if (cancelClk) {
            loginState_ = LoginState::None;
            download_.active = false;
            // 如果是启动游戏的登录，清除状态
            if (showLaunchLogin_) {
                showLaunchLogin_ = false;
                launchCallback_ = nullptr;
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void GameInstallPanel::TryLogin() {
    if (loginState_ == LoginState::NeedSteamGuard) {
        // 发送 Steam Guard 验证码给正在运行的进程
        DepotDownloader::Get().SendSteamGuardCode(steamGuardCode_);
        loginState_ = LoginState::LoggingIn;
        memset(steamGuardCode_, 0, sizeof(steamGuardCode_));
        // 不需要重新启动下载，进程会继续运行
    } else {
        // Normal login
        if (strlen(username_) == 0) {
            loginError_ = u8"请输入用户名";
            return;
        }
        loginState_ = LoginState::LoggingIn;
        loginError_.clear();

        // Save credentials if requested
        if (rememberLogin_) {
            SaveCredentials();
        }

        // 如果是启动游戏的登录，执行回调并关闭弹窗
        if (showLaunchLogin_) {
            loginState_ = LoginState::None;
            showLaunchLogin_ = false;
            if (launchCallback_) {
                auto callback = launchCallback_;
                launchCallback_ = nullptr;
                callback();
            }
            return;
        }

        // Start download with credentials
        DoStartDownload();
    }
}

void GameInstallPanel::LoadSavedCredentials() {
    // Load from simple file (in production, use Windows Credential Manager)
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
        std::string path = std::string(appdata) + "\\SteamForge\\credentials.dat";
        std::ifstream f(path);
        if (f.is_open()) {
            std::string user, pass;
            std::getline(f, user);
            std::getline(f, pass);
            strncpy(username_, user.c_str(), sizeof(username_) - 1);
            // Note: password is not stored for security
        }
    }
}

void GameInstallPanel::SaveCredentials() {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
        std::string dir = std::string(appdata) + "\\SteamForge";
        CreateDirectoryA(dir.c_str(), nullptr);
        std::string path = dir + "\\credentials.dat";
        std::ofstream f(path);
        if (f.is_open()) {
            f << username_ << "\n";
            // Note: not storing password for security
        }
    }
}

bool GameInstallPanel::HasSavedCredentials() {
    // 先加载保存的凭据
    LoadSavedCredentials();
    return username_[0] != '\0';
}

void GameInstallPanel::ShowLoginForLaunch(std::function<void()> onSuccess) {
    // 先检查是否已有凭据
    LoadSavedCredentials();
    if (username_[0] != '\0') {
        // 已有凭据，直接执行回调
        if (onSuccess) onSuccess();
        return;
    }

    // 没有凭据，显示登录弹窗
    showLaunchLogin_ = true;
    launchCallback_ = onSuccess;
    loginState_ = LoginState::ShowDialog;
    loginError_.clear();
    memset(password_, 0, sizeof(password_));
    memset(steamGuardCode_, 0, sizeof(steamGuardCode_));
}

} // namespace sf
