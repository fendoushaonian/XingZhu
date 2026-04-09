#include "install_record.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <shlobj.h>
#include <windows.h>

namespace fs = std::filesystem;

namespace sf {

// UTF-8 字符串转 wstring (用于 filesystem 操作)
static std::wstring Utf8ToWideIR(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// 安全检查路径是否存在（支持中文路径）
static bool PathExistsSafeIR(const std::string& path) {
    if (path.empty()) return false;  // 空路径直接返回 false
    try {
        std::wstring wpath = Utf8ToWideIR(path);
        if (wpath.empty()) return false;
        return fs::exists(wpath) && fs::is_directory(wpath);
    } catch (...) {
        return false;
    }
}

InstallRecord& InstallRecord::Get() {
    static InstallRecord instance;
    return instance;
}

InstallRecord::InstallRecord() {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
        filePath_ = std::string(appdata) + "\\SteamForge\\installed_games.txt";
        fs::create_directories(std::string(appdata) + "\\SteamForge");
    } else {
        filePath_ = "installed_games.txt";
    }
    Load();
}

void InstallRecord::AddInstalled(const std::string& appId, const std::string& installPath, const std::string& gameName) {
    // 检查是否已存在
    for (auto& r : records_) {
        if (r.appId == appId) {
            r.installPath = installPath;
            if (!gameName.empty()) r.gameName = gameName;
            Save();
            return;
        }
    }
    InstallInfo info;
    info.appId = appId;
    info.installPath = installPath;
    info.gameName = gameName;
    info.lastPlayed = 0;
    records_.push_back(info);
    Save();
}

void InstallRecord::RemoveInstalled(const std::string& appId) {
    records_.erase(
        std::remove_if(records_.begin(), records_.end(),
            [&](const InstallInfo& r) { return r.appId == appId; }),
        records_.end());
    Save();
}

bool InstallRecord::IsInstalled(const std::string& appId) const {
    for (const auto& r : records_) {
        if (r.appId == appId) {
            // 验证安装路径是否仍然存在（使用安全的中文路径检查）
            if (PathExistsSafeIR(r.installPath)) {
                return true;
            }
        }
    }
    return false;
}

std::string InstallRecord::GetInstallPath(const std::string& appId) const {
    for (const auto& r : records_) {
        if (r.appId == appId) {
            return r.installPath;
        }
    }
    return "";
}

std::string InstallRecord::GetGameName(const std::string& appId) const {
    for (const auto& r : records_) {
        if (r.appId == appId) {
            return r.gameName;
        }
    }
    return "";
}

std::vector<std::string> InstallRecord::GetAllInstalled() const {
    std::vector<std::string> result;
    for (const auto& r : records_) {
        result.push_back(r.appId);
    }
    return result;
}

void InstallRecord::RecordGameLaunch(const std::string& appId) {
    for (auto& r : records_) {
        if (r.appId == appId) {
            r.lastPlayed = time(nullptr);
            Save();
            return;
        }
    }
    // 游戏不在记录中（Steam 安装的游戏），添加新记录
    InstallInfo info;
    info.appId = appId;
    info.installPath = "";  // Steam 游戏没有本地安装路径
    info.gameName = "";
    info.lastPlayed = time(nullptr);
    records_.push_back(info);
    Save();
}

time_t InstallRecord::GetLastPlayedTime(const std::string& appId) const {
    for (const auto& r : records_) {
        if (r.appId == appId) {
            return r.lastPlayed;
        }
    }
    return 0;
}

std::vector<std::string> InstallRecord::GetRecentlyPlayed(int maxCount) const {
    // 复制并按最后游玩时间排序
    std::vector<InstallInfo> sorted = records_;
    std::sort(sorted.begin(), sorted.end(), [](const InstallInfo& a, const InstallInfo& b) {
        return a.lastPlayed > b.lastPlayed;  // 最近的在前
    });

    std::vector<std::string> result;
    for (const auto& r : sorted) {
        if (r.lastPlayed > 0) {  // 只包含玩过的游戏
            result.push_back(r.appId);
            if ((int)result.size() >= maxCount) break;
        }
    }
    return result;
}

void InstallRecord::Save() {
    std::ofstream f(filePath_);
    if (!f) return;
    for (const auto& r : records_) {
        // 格式: appId|installPath|lastPlayed|gameName
        f << r.appId << "|" << r.installPath << "|" << r.lastPlayed << "|" << r.gameName << "\n";
    }
}

void InstallRecord::Load() {
    records_.clear();
    std::ifstream f(filePath_);
    if (!f) return;

    std::string line;
    while (std::getline(f, line)) {
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) {
            parts.push_back(part);
        }

        if (parts.size() >= 2) {
            InstallInfo info;
            info.appId = parts[0];
            info.installPath = parts[1];
            info.lastPlayed = 0;
            info.gameName = "";

            if (parts.size() >= 3) {
                try { info.lastPlayed = std::stoll(parts[2]); } catch (...) { info.lastPlayed = 0; }
            }
            if (parts.size() >= 4) {
                info.gameName = parts[3];
            }
            records_.push_back(info);
        }
    }
}

} // namespace sf
