#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sf {

// Download progress callback
struct SteamCMDProgress {
    std::string state;          // "downloading", "verifying", "preallocating"
    double percentage = 0.0;
    uint64_t bytesDownloaded = 0;
    uint64_t totalBytes = 0;
    bool finished = false;
    bool success = false;
    std::string error;
};

using ProgressCallback = std::function<void(const SteamCMDProgress&)>;

class SteamCMD {
public:
    static SteamCMD& Get();

    // Check if SteamCMD is available
    bool IsAvailable() const;

    // Get SteamCMD path
    const std::string& GetPath() const { return steamcmdPath_; }

    // Download and setup SteamCMD (async, calls callback when done)
    void EnsureInstalled(std::function<void(bool success, const std::string& error)> callback);

    // Check if currently downloading SteamCMD itself
    bool IsSettingUp() const { return settingUp_; }

    // Start game download (async)
    // username/password empty = anonymous login (only works for free games/dedicated servers)
    void StartDownload(const std::string& appId,
                       const std::string& installDir,
                       const std::string& username,
                       const std::string& password,
                       ProgressCallback progressCb);

    // Cancel current download
    void CancelDownload();

    // Check if download is in progress
    bool IsDownloading() const { return downloading_; }

    // Set Steam Guard code for 2FA
    void SetSteamGuardCode(const std::string& code) { steamGuardCode_ = code; }

    // Check if waiting for Steam Guard
    bool NeedsSteamGuard() const { return needsSteamGuard_; }

    // Request free license for a game (async)
    // This is needed before downloading free games/demos
    // Callback: (success, errorMsg)
    void RequestFreeLicense(const std::string& appId,
                            const std::string& username,
                            const std::string& password,
                            std::function<void(bool, const std::string&)> callback);

    // Check if license request is in progress
    bool IsRequestingLicense() const { return requestingLicense_; }

private:
    SteamCMD();
    ~SteamCMD();

    void DownloadSteamCMD();
    void RunDownloadWorker(const std::string& appId,
                           const std::string& installDir,
                           const std::string& username,
                           const std::string& password);
    void RunLicenseRequestWorker(const std::string& appId,
                                  const std::string& username,
                                  const std::string& password);
    void ParseOutput(const std::string& line);

    std::string steamcmdPath_;
    std::string steamcmdDir_;

    std::atomic<bool> settingUp_{false};
    std::atomic<bool> downloading_{false};
    std::atomic<bool> requestingLicense_{false};
    std::atomic<bool> cancelRequested_{false};
    std::atomic<bool> needsSteamGuard_{false};

    std::string steamGuardCode_;

    std::thread workerThread_;
    std::mutex mtx_;
    ProgressCallback progressCallback_;
    std::function<void(bool, const std::string&)> setupCallback_;
    std::function<void(bool, const std::string&)> licenseCallback_;

    HANDLE hProcess_ = nullptr;
};

} // namespace sf
