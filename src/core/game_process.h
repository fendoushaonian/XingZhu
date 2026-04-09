#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sf {

// 跟踪运行中的游戏进程
class GameProcessManager {
public:
    static GameProcessManager& Get();

    // 记录启动的游戏进程
    void RegisterProcess(const std::string& appId, DWORD processId);

    // 检查游戏是否正在运行
    bool IsGameRunning(const std::string& appId);

    // 停止游戏
    bool StopGame(const std::string& appId);

    // 停止所有游戏（程序退出时调用）
    void StopAllGames();

    // 获取游戏进程ID（0表示未运行）
    DWORD GetProcessId(const std::string& appId);

    // 清理已退出的进程
    void CleanupExitedProcesses();

private:
    GameProcessManager() = default;
    ~GameProcessManager() = default;

    std::unordered_map<std::string, DWORD> runningGames_;
    std::mutex mtx_;
};

} // namespace sf
