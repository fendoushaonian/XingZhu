#include "core/steam_emu.h"
#include "utils/logger.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <shlobj.h>

namespace fs = std::filesystem;

namespace sf {

// UTF-8 字符串转 wstring (用于 filesystem 操作支持中文路径)
static std::wstring Utf8ToWideEmu(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

SteamEmu& SteamEmu::Get() {
    static SteamEmu instance;
    return instance;
}

SteamEmu::SteamEmu() {
    // 存放在 AppData/Local/SteamForge/emu
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        emuDir_ = std::string(path) + "\\SteamForge\\emu";
    }
}

bool SteamEmu::IsAvailable() const {
    if (emuDir_.empty()) return false;
    // 检查关键文件是否存在
    return fs::exists(emuDir_ + "\\steam_api.dll") &&
           fs::exists(emuDir_ + "\\steam_api64.dll");
}

bool SteamEmu::GameNeedsSteamDRM(const std::string& gameDir) {
    try {
        std::wstring gameDirW = Utf8ToWideEmu(gameDir);
        if (gameDirW.empty()) return false;
        for (const auto& entry : fs::recursive_directory_iterator(
                 gameDirW, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                // 使用 wstring 获取文件名，避免中文路径问题
                std::wstring fnameW = entry.path().filename().wstring();
                std::wstring fnameLowerW = fnameW;
                std::transform(fnameLowerW.begin(), fnameLowerW.end(), fnameLowerW.begin(), ::towlower);
                if (fnameLowerW == L"steam_api.dll" || fnameLowerW == L"steam_api64.dll") {
                    return true;
                }
            }
        }
    } catch (...) {}
    return false;
}

bool SteamEmu::ApplyToGame(const std::string& gameDir, const std::string& appId, const std::string& gameName) {
    if (!IsAvailable()) {
        Log(LogLevel::Error, "SteamEmu: Emulator not available");
        return false;
    }

    Log(LogLevel::Info, "SteamEmu: Applying to game %s at %s", appId.c_str(), gameDir.c_str());

    bool applied = false;
    std::wstring gameDirW = Utf8ToWideEmu(gameDir);
    if (gameDirW.empty()) {
        Log(LogLevel::Error, "SteamEmu: Failed to convert game path to wstring");
        return false;
    }

    try {
        // 遍历游戏目录，找到所有 steam_api.dll 和 steam_api64.dll
        for (const auto& entry : fs::recursive_directory_iterator(
                 gameDirW, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            // 使用 wstring 获取文件名，避免中文路径问题
            std::wstring fnameW = entry.path().filename().wstring();
            std::wstring fnameLowerW = fnameW;
            std::transform(fnameLowerW.begin(), fnameLowerW.end(), fnameLowerW.begin(), ::towlower);

            bool is32 = (fnameLowerW == L"steam_api.dll");
            bool is64 = (fnameLowerW == L"steam_api64.dll");

            if (!is32 && !is64) continue;

            // 使用 wstring 路径来支持中文
            std::wstring dllPathW = entry.path().wstring();
            std::wstring dllDirW = entry.path().parent_path().wstring();
            std::wstring backupPathW = dllPathW + L".original";

            // 转换为 UTF-8 用于日志 - 使用 WideCharToMultiByte 而不是 .string()
            std::string dllPathUtf8;
            {
                int len = WideCharToMultiByte(CP_UTF8, 0, dllPathW.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    dllPathUtf8.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, dllPathW.c_str(), -1, &dllPathUtf8[0], len, nullptr, nullptr);
                }
            }

            // 备份原始 DLL（如果还没备份）
            if (!fs::exists(backupPathW)) {
                try {
                    fs::copy_file(dllPathW, backupPathW, fs::copy_options::overwrite_existing);
                    Log(LogLevel::Info, "SteamEmu: Backed up %s", dllPathUtf8.c_str());
                } catch (const std::exception& e) {
                    Log(LogLevel::Warn, "SteamEmu: Failed to backup %s: %s", dllPathUtf8.c_str(), e.what());
                }
            }

            // 复制 emulator DLL - 使用 wstring 路径
            std::wstring srcDllW = Utf8ToWideEmu(emuDir_ + "\\" + (is64 ? "steam_api64.dll" : "steam_api.dll"));
            try {
                fs::copy_file(srcDllW, dllPathW, fs::copy_options::overwrite_existing);
                Log(LogLevel::Info, "SteamEmu: Replaced %s", dllPathUtf8.c_str());
                applied = true;
            } catch (const std::exception& e) {
                Log(LogLevel::Error, "SteamEmu: Failed to copy to %s: %s", dllPathUtf8.c_str(), e.what());
                continue;
            }

            // 创建 steam_settings 目录和配置文件 - 使用 wstring
            std::wstring settingsDirW = dllDirW + L"\\steam_settings";
            try {
                fs::create_directories(settingsDirW);
            } catch (...) {}

            // steam_appid.txt - 使用 wstring 路径
            {
                std::wstring appidPathW = settingsDirW + L"\\steam_appid.txt";
                std::ofstream ofs(appidPathW);
                if (ofs.is_open()) {
                    ofs << appId;
                    ofs.close();
                }
            }

            // 也在 DLL 同目录创建 steam_appid.txt
            {
                std::wstring appidPathW = dllDirW + L"\\steam_appid.txt";
                std::ofstream ofs(appidPathW);
                if (ofs.is_open()) {
                    ofs << appId;
                    ofs.close();
                }
            }

            // configs.user.ini - 基本配置
            {
                std::wstring configPathW = settingsDirW + L"\\configs.user.ini";
                std::ofstream ofs(configPathW);
                if (ofs.is_open()) {
                    ofs << "[user::general]\n";
                    ofs << "account_name=SteamForge\n";
                    ofs << "account_steamid=76561198000000001\n";
                    ofs << "language=schinese\n";
                    ofs.close();
                }
            }

            // configs.app.ini - 应用配置
            {
                std::wstring configPathW = settingsDirW + L"\\configs.app.ini";
                std::ofstream ofs(configPathW);
                if (ofs.is_open()) {
                    ofs << "[app::general]\n";
                    ofs << "build_id=0\n";
                    ofs << "enable_overlay=0\n";
                    ofs.close();
                }
            }

            // configs.main.ini - 主配置
            {
                std::wstring configPathW = settingsDirW + L"\\configs.main.ini";
                std::ofstream ofs(configPathW);
                if (ofs.is_open()) {
                    ofs << "[main::general]\n";
                    ofs << "enable_experimental_overlay=0\n";
                    ofs << "enable_console=0\n";
                    ofs.close();
                }
            }
        }
    } catch (const std::exception& e) {
        Log(LogLevel::Error, "SteamEmu: Error applying to game: %s", e.what());
        return false;
    }

    if (applied) {
        Log(LogLevel::Info, "SteamEmu: Successfully applied to game %s", appId.c_str());
    } else {
        Log(LogLevel::Warn, "SteamEmu: No steam_api DLLs found in %s", gameDir.c_str());
    }

    return applied;
}

void SteamEmu::EnsureInstalled(std::function<void(bool, const std::string&)> callback) {
    if (IsAvailable()) {
        if (callback) callback(true, "");
        return;
    }

    if (settingUp_) {
        if (callback) callback(false, "Already setting up");
        return;
    }

    settingUp_ = true;
    std::thread([this, callback]() {
        DownloadEmulator(callback);
        settingUp_ = false;
    }).detach();
}

void SteamEmu::DownloadEmulator(std::function<void(bool, const std::string&)> callback) {
    Log(LogLevel::Info, "SteamEmu: Downloading Goldberg Steam Emulator...");

    // 创建目录
    try {
        fs::create_directories(emuDir_);
    } catch (const std::exception& e) {
        if (callback) callback(false, std::string("Failed to create directory: ") + e.what());
        return;
    }

    // 使用 GitLab 的 zip 格式（更兼容）
    std::string downloadUrl = "https://gitlab.com/Mr_Goldberg/goldberg_emulator/-/jobs/artifacts/master/download?job=build_windows";
    std::string zipPath = emuDir_ + "\\goldberg.zip";

    // 使用 curl 下载
    std::string curlCmd = "curl -L -o \"" + zipPath + "\" \"" + downloadUrl + "\"";
    Log(LogLevel::Info, "SteamEmu: Downloading from GitLab...");

    FILE* pipe = _popen(curlCmd.c_str(), "r");
    if (!pipe) {
        if (callback) callback(false, "Failed to run curl");
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {}
    int exitCode = _pclose(pipe);

    if (exitCode != 0 || !fs::exists(zipPath)) {
        Log(LogLevel::Error, "SteamEmu: Download failed");
        if (callback) callback(false, "Download failed");
        return;
    }

    Log(LogLevel::Info, "SteamEmu: Downloaded, extracting...");

    // 使用 PowerShell 解压 zip
    std::string extractCmd = "powershell -Command \"Expand-Archive -Force -Path '" + zipPath + "' -DestinationPath '" + emuDir_ + "'\"";
    Log(LogLevel::Info, "SteamEmu: Extracting with PowerShell...");

    pipe = _popen(extractCmd.c_str(), "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe)) {}
        _pclose(pipe);
    }

    // 清理下载的压缩包
    try { fs::remove(zipPath); } catch (...) {}

    // Goldberg 的文件在 release 子目录中，需要移动到根目录
    std::string releaseDir = emuDir_ + "\\release";
    if (fs::exists(releaseDir)) {
        try {
            // 复制 DLL 文件到根目录
            if (fs::exists(releaseDir + "\\steam_api.dll")) {
                fs::copy_file(releaseDir + "\\steam_api.dll", emuDir_ + "\\steam_api.dll", fs::copy_options::overwrite_existing);
            }
            if (fs::exists(releaseDir + "\\steam_api64.dll")) {
                fs::copy_file(releaseDir + "\\steam_api64.dll", emuDir_ + "\\steam_api64.dll", fs::copy_options::overwrite_existing);
            }
        } catch (const std::exception& e) {
            Log(LogLevel::Error, "SteamEmu: Failed to copy DLLs: %s", e.what());
        }
    }

    if (IsAvailable()) {
        Log(LogLevel::Info, "SteamEmu: Installation complete");
        if (callback) callback(true, "");
    } else {
        Log(LogLevel::Error, "SteamEmu: Installation failed - DLLs not found");
        if (callback) callback(false, "Extraction failed - steam_api.dll not found");
    }
}

} // namespace sf
