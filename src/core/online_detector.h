#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_set>

namespace sf {

// 联机游戏检测器 - 防止在联机游戏中使用修改器
class OnlineDetector {
public:
    static OnlineDetector& Get();

    // 检测进程是否为联机游戏
    bool IsOnlineGame(DWORD processId);
    bool IsOnlineGame(const std::string& processName);

    // 获取检测原因
    std::string GetDetectionReason() const { return detectionReason_; }

    // 强制终止游戏
    bool ForceTerminate(DWORD processId);

    // 添加/移除白名单（用户确认的单机游戏）
    void AddToWhitelist(const std::string& processName);
    void RemoveFromWhitelist(const std::string& processName);
    bool IsWhitelisted(const std::string& processName);

    // 已知联机游戏列表
    static const std::vector<std::string>& GetKnownOnlineGames();

private:
    OnlineDetector() = default;

    // 检测方法
    bool CheckKnownOnlineGame(const std::string& processName);
    bool CheckNetworkConnections(DWORD processId);
    bool CheckLoadedModules(DWORD processId);
    bool CheckAntiCheat(DWORD processId);

    std::string detectionReason_;
    std::unordered_set<std::string> whitelist_;
};

} // namespace sf
