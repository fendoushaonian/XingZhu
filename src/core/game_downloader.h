#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstdint>

namespace sf {

// 游戏文件信息
struct GameFile {
    std::string path;           // 相对路径 e.g. "game/bin/game.exe"
    std::string url;            // 下载URL
    int64_t size = 0;           // 文件大小
    std::string sha256;         // SHA256校验和
    bool compressed = false;    // 是否压缩
    int64_t compressedSize = 0; // 压缩后大小
};

// 游戏清单
struct GameManifest {
    std::string appId;
    std::string name;
    std::string version;
    int64_t totalSize = 0;          // 总大小
    int64_t totalCompressed = 0;    // 压缩后总大小
    std::vector<GameFile> files;
    std::string installScript;      // 安装后执行的脚本
};

// 下载进度
struct DownloadProgress {
    std::string appId;
    std::string gameName;
    std::string currentFile;
    int64_t bytesDownloaded = 0;
    int64_t bytesTotal = 0;
    int filesCompleted = 0;
    int filesTotal = 0;
    float speed = 0.0f;             // bytes/sec
    float progress = 0.0f;          // 0.0 - 1.0
    float eta = 0.0f;               // 剩余秒数
    std::string status;             // "downloading", "verifying", "extracting", "done", "error"
    std::string error;
};

// 下载配置
struct DownloadConfig {
    std::string installPath;        // 安装目录
    int maxConnections = 8;         // 最大并发连接数
    int maxRetries = 3;             // 最大重试次数
    bool verifyAfterDownload = true;// 下载后校验
    bool createShortcut = true;     // 创建快捷方式
    std::string cdnBaseUrl;         // CDN基础URL (可选，覆盖manifest中的URL)
};

// 进度回调
using GameDownloadCallback = std::function<void(const DownloadProgress&)>;

class GameDownloader {
public:
    GameDownloader();
    ~GameDownloader();

    // 设置CDN服务器列表 (会自动选择最快的)
    void SetCDNServers(const std::vector<std::string>& servers);

    // 从服务器获取游戏清单
    bool FetchManifest(const std::string& appId, GameManifest& outManifest);

    // 开始下载游戏
    bool StartDownload(const GameManifest& manifest, const DownloadConfig& config, GameDownloadCallback callback);

    // 暂停下载
    void PauseDownload();

    // 恢复下载
    void ResumeDownload();

    // 取消下载
    void CancelDownload();

    // 获取当前进度
    DownloadProgress GetProgress() const;

    // 是否正在下载
    bool IsDownloading() const { return downloading_.load(); }

    // 是否暂停
    bool IsPaused() const { return paused_.load(); }

    // 验证已安装的游戏文件
    bool VerifyInstallation(const std::string& installPath, const GameManifest& manifest,
                            std::vector<std::string>& corruptedFiles);

    // 修复损坏的文件
    bool RepairFiles(const std::string& installPath, const GameManifest& manifest,
                     const std::vector<std::string>& files, GameDownloadCallback callback);

private:
    // 下载单个文件 (支持断点续传)
    bool DownloadFile(const GameFile& file, const std::string& destPath,
                      const std::string& cdnBase, int64_t& bytesDownloaded);

    // 下载文件块
    bool DownloadChunk(const std::string& url, const std::string& destPath,
                       int64_t startByte, int64_t endByte, int64_t& bytesDownloaded);

    // 校验文件SHA256
    bool VerifyFileSHA256(const std::string& filePath, const std::string& expectedHash);

    // 解压文件 (如果是压缩的)
    bool DecompressFile(const std::string& srcPath, const std::string& destPath);

    // 选择最快的CDN
    std::string SelectFastestCDN();

    // 更新进度
    void UpdateProgress(const std::string& status, const std::string& currentFile = "");

    // 下载线程函数
    void DownloadThreadFunc();

    // CDN服务器列表
    std::vector<std::string> cdnServers_;
    std::string activeCdn_;

    // 下载状态
    std::atomic<bool> downloading_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> cancelled_{false};

    // 当前下载任务
    GameManifest currentManifest_;
    DownloadConfig currentConfig_;
    GameDownloadCallback progressCallback_;

    // 进度数据
    mutable std::mutex progressMtx_;
    DownloadProgress progress_;
    int64_t lastSpeedCheckBytes_ = 0;
    float lastSpeedCheckTime_ = 0.0f;

    // 下载线程
    std::thread downloadThread_;

    // 断点续传状态文件
    std::string GetResumeFilePath(const std::string& appId) const;
    void SaveResumeState();
    bool LoadResumeState();
};

// 全局下载器实例
GameDownloader& GetGameDownloader();

} // namespace sf
