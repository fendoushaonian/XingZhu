#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <ctime>

namespace sf {

// 本地安装记录 - 记录通过 SteamForge 安装的游戏
class InstallRecord {
public:
    static InstallRecord& Get();

    // 添加已安装的游戏（带名称）
    void AddInstalled(const std::string& appId, const std::string& installPath, const std::string& gameName = "");

    // 移除安装记录
    void RemoveInstalled(const std::string& appId);

    // 检查游戏是否已安装（通过 SteamForge）
    bool IsInstalled(const std::string& appId) const;

    // 获取游戏安装路径
    std::string GetInstallPath(const std::string& appId) const;

    // 获取游戏名称
    std::string GetGameName(const std::string& appId) const;

    // 获取所有已安装的 appId
    std::vector<std::string> GetAllInstalled() const;

    // 记录游戏启动时间
    void RecordGameLaunch(const std::string& appId);

    // 获取最后游玩时间（返回 0 表示从未玩过）
    time_t GetLastPlayedTime(const std::string& appId) const;

    // 获取按最后游玩时间排序的游戏列表（最近玩的在前）
    std::vector<std::string> GetRecentlyPlayed(int maxCount = 10) const;

    // 保存到文件
    void Save();

    // 从文件加载
    void Load();

    // 获取安装信息结构（供外部使用）
    struct InstallInfo {
        std::string appId;
        std::string installPath;
        std::string gameName;
        time_t lastPlayed = 0;
    };

    // 获取所有安装记录
    const std::vector<InstallInfo>& GetAllRecords() const { return records_; }

private:
    InstallRecord();

    std::vector<InstallInfo> records_;
    std::string filePath_;
};

// 便捷函数
inline bool IsGameInstalledBySteamForge(const std::string& appId) {
    return InstallRecord::Get().IsInstalled(appId);
}

} // namespace sf
