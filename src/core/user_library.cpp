#include "user_library.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <shlobj.h>

namespace fs = std::filesystem;

namespace sf {

UserLibrary& UserLibrary::Get() {
    static UserLibrary instance;
    return instance;
}

UserLibrary::UserLibrary() {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
        filePath_ = std::string(appdata) + "\\SteamForge\\user_library.txt";
        fs::create_directories(std::string(appdata) + "\\SteamForge");
    } else {
        filePath_ = "user_library.txt";
    }
    Load();
}

void UserLibrary::AddGame(const std::string& appId, const std::string& name,
                          const std::string& type, const std::string& source) {
    // 检查是否已存在
    for (auto& g : games_) {
        if (g.appId == appId) {
            // 更新信息
            g.name = name;
            g.type = type;
            g.source = source;
            Save();
            return;
        }
    }

    UserLibraryGame game;
    game.appId = appId;
    game.name = name;
    game.type = type;
    game.source = source;
    game.addedTime = time(nullptr);
    game.installed = false;
    games_.push_back(game);
    Save();
}

void UserLibrary::RemoveGame(const std::string& appId) {
    games_.erase(
        std::remove_if(games_.begin(), games_.end(),
            [&](const UserLibraryGame& g) { return g.appId == appId; }),
        games_.end());
    Save();
}

bool UserLibrary::HasGame(const std::string& appId) const {
    for (const auto& g : games_) {
        if (g.appId == appId) return true;
    }
    return false;
}

const UserLibraryGame* UserLibrary::GetGame(const std::string& appId) const {
    for (const auto& g : games_) {
        if (g.appId == appId) return &g;
    }
    return nullptr;
}

void UserLibrary::SetInstalled(const std::string& appId, bool installed) {
    for (auto& g : games_) {
        if (g.appId == appId) {
            g.installed = installed;
            Save();
            return;
        }
    }
}

void UserLibrary::Save() {
    std::ofstream f(filePath_);
    if (!f) return;
    // 格式: appId|name|type|source|addedTime|installed
    for (const auto& g : games_) {
        f << g.appId << "|" << g.name << "|" << g.type << "|"
          << g.source << "|" << g.addedTime << "|" << (g.installed ? 1 : 0) << "\n";
    }
}

void UserLibrary::Load() {
    games_.clear();
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

        if (parts.size() >= 6) {
            UserLibraryGame game;
            game.appId = parts[0];
            game.name = parts[1];
            game.type = parts[2];
            game.source = parts[3];
            try { game.addedTime = std::stoll(parts[4]); } catch (...) { game.addedTime = 0; }
            game.installed = (parts[5] == "1");
            games_.push_back(game);
        }
    }
}

} // namespace sf
