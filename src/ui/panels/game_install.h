#pragma once
#include "imgui.h"
#include "ui/widgets/game_card.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

namespace sf {

// Represents a Steam library folder for install location selection
struct InstallLocation {
    std::string path;       // e.g. "D:\SteamLibrary"
    std::string label;      // e.g. "D: (SteamLibrary)"
    int64_t freeBytes = 0;
    int64_t totalBytes = 0;
};

// Login state for Steam authentication
enum class LoginState {
    None,           // Not logged in, no dialog
    ShowDialog,     // Show login dialog
    LoggingIn,      // Attempting login
    NeedSteamGuard, // Need 2FA code
    LoggedIn,       // Successfully logged in
    Failed          // Login failed
};

// Download state tracked by DepotDownloader
struct DownloadState {
    std::string appId;
    std::string gameName;
    std::string installPath;    // 安装路径
    int64_t bytesDownloaded = 0;
    int64_t bytesToDownload = 0;
    int64_t bytesStaged = 0;
    int64_t bytesToStage = 0;
    float progress = 0.0f;      // 0..1
    float speed = 0.0f;         // bytes/sec (estimated)
    float peakSpeed = 0.0f;
    float eta = 0.0f;           // seconds remaining
    int stateFlags = 0;
    bool active = false;        // download in progress
    bool completed = false;     // download finished
    bool failed = false;
    bool isVerifying = false;   // 是否在验证阶段
    bool verifyOnly = false;    // 仅验证完成（无需下载）
    bool sizeIsEstimated = false; // 字节数是估算的，不应显示
    float verifyProgress = 0.0f; // 验证进度 0..1
    std::string statusText;     // "downloading", "verifying", etc.
    std::string errorMsg;
    std::string currentPhase;   // "verify" 或 "download"
};

class GameInstallPanel {
public:
    ~GameInstallPanel();

    // Open install dialog for a game
    // gameSize: required disk space in GB (0 = unknown)
    void Open(const std::string& appId, const std::string& gameName, float gameSize = 0.0f);

    // Call every frame from application
    void Render(float winW, float winH);

    // Render download progress on the right panel (replaces library content)
    // Returns true if a download is active and panel was rendered
    bool RenderDownloadPanel(float x, float y, float width, float height);

    bool IsDialogOpen() const { return showDialog_; }
    bool IsDownloading() const { return download_.active && !download_.completed; }
    bool HasActiveDownload() const { return download_.active; }
    std::string GetDownloadingAppId() const { return download_.appId; }

    // 检查是否有保存的 Steam 凭据
    bool HasSavedCredentials();

    // 显示登录弹窗，登录成功后执行回调
    // 用于启动游戏前的登录检查
    void ShowLoginForLaunch(std::function<void()> onSuccess);

    // 检查是否正在显示启动游戏的登录弹窗
    bool IsShowingLaunchLogin() const { return showLaunchLogin_; }

private:
    // Dialog state
    bool showDialog_ = false;
    float dialogAnim_ = 0.0f;
    std::string pendingAppId_;
    std::string pendingGameName_;
    float pendingGameSize_ = 0.0f;  // Required disk space in GB
    int selectedLocation_ = 0;
    bool createDesktopShortcut_ = true;
    bool createStartMenuShortcut_ = true;
    std::vector<InstallLocation> locations_;

    // Login state
    LoginState loginState_ = LoginState::None;
    char username_[128] = {};
    char password_[128] = {};
    char steamGuardCode_[16] = {};
    bool rememberLogin_ = true;
    std::string loginError_;

    // 启动游戏登录状态
    bool showLaunchLogin_ = false;
    std::function<void()> launchCallback_;

    // Download state (updated from DepotDownloader callback)
    DownloadState download_;
    std::mutex mtx_;
    int64_t lastBytes_ = 0;
    float lastCheckTime_ = 0.0f;
    float downloadStartTime_ = 0.0f;

    // History for speed graph
    static constexpr int SPEED_HISTORY_SIZE = 120;
    float speedHistory_[SPEED_HISTORY_SIZE] = {};
    int speedHistoryIdx_ = 0;

    void CollectInstallLocations();
    void StartInstall();
    void DoStartDownload();  // 实际开始下载
    void DoActualDownload(const std::string& user, const std::string& pass);  // 执行下载
    void OnDownloadProgress(const struct DepotProgress& progress);
    void RenderLoginDialog(float winW, float winH);
    void TryLogin();
    void LoadSavedCredentials();
    void SaveCredentials();
};

} // namespace sf
