#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sf {

// 下载进度
struct DepotProgress {
    std::string state;          // "connecting", "logging_in", "downloading", "verifying", "completed", "error"
    std::string currentFile;    // 当前文件
    double percentage = 0.0;    // 0-100
    uint64_t bytesDownloaded = 0;
    uint64_t totalBytes = 0;
    float speed = 0.0f;         // bytes/sec
    bool finished = false;
    bool success = false;
    std::string error;
    bool isVerifying = false;   // 是否在验证阶段
    double verifyPercentage = 0.0;  // 验证进度 0-100
    bool sizeIsEstimated = false;   // 字节数是估算的
};

using DepotProgressCallback = std::function<void(const DepotProgress&)>;

class DepotDownloader {
public:
    static DepotDownloader& Get();

    // 检查 DepotDownloader 是否可用
    bool IsAvailable() const;

    // 确保已安装（自动下载）
    void EnsureInstalled(std::function<void(bool success, const std::string& error)> callback);

    // 是否正在安装
    bool IsSettingUp() const { return settingUp_; }

    // 开始下载游戏
    void StartDownload(const std::string& appId,
                       const std::string& installDir,
                       const std::string& username,
                       const std::string& password,
                       DepotProgressCallback callback);

    // 取消下载
    void CancelDownload();

    // 是否正在下载
    bool IsDownloading() const { return downloading_; }

    // 设置 Steam Guard 码并发送给进程
    void SetSteamGuardCode(const std::string& code);

    // 发送 Steam Guard 码给正在运行的进程
    void SendSteamGuardCode(const std::string& code);

    // 是否需要 Steam Guard
    bool NeedsSteamGuard() const { return needsSteamGuard_; }

    // 获取路径
    const std::string& GetPath() const { return exePath_; }

private:
    DepotDownloader();
    ~DepotDownloader();

    void DownloadDepotDownloader();
    void RunDownloadWorker(const std::string& appId,
                           const std::string& installDir,
                           const std::string& username,
                           const std::string& password);
    void RunDownloadWorkerWithCode(const std::string& appId,
                                   const std::string& installDir,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& steamGuardCode);
    void ParseOutput(const std::string& line);

    std::string baseDir_;       // DepotDownloader 所在目录
    std::string exePath_;       // DepotDownloader.exe 路径

    std::atomic<bool> settingUp_{false};
    std::atomic<bool> downloading_{false};
    std::atomic<bool> cancelRequested_{false};
    std::atomic<bool> needsSteamGuard_{false};

    std::string steamGuardCode_;

    // 保存上次下载的参数，用于 Steam Guard 重试
    std::string lastAppId_;
    std::string lastInstallDir_;
    std::string lastUsername_;
    std::string lastPassword_;

    std::thread workerThread_;
    std::mutex mtx_;
    DepotProgressCallback callback_;
    std::function<void(bool, const std::string&)> setupCallback_;

    HANDLE hProcess_ = nullptr;
    HANDLE hStdinWrite_ = nullptr;  // 用于发送 Steam Guard 验证码

    // 进度追踪
    uint64_t lastBytes_ = 0;
    double lastTime_ = 0.0;
    float currentSpeed_ = 0.0f;
    uint64_t totalSize_ = 0;
    uint64_t downloadedSize_ = 0;
    bool isVerifying_ = false;  // 当前是否在验证阶段
    bool sizeIsEstimated_ = false;  // 字节数是估算的
    double lastPercentage_ = 0.0;  // 上次百分比，用于计算速度
    double downloadStartTime_ = 0.0;  // 下载开始时间
};

} // namespace sf
