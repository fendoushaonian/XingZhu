#pragma once
#include <string>
#include <vector>
#include <ctime>

namespace sf {

// 用户本地库 - 存储用户手动添加的游戏（如免费游戏）
// 这些游戏不在 Steam 库中，但用户想要管理它们

struct UserLibraryGame {
    std::string appId;
    std::string name;
    std::string type;       // "game", "dlc", "software"
    std::string source;     // "free", "manual", etc.
    time_t addedTime = 0;   // 添加时间
    bool installed = false; // 是否已安装
};

class UserLibrary {
public:
    static UserLibrary& Get();

    // 添加游戏到用户库
    void AddGame(const std::string& appId, const std::string& name,
                 const std::string& type = "game", const std::string& source = "free");

    // 移除游戏
    void RemoveGame(const std::string& appId);

    // 检查游戏是否在用户库中
    bool HasGame(const std::string& appId) const;

    // 获取游戏信息
    const UserLibraryGame* GetGame(const std::string& appId) const;

    // 设置游戏安装状态
    void SetInstalled(const std::string& appId, bool installed);

    // 获取所有游戏
    const std::vector<UserLibraryGame>& GetAllGames() const { return games_; }

    // 保存/加载
    void Save();
    void Load();

private:
    UserLibrary();
    std::vector<UserLibraryGame> games_;
    std::string filePath_;
};

} // namespace sf
