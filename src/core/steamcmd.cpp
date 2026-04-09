#include "core/steamcmd.h"
#include "core/steam_detect.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <shlobj.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

namespace fs = std::filesystem;

namespace sf {

// Helper to log to file for debugging
static void LogToFile(const std::string& msg) {
    static std::string logPath;
    if (logPath.empty()) {
        char appdata[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
            logPath = std::string(appdata) + "\\SteamForge\\steamcmd_debug.log";
        } else {
            logPath = "steamcmd_debug.log";
        }
    }
    std::ofstream f(logPath, std::ios::app);
    if (f) {
        f << msg;
        f.flush();
    }
    OutputDebugStringA(msg.c_str());
}

SteamCMD& SteamCMD::Get() {
    static SteamCMD instance;
    return instance;
}

SteamCMD::SteamCMD() {
    // Default location: next to Steam or in AppData
    auto& st = GetSteamStatus();
    if (!st.steamPath.empty()) {
        steamcmdDir_ = st.steamPath + "\\steamcmd";
    } else {
        char appdata[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
            steamcmdDir_ = std::string(appdata) + "\\SteamForge\\steamcmd";
        } else {
            steamcmdDir_ = "C:\\SteamCMD";
        }
    }
    steamcmdPath_ = steamcmdDir_ + "\\steamcmd.exe";
}

SteamCMD::~SteamCMD() {
    CancelDownload();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

bool SteamCMD::IsAvailable() const {
    DWORD attr = GetFileAttributesA(steamcmdPath_.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

void SteamCMD::EnsureInstalled(std::function<void(bool, const std::string&)> callback) {
    if (IsAvailable()) {
        if (callback) callback(true, "");
        return;
    }

    if (settingUp_) {
        if (callback) callback(false, "Already setting up");
        return;
    }

    setupCallback_ = callback;
    settingUp_ = true;

    std::thread([this]() {
        DownloadSteamCMD();
    }).detach();
}

void SteamCMD::DownloadSteamCMD() {
    // Create directory
    fs::create_directories(steamcmdDir_);

    std::string zipPath = steamcmdDir_ + "\\steamcmd.zip";
    const char* url = "https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip";

    // Download zip file
    HRESULT hr = URLDownloadToFileA(nullptr, url, zipPath.c_str(), 0, nullptr);
    if (FAILED(hr)) {
        settingUp_ = false;
        if (setupCallback_) setupCallback_(false, "Failed to download SteamCMD");
        return;
    }

    // Extract using PowerShell (hidden window)
    std::string psCmd = "powershell -WindowStyle Hidden -Command \"Expand-Archive -Path '" + zipPath +
                        "' -DestinationPath '" + steamcmdDir_ + "' -Force\"";

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(nullptr, const_cast<char*>(psCmd.c_str()),
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 60000); // Wait up to 60 seconds
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Clean up zip
    DeleteFileA(zipPath.c_str());

    if (!IsAvailable()) {
        settingUp_ = false;
        if (setupCallback_) setupCallback_(false, "Failed to extract SteamCMD");
        return;
    }

    // Run once to let it update itself (hidden)
    std::string initCmd = "\"" + steamcmdPath_ + "\" +quit";

    STARTUPINFOA si2 = { sizeof(STARTUPINFOA) };
    si2.dwFlags = STARTF_USESHOWWINDOW;
    si2.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi2 = {};

    if (CreateProcessA(nullptr, const_cast<char*>(initCmd.c_str()),
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, steamcmdDir_.c_str(), &si2, &pi2)) {
        WaitForSingleObject(pi2.hProcess, 120000); // Wait up to 2 minutes for update
        CloseHandle(pi2.hProcess);
        CloseHandle(pi2.hThread);
    }

    settingUp_ = false;
    if (setupCallback_) setupCallback_(true, "");
}

void SteamCMD::StartDownload(const std::string& appId,
                              const std::string& installDir,
                              const std::string& username,
                              const std::string& password,
                              ProgressCallback progressCb) {
    if (downloading_) return;
    if (!IsAvailable()) return;

    downloading_ = true;
    cancelRequested_ = false;
    needsSteamGuard_ = false;
    progressCallback_ = progressCb;

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    workerThread_ = std::thread(&SteamCMD::RunDownloadWorker, this,
                                 appId, installDir, username, password);
}

void SteamCMD::CancelDownload() {
    cancelRequested_ = true;
    if (hProcess_) {
        TerminateProcess(hProcess_, 1);
    }
}

void SteamCMD::RequestFreeLicense(const std::string& appId,
                                   const std::string& username,
                                   const std::string& password,
                                   std::function<void(bool, const std::string&)> callback) {
    if (requestingLicense_ || downloading_) {
        if (callback) callback(false, "Another operation in progress");
        return;
    }
    if (!IsAvailable()) {
        if (callback) callback(false, "SteamCMD not available");
        return;
    }

    requestingLicense_ = true;
    cancelRequested_ = false;
    licenseCallback_ = callback;

    std::thread([this, appId, username, password]() {
        RunLicenseRequestWorker(appId, username, password);
    }).detach();
}

void SteamCMD::RunLicenseRequestWorker(const std::string& appId,
                                        const std::string& username,
                                        const std::string& password) {
    LogToFile("[SteamCMD] Requesting free license for app " + appId + "\n");

    // Create a script file for SteamCMD
    std::string scriptPath = steamcmdDir_ + "\\steamforge_license.txt";
    {
        std::ofstream script(scriptPath);
        if (!script) {
            requestingLicense_ = false;
            if (licenseCallback_) licenseCallback_(false, "Failed to create script file");
            return;
        }

        script << "@ShutdownOnFailedCommand 0\n";
        script << "@NoPromptForPassword 1\n";

        if (username.empty() || username == "anonymous") {
            script << "login anonymous\n";
        } else {
            script << "login " << username << " " << password;
            if (!steamGuardCode_.empty()) {
                script << " " << steamGuardCode_;
                steamGuardCode_.clear();
            }
            script << "\n";
        }

        // Request free license
        script << "app_license_request " << appId << "\n";
        script << "quit\n";
    }

    std::string cmdLine = "\"" + steamcmdPath_ + "\" +runscript \"" + scriptPath + "\"";
    LogToFile("[SteamCMD] License request command: " + cmdLine + "\n");

    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        requestingLicense_ = false;
        if (licenseCallback_) licenseCallback_(false, "Failed to create pipe");
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()),
                        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, steamcmdDir_.c_str(), &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        requestingLicense_ = false;
        if (licenseCallback_) licenseCallback_(false, "Failed to start SteamCMD");
        return;
    }

    hProcess_ = pi.hProcess;
    CloseHandle(hWritePipe);

    // Read output and look for success/failure
    char buffer[4096];
    DWORD bytesRead;
    std::string allOutput;
    bool success = false;
    std::string errorMsg;

    while (!cancelRequested_) {
        DWORD available = 0;
        if (!PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &available, nullptr)) {
            break;
        }

        if (available > 0) {
            if (ReadFile(hReadPipe, buffer, (std::min)(available, (DWORD)sizeof(buffer) - 1),
                         &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                allOutput += buffer;
                LogToFile(std::string("[SteamCMD License] ") + buffer);
            }
        } else {
            DWORD exitCode;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                // Read remaining
                while (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
                    if (ReadFile(hReadPipe, buffer, (std::min)(available, (DWORD)sizeof(buffer) - 1),
                                 &bytesRead, nullptr) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        allOutput += buffer;
                        LogToFile(std::string("[SteamCMD License] ") + buffer);
                    } else {
                        break;
                    }
                }
                break;
            }
            Sleep(50);
        }
    }

    WaitForSingleObject(pi.hProcess, 5000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    hProcess_ = nullptr;

    // Check output for success indicators
    // SteamCMD outputs "License acquired" or similar on success
    // Or "already owns" if user already has the license
    if (allOutput.find("License acquired") != std::string::npos ||
        allOutput.find("already owns") != std::string::npos) {
        success = true;
    } else if (allOutput.find("ERROR!") != std::string::npos ||
               allOutput.find("Failed getting license") != std::string::npos ||
               allOutput.find("FAILED") != std::string::npos ||
               allOutput.find("Invalid") != std::string::npos) {
        success = false;
        errorMsg = "LICENSE_REQUIRED";  // Special marker for UI to handle
    } else {
        // Even if we don't see explicit success, if exit code is 0, assume success
        // But only if we see "Logged in OK" which means login worked
        if (allOutput.find("Logged in OK") != std::string::npos) {
            // Login worked but no license message - might already own it
            success = true;
        } else {
            success = (exitCode == 0);
            if (!success) {
                errorMsg = "License request failed (exit code " + std::to_string(exitCode) + ")";
            }
        }
    }

    LogToFile("[SteamCMD] License request finished, success=" + std::to_string(success) + "\n");

    requestingLicense_ = false;
    if (licenseCallback_) licenseCallback_(success, errorMsg);
}

void SteamCMD::RunDownloadWorker(const std::string& appId,
                                  const std::string& installDir,
                                  const std::string& username,
                                  const std::string& password) {
    // Create a script file for SteamCMD to execute
    // This is more reliable than passing commands on command line
    std::string scriptPath = steamcmdDir_ + "\\steamforge_script.txt";
    {
        std::ofstream script(scriptPath);
        if (!script) {
            downloading_ = false;
            SteamCMDProgress p;
            p.finished = true;
            p.error = "Failed to create script file";
            if (progressCallback_) progressCallback_(p);
            return;
        }

        // Write login command
        if (username.empty() || username == "anonymous") {
            script << "@ShutdownOnFailedCommand 1\n";
            script << "@NoPromptForPassword 1\n";
            script << "login anonymous\n";
        } else {
            script << "@ShutdownOnFailedCommand 1\n";
            script << "@NoPromptForPassword 1\n";
            script << "login " << username << " " << password;
            if (!steamGuardCode_.empty()) {
                script << " " << steamGuardCode_;
                steamGuardCode_.clear();
            }
            script << "\n";
        }

        // Write download commands
        script << "force_install_dir \"" << installDir << "\"\n";
        script << "app_update " << appId << " validate\n";
        script << "quit\n";
    }

    // Build command line to run script
    std::string cmdLine = "\"" + steamcmdPath_ + "\" +runscript \"" + scriptPath + "\"";

    // Log without password for security
    LogToFile("[SteamCMD] Starting with script: " + scriptPath + "\n");
    LogToFile("[SteamCMD] Working dir: " + steamcmdDir_ + "\n");
    OutputDebugStringA(("[SteamCMD] Starting with script: " + scriptPath + "\n").c_str());

    // Create pipes for output capture
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        downloading_ = false;
        SteamCMDProgress p;
        p.finished = true;
        p.error = "Failed to create pipe";
        if (progressCallback_) progressCallback_(p);
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // Start process
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()),
                        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, steamcmdDir_.c_str(), &si, &pi)) {
        DWORD err = GetLastError();
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "[SteamCMD] CreateProcess failed, error=%lu\n", err);
        OutputDebugStringA(errMsg);
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        downloading_ = false;
        SteamCMDProgress p;
        p.finished = true;
        p.error = "Failed to start SteamCMD";
        if (progressCallback_) progressCallback_(p);
        return;
    }

    LogToFile("[SteamCMD] Process created successfully\n");
    OutputDebugStringA("[SteamCMD] Process created successfully\n");

    hProcess_ = pi.hProcess;
    CloseHandle(hWritePipe);

    // Read output
    char buffer[4096];
    DWORD bytesRead;
    std::string lineBuffer;

    LogToFile("[SteamCMD] Worker started, reading output...\n");
    OutputDebugStringA("[SteamCMD] Worker started, reading output...\n");

    while (!cancelRequested_) {
        DWORD available = 0;
        if (!PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &available, nullptr)) {
            OutputDebugStringA("[SteamCMD] PeekNamedPipe failed\n");
            break;
        }

        if (available > 0) {
            if (ReadFile(hReadPipe, buffer, (std::min)(available, (DWORD)sizeof(buffer) - 1),
                         &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                lineBuffer += buffer;

                // Process complete lines (handle both \n and \r as line endings)
                // SteamCMD uses \r to update progress on the same line
                size_t pos;
                while ((pos = lineBuffer.find_first_of("\r\n")) != std::string::npos) {
                    std::string line = lineBuffer.substr(0, pos);
                    // Skip empty lines
                    if (!line.empty()) {
                        // Remove any trailing \r or \n
                        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                            line.pop_back();
                        }
                        if (!line.empty()) {
                            ParseOutput(line);
                        }
                    }
                    // Skip consecutive \r\n
                    size_t next = pos + 1;
                    while (next < lineBuffer.size() &&
                           (lineBuffer[next] == '\r' || lineBuffer[next] == '\n')) {
                        next++;
                    }
                    lineBuffer = lineBuffer.substr(next);
                }
            }
        } else {
            // Check if process is still running
            DWORD exitCode;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                OutputDebugStringA("[SteamCMD] Process exited, reading remaining output...\n");
                LogToFile("[SteamCMD] Process exited, reading remaining output...\n");
                // Read any remaining data in the pipe after process exits
                DWORD remaining = 0;
                while (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &remaining, nullptr) && remaining > 0) {
                    if (ReadFile(hReadPipe, buffer, (std::min)(remaining, (DWORD)sizeof(buffer) - 1),
                                 &bytesRead, nullptr) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        lineBuffer += buffer;
                    } else {
                        break;
                    }
                }
                break;
            }
            Sleep(50);  // Reduced sleep for faster response
        }
    }

    // Process remaining buffer - handle all lines
    while (!lineBuffer.empty()) {
        size_t pos = lineBuffer.find_first_of("\r\n");
        if (pos == std::string::npos) {
            // No more line endings, process the rest as a single line
            std::string line = lineBuffer;
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }
            if (!line.empty()) {
                ParseOutput(line);
            }
            break;
        }
        std::string line = lineBuffer.substr(0, pos);
        if (!line.empty()) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }
            if (!line.empty()) {
                ParseOutput(line);
            }
        }
        size_t next = pos + 1;
        while (next < lineBuffer.size() && (lineBuffer[next] == '\r' || lineBuffer[next] == '\n')) {
            next++;
        }
        lineBuffer = lineBuffer.substr(next);
    }

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, 5000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    hProcess_ = nullptr;

    downloading_ = false;

    SteamCMDProgress p;
    p.finished = true;
    p.success = (exitCode == 0 && !cancelRequested_);
    if (cancelRequested_) {
        p.error = "Cancelled";
    } else if (exitCode != 0) {
        p.error = "Download failed (exit code " + std::to_string(exitCode) + ")";
    }
    if (progressCallback_) progressCallback_(p);
}

void SteamCMD::ParseOutput(const std::string& line) {
    SteamCMDProgress p;

    // Debug: log all output
    LogToFile("[SteamCMD] " + line + "\n");
    OutputDebugStringA(("[SteamCMD] " + line + "\n").c_str());

    // Check for Steam Guard prompt
    if (line.find("Steam Guard") != std::string::npos ||
        line.find("Two-factor") != std::string::npos ||
        line.find("two factor") != std::string::npos) {
        needsSteamGuard_ = true;
        p.state = "steamguard";
        if (progressCallback_) progressCallback_(p);
        return;
    }

    // Check for login failure
    if (line.find("FAILED") != std::string::npos &&
        (line.find("login") != std::string::npos || line.find("Login") != std::string::npos)) {
        p.state = "error";
        p.error = "Login failed";
        if (progressCallback_) progressCallback_(p);
        return;
    }

    // Parse progress format 1: "Update state (0x61) downloading, progress: 45.23 (1234567890 / 2730000000)"
    std::regex progressRegex1(
        R"(Update state \(0x\w+\) (\w+), progress: ([\d.]+) \((\d+) / (\d+)\))");
    std::smatch match;
    if (std::regex_search(line, match, progressRegex1)) {
        p.state = match[1].str();
        p.percentage = std::stod(match[2].str());
        p.bytesDownloaded = std::stoull(match[3].str());
        p.totalBytes = std::stoull(match[4].str());
        if (progressCallback_) progressCallback_(p);
        return;
    }

    // Parse progress format 2: " Update state (0x5) verifying install, progress: 0.00 (0 / 0)"
    // Some states don't have byte counts
    std::regex progressRegex2(R"(Update state \(0x\w+\) ([^,]+), progress: ([\d.]+))");
    if (std::regex_search(line, match, progressRegex2)) {
        p.state = match[1].str();
        p.percentage = std::stod(match[2].str());
        // Try to extract bytes if present
        std::regex bytesRegex(R"(\((\d+) / (\d+)\))");
        std::smatch bytesMatch;
        if (std::regex_search(line, bytesMatch, bytesRegex)) {
            p.bytesDownloaded = std::stoull(bytesMatch[1].str());
            p.totalBytes = std::stoull(bytesMatch[2].str());
        }
        if (progressCallback_) progressCallback_(p);
        return;
    }

    // Parse progress format 3: "Downloading update (X of Y)..."
    std::regex downloadingRegex(R"(Downloading update \((\d+) of (\d+)\))");
    if (std::regex_search(line, match, downloadingRegex)) {
        p.state = "downloading";
        int current = std::stoi(match[1].str());
        int total = std::stoi(match[2].str());
        p.percentage = total > 0 ? (current * 100.0 / total) : 0;
        if (progressCallback_) progressCallback_(p);
        return;
    }

    // Check for "Logged in OK" - login success
    if (line.find("Logged in OK") != std::string::npos ||
        line.find("Waiting for user info") != std::string::npos) {
        p.state = "logged_in";
        if (progressCallback_) progressCallback_(p);
        return;
    }

    // Check for success
    if (line.find("Success!") != std::string::npos ||
        line.find("fully installed") != std::string::npos ||
        line.find("already up to date") != std::string::npos) {
        p.state = "completed";
        p.percentage = 100.0;
        p.finished = true;
        p.success = true;
        if (progressCallback_) progressCallback_(p);
        return;
    }

    // Check for error
    if (line.find("Error!") != std::string::npos ||
        line.find("ERROR!") != std::string::npos) {
        p.state = "error";
        p.error = line;
        if (progressCallback_) progressCallback_(p);
    }
}

} // namespace sf
