#pragma once
#include "imgui.h"
#include <string>
#include <atomic>
#include <mutex>
#include <thread>

namespace sf {

struct UninstallState {
    std::string appId;
    std::string gameName;
    std::string installPath;
    int64_t totalFiles = 0;
    int64_t deletedFiles = 0;
    int64_t totalBytes = 0;
    int64_t deletedBytes = 0;
    float progress = 0.0f;
    bool active = false;
    bool completed = false;
    bool failed = false;
    bool confirmed = false;
    bool needsNotify = false;  // 需要通知主线程更新状态
    std::string currentFile;
    std::string errorMsg;
};

class GameUninstallPanel {
public:
    ~GameUninstallPanel();

    void Open(const std::string& appId, const std::string& gameName);
    void Render(float winW, float winH);
    bool IsOpen() const { return showDialog_; }

private:
    bool showDialog_ = false;
    float dialogAnim_ = 0.0f;
    UninstallState state_;
    std::mutex mtx_;
    std::thread worker_;

    void StartUninstall();
    void UninstallWorker();
};

} // namespace sf
