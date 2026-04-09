#include "game_process.h"
#include <tlhelp32.h>

namespace sf {

GameProcessManager& GameProcessManager::Get() {
    static GameProcessManager instance;
    return instance;
}

void GameProcessManager::RegisterProcess(const std::string& appId, DWORD processId) {
    std::lock_guard<std::mutex> lock(mtx_);
    runningGames_[appId] = processId;
}

bool GameProcessManager::IsGameRunning(const std::string& appId) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = runningGames_.find(appId);
    if (it == runningGames_.end()) return false;

    DWORD pid = it->second;
    if (pid == 0) return false;

    // 检查进程是否仍在运行
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) {
        // 进程已退出，移除记录
        runningGames_.erase(it);
        return false;
    }

    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
        CloseHandle(hProcess);
        runningGames_.erase(it);
        return false;
    }

    CloseHandle(hProcess);
    return true;
}

bool GameProcessManager::StopGame(const std::string& appId) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = runningGames_.find(appId);
    if (it == runningGames_.end()) return false;

    DWORD pid = it->second;
    if (pid == 0) return false;

    // 尝试终止进程
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) {
        runningGames_.erase(it);
        return false;
    }

    BOOL result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);

    if (result) {
        runningGames_.erase(it);
        return true;
    }

    return false;
}

DWORD GameProcessManager::GetProcessId(const std::string& appId) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = runningGames_.find(appId);
    if (it == runningGames_.end()) return 0;
    return it->second;
}

void GameProcessManager::CleanupExitedProcesses() {
    std::lock_guard<std::mutex> lock(mtx_);

    for (auto it = runningGames_.begin(); it != runningGames_.end(); ) {
        DWORD pid = it->second;
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

        bool shouldRemove = false;
        if (!hProcess) {
            shouldRemove = true;
        } else {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                shouldRemove = true;
            }
            CloseHandle(hProcess);
        }

        if (shouldRemove) {
            it = runningGames_.erase(it);
        } else {
            ++it;
        }
    }
}

void GameProcessManager::StopAllGames() {
    std::lock_guard<std::mutex> lock(mtx_);

    for (auto& pair : runningGames_) {
        DWORD pid = pair.second;
        if (pid == 0) continue;

        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }

    runningGames_.clear();
}

} // namespace sf
