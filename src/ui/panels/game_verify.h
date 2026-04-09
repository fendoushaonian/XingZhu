#pragma once
#include "imgui.h"
#include "ui/widgets/game_card.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

namespace sf {

class GameVerifyPanel {
public:
    ~GameVerifyPanel();
    void Render(float winW, float winH);
    void Open(const GameInfo& game);
    bool IsOpen() const { return open_; }

private:
    bool open_ = false;
    float anim_ = 0.0f;
    GameInfo game_;
    bool scanStarted_ = false;

    // Thread-safe progress
    std::thread scanThread_;
    std::atomic<bool> scanning_{false};
    std::atomic<bool> scanDone_{false};
    std::atomic<float> progress_{0.0f};
    std::atomic<int> filesScanned_{0};
    std::atomic<int> totalFiles_{0};
    std::atomic<int64_t> bytesScanned_{0};

    std::mutex mtx_;
    std::string currentFile_;

    // Results (written by thread, read after scanDone_)
    std::string installPath_;
    std::string buildId_;
    std::string lastUpdated_;
    std::string stateDesc_;
    int stateFlags_ = 0;
    int64_t sizeOnDisk_ = 0;
    int fileCount_ = 0;
    int folderCount_ = 0;
    int64_t actualSize_ = 0;
    bool installFound_ = false;
    bool verified_ = false;
    float sizePct_ = 0.0f;

    struct FileTypeInfo {
        std::string ext;
        int count;
        int64_t size;
    };
    std::vector<FileTypeInfo> fileTypes_;

    void StartScan();
    void ScanWorker();
};

} // namespace sf
