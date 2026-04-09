#include "depot_downloader.h"
#include "steam_detect.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <shlobj.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

namespace fs = std::filesystem;

namespace sf {

// 日志输出
static void LogDD(const std::string& msg) {
    static std::string logPath;
    if (logPath.empty()) {
        char appdata[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
            logPath = std::string(appdata) + "\\SteamForge\\depot_debug.log";
            fs::create_directories(std::string(appdata) + "\\SteamForge");
        } else {
            logPath = "depot_debug.log";
        }
    }
    std::ofstream f(logPath, std::ios::app);
    if (f) {
        f << msg;
        f.flush();
    }
    OutputDebugStringA(msg.c_str());
}

static double GetTimeNow() {
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

DepotDownloader& DepotDownloader::Get() {
    static DepotDownloader instance;
    return instance;
}

DepotDownloader::DepotDownloader() {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata))) {
        baseDir_ = std::string(appdata) + "\\SteamForge\\DepotDownloader";
    } else {
        baseDir_ = "C:\\DepotDownloader";
    }
    exePath_ = baseDir_ + "\\DepotDownloader.exe";
}

DepotDownloader::~DepotDownloader() {
    CancelDownload();
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

bool DepotDownloader::IsAvailable() const {
    DWORD attr = GetFileAttributesA(exePath_.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

void DepotDownloader::EnsureInstalled(std::function<void(bool, const std::string&)> callback) {
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
        DownloadDepotDownloader();
    }).detach();
}

void DepotDownloader::DownloadDepotDownloader() {
    LogDD("[DepotDownloader] Starting download...\n");

    fs::create_directories(baseDir_);

    // 下载最新 release
    // GitHub API: https://api.github.com/repos/SteamRE/DepotDownloader/releases/latest
    // 直接用固定版本链接更可靠
    std::string zipPath = baseDir_ + "\\depotdownloader.zip";
    const char* url = "https://github.com/SteamRE/DepotDownloader/releases/latest/download/DepotDownloader-windows-x64.zip";

    LogDD("[DepotDownloader] Downloading from: " + std::string(url) + "\n");

    HRESULT hr = URLDownloadToFileA(nullptr, url, zipPath.c_str(), 0, nullptr);
    if (FAILED(hr)) {
        LogDD("[DepotDownloader] Download failed\n");
        settingUp_ = false;
        if (setupCallback_) setupCallback_(false, "Failed to download DepotDownloader");
        return;
    }

    LogDD("[DepotDownloader] Download complete, extracting...\n");

    // 解压
    std::string psCmd = "powershell -WindowStyle Hidden -Command \"Expand-Archive -Path '" + zipPath +
                        "' -DestinationPath '" + baseDir_ + "' -Force\"";

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(nullptr, const_cast<char*>(psCmd.c_str()),
                       nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 60000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    DeleteFileA(zipPath.c_str());

    if (!IsAvailable()) {
        LogDD("[DepotDownloader] Extract failed\n");
        settingUp_ = false;
        if (setupCallback_) setupCallback_(false, "Failed to extract DepotDownloader");
        return;
    }

    LogDD("[DepotDownloader] Setup complete\n");
    settingUp_ = false;
    if (setupCallback_) setupCallback_(true, "");
}

void DepotDownloader::StartDownload(const std::string& appId,
                                     const std::string& installDir,
                                     const std::string& username,
                                     const std::string& password,
                                     DepotProgressCallback callback) {
    if (downloading_) return;
    if (!IsAvailable()) return;

    downloading_ = true;
    cancelRequested_ = false;
    needsSteamGuard_ = false;
    callback_ = callback;

    // 重置进度
    lastBytes_ = 0;
    lastTime_ = GetTimeNow();
    downloadStartTime_ = lastTime_;
    currentSpeed_ = 0.0f;
    totalSize_ = 0;
    downloadedSize_ = 0;
    isVerifying_ = false;
    sizeIsEstimated_ = false;
    lastPercentage_ = 0.0;

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    workerThread_ = std::thread(&DepotDownloader::RunDownloadWorker, this,
                                 appId, installDir, username, password);
}

void DepotDownloader::CancelDownload() {
    cancelRequested_ = true;
    if (hStdinWrite_) {
        CloseHandle(hStdinWrite_);
        hStdinWrite_ = nullptr;
    }
    if (hProcess_) {
        TerminateProcess(hProcess_, 1);
    }
}

void DepotDownloader::SetSteamGuardCode(const std::string& code) {
    steamGuardCode_ = code;
}

void DepotDownloader::SendSteamGuardCode(const std::string& code) {
    if (code.empty()) {
        LogDD("[DepotDownloader] Cannot send Steam Guard code - empty code\n");
        return;
    }

    LogDD("[DepotDownloader] Received Steam Guard code: " + code + "\n");

    // 保存验证码，然后重新启动下载
    steamGuardCode_ = code;
    needsSteamGuard_ = false;

    // 终止当前进程
    if (hProcess_) {
        TerminateProcess(hProcess_, 0);
        CloseHandle(hProcess_);
        hProcess_ = nullptr;
    }
    if (hStdinWrite_) {
        CloseHandle(hStdinWrite_);
        hStdinWrite_ = nullptr;
    }

    // 重新启动下载（使用保存的凭据）
    if (!lastAppId_.empty() && !lastUsername_.empty()) {
        LogDD("[DepotDownloader] Restarting download with Steam Guard code\n");
        std::thread([this]() {
            RunDownloadWorkerWithCode(lastAppId_, lastInstallDir_, lastUsername_, lastPassword_, steamGuardCode_);
        }).detach();
    }
}

void DepotDownloader::RunDownloadWorker(const std::string& appId,
                                         const std::string& installDir,
                                         const std::string& username,
                                         const std::string& password) {
    LogDD("[DepotDownloader] Starting download for app " + appId + "\n");

    // 保存参数用于 Steam Guard 重试
    lastAppId_ = appId;
    lastInstallDir_ = installDir;
    lastUsername_ = username;
    lastPassword_ = password;

    RunDownloadWorkerWithCode(appId, installDir, username, password, "");
}

// Helper: UTF-8 std::string → wstring
static std::wstring Utf8ToWideDD(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

void DepotDownloader::RunDownloadWorkerWithCode(const std::string& appId,
                                                 const std::string& installDir,
                                                 const std::string& username,
                                                 const std::string& password,
                                                 const std::string& steamGuardCode) {
    LogDD("[DepotDownloader] RunDownloadWorkerWithCode for app " + appId + "\n");

    // 构建命令行 (使用 wstring 以支持中文路径)
    std::wostringstream cmd;

    // 转换路径为 wstring
    std::wstring wExePath = Utf8ToWideDD(exePath_);
    std::wstring wInstallDir = Utf8ToWideDD(installDir);
    std::wstring wAppId = Utf8ToWideDD(appId);
    std::wstring wUsername = Utf8ToWideDD(username);
    std::wstring wPassword = Utf8ToWideDD(password);
    std::wstring wSteamGuardCode = Utf8ToWideDD(steamGuardCode);

    // 如果有 Steam Guard 验证码，使用 cmd /c echo 来发送
    if (!steamGuardCode.empty()) {
        cmd << L"cmd /c \"echo " << wSteamGuardCode << L" | ";
        cmd << L"\"" << wExePath << L"\"";
    } else {
        cmd << L"\"" << wExePath << L"\"";
    }

    cmd << L" -app " << wAppId;
    cmd << L" -dir \"" << wInstallDir << L"\"";

    if (!username.empty()) {
        cmd << L" -username " << wUsername;
        if (!password.empty()) {
            cmd << L" -password " << wPassword;
        }
        cmd << L" -remember-password";
    }

    if (!steamGuardCode.empty()) {
        cmd << L"\"";  // 关闭 cmd /c 的引号
    }

    std::wstring cmdLineW = cmd.str();
    LogDD("[DepotDownloader] Command: " + installDir + " (app " + appId + ")\n");

    // 创建输出管道（用于读取进程输出）
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        downloading_ = false;
        DepotProgress p;
        p.state = "error";
        p.error = "Failed to create pipe";
        p.finished = true;
        if (callback_) callback_(p);
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // 创建输入管道（用于发送 Steam Guard 验证码）
    HANDLE hStdinRead, hStdinWrite;
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        downloading_ = false;
        DepotProgress p;
        p.state = "error";
        p.error = "Failed to create stdin pipe";
        p.finished = true;
        if (callback_) callback_(p);
        return;
    }
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    // 启动进程 (使用 CreateProcessW 支持中文路径)
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    std::wstring wBaseDir = Utf8ToWideDD(baseDir_);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &cmdLineW[0],
                        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, wBaseDir.c_str(), &si, &pi)) {
        DWORD err = GetLastError();
        LogDD("[DepotDownloader] CreateProcess failed: " + std::to_string(err) + "\n");
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        downloading_ = false;
        DepotProgress p;
        p.state = "error";
        p.error = "Failed to start DepotDownloader";
        p.finished = true;
        if (callback_) callback_(p);
        return;
    }

    LogDD("[DepotDownloader] Process started\n");
    hProcess_ = pi.hProcess;
    hStdinWrite_ = hStdinWrite;  // 保存输入管道句柄
    CloseHandle(hWritePipe);
    CloseHandle(hStdinRead);  // 关闭读端，只保留写端

    // 读取输出
    char buffer[4096];
    DWORD bytesRead;
    std::string lineBuffer;

    while (!cancelRequested_) {
        DWORD available = 0;
        if (!PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &available, nullptr)) {
            break;
        }

        if (available > 0) {
            DWORD toRead = (std::min)(available, (DWORD)(sizeof(buffer) - 1));
            if (ReadFile(hReadPipe, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                lineBuffer += buffer;

                // 处理完整行
                size_t pos;
                while ((pos = lineBuffer.find_first_of("\r\n")) != std::string::npos) {
                    std::string line = lineBuffer.substr(0, pos);
                    if (!line.empty()) {
                        // 去除首尾空白
                        size_t start = line.find_first_not_of(" \t\r\n");
                        size_t end = line.find_last_not_of(" \t\r\n");
                        if (start != std::string::npos && end != std::string::npos) {
                            line = line.substr(start, end - start + 1);
                            if (!line.empty()) {
                                ParseOutput(line);
                            }
                        }
                    }
                    // 跳过连续换行
                    size_t next = pos + 1;
                    while (next < lineBuffer.size() &&
                           (lineBuffer[next] == '\r' || lineBuffer[next] == '\n')) {
                        next++;
                    }
                    lineBuffer = lineBuffer.substr(next);
                }
            }
        } else {
            // 如果需要 Steam Guard，等待用户输入（不要检查进程退出）
            if (needsSteamGuard_) {
                Sleep(100);
                continue;
            }

            // 检查进程是否结束
            DWORD exitCode;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                // 读取剩余数据
                while (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
                    DWORD toRead = (std::min)(available, (DWORD)(sizeof(buffer) - 1));
                    if (ReadFile(hReadPipe, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        lineBuffer += buffer;
                    } else {
                        break;
                    }
                }
                break;
            }
            Sleep(50);
        }
    }

    // 处理剩余缓冲
    if (!lineBuffer.empty()) {
        size_t start = lineBuffer.find_first_not_of(" \t\r\n");
        size_t end = lineBuffer.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            std::string line = lineBuffer.substr(start, end - start + 1);
            if (!line.empty()) {
                ParseOutput(line);
            }
        }
    }

    WaitForSingleObject(pi.hProcess, 5000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (hStdinWrite_) {
        CloseHandle(hStdinWrite_);
        hStdinWrite_ = nullptr;
    }
    hProcess_ = nullptr;

    downloading_ = false;

    LogDD("[DepotDownloader] Process finished with exit code " + std::to_string(exitCode) + "\n");

    DepotProgress p;
    p.finished = true;
    p.success = (exitCode == 0 && !cancelRequested_);
    if (cancelRequested_) {
        p.state = "error";
        p.error = "Cancelled";
    } else if (exitCode != 0) {
        p.state = "error";
        p.error = "Download failed (exit code " + std::to_string(exitCode) + ")";
    } else {
        p.state = "completed";
        p.percentage = 100.0;
    }
    if (callback_) callback_(p);
}

void DepotDownloader::ParseOutput(const std::string& line) {
    LogDD("[DD] " + line + "\n");

    DepotProgress p;
    p.bytesDownloaded = downloadedSize_;
    p.totalBytes = totalSize_;
    p.speed = currentSpeed_;
    p.isVerifying = isVerifying_;
    p.sizeIsEstimated = sizeIsEstimated_;

    // 连接 Steam
    if (line.find("Connecting to Steam") != std::string::npos) {
        p.state = "connecting";
        if (callback_) callback_(p);
        return;
    }

    // 登录
    if (line.find("Logging") != std::string::npos && line.find("into Steam") != std::string::npos) {
        p.state = "logging_in";
        if (callback_) callback_(p);
        return;
    }

    // Steam Guard (检测多种格式)
    if (line.find("Steam Guard") != std::string::npos ||
        line.find("STEAM GUARD") != std::string::npos ||
        line.find("steam guard") != std::string::npos ||
        line.find("two-factor") != std::string::npos ||
        line.find("Two-factor") != std::string::npos ||
        line.find("auth code") != std::string::npos ||
        line.find("enter the auth code") != std::string::npos) {
        needsSteamGuard_ = true;
        p.state = "steamguard";
        if (callback_) callback_(p);
        return;
    }

    // 登录失败
    if (line.find("Failed to authenticate") != std::string::npos ||
        line.find("Invalid Password") != std::string::npos) {
        p.state = "error";
        p.error = "Login failed";
        if (callback_) callback_(p);
        return;
    }

    // 处理 depot
    if (line.find("Processing depot") != std::string::npos) {
        p.state = "downloading";
        p.currentFile = "Processing depot...";
        if (callback_) callback_(p);
        return;
    }

    // 下载 manifest
    if (line.find("Downloading depot") != std::string::npos && line.find("manifest") != std::string::npos) {
        p.state = "downloading";
        p.currentFile = "Downloading manifest...";
        if (callback_) callback_(p);
        return;
    }

    // 预分配文件
    if (line.find("Pre-allocating") != std::string::npos) {
        p.state = "downloading";
        size_t pos = line.find("Pre-allocating");
        if (pos != std::string::npos) {
            p.currentFile = line.substr(pos + 15);
        }
        if (callback_) callback_(p);
        return;
    }

    // 验证文件开始 - 进入验证阶段
    if (line.find("Validating") != std::string::npos) {
        isVerifying_ = true;
        p.state = "verifying";
        p.isVerifying = true;
        size_t pos = line.find("Validating");
        if (pos != std::string::npos) {
            p.currentFile = line.substr(pos + 11);
        }
        if (callback_) callback_(p);
        return;
    }

    // 下载开始 - 退出验证阶段
    if (line.find("Downloading") != std::string::npos && line.find("manifest") == std::string::npos) {
        isVerifying_ = false;
        p.state = "downloading";
        p.isVerifying = false;
        p.percentage = 0.0;  // 重置进度
        if (callback_) callback_(p);
        return;
    }

    // 进度: "  0.00% path/to/file" 或 " 50.32% path/to/file"
    std::regex progressRegex(R"(^\s*([\d.]+)%\s+(.+)$)");
    std::smatch match;
    if (std::regex_search(line, match, progressRegex)) {
        double pct = std::stod(match[1].str());
        p.currentFile = match[2].str();

        if (isVerifying_) {
            // 验证阶段
            p.state = "verifying";
            p.isVerifying = true;
            p.verifyPercentage = pct;
            p.percentage = pct;
        } else {
            // 下载阶段
            p.state = "downloading";
            p.isVerifying = false;
            p.percentage = pct;

            // 基于百分比变化计算速度
            double now = GetTimeNow();
            double dt = now - lastTime_;
            if (dt > 0.3 && pct > lastPercentage_) {
                double pctDelta = pct - lastPercentage_;
                // 假设总大小为 100 单位，每个百分比 = 1 单位
                // 速度 = 百分比变化 / 时间 (单位: %/秒)
                // 转换为字节/秒需要知道总大小，但我们可以用百分比速度来估算

                // 如果有 totalSize_，计算真实字节速度
                if (totalSize_ > 0 && !sizeIsEstimated_) {
                    uint64_t byteDelta = (uint64_t)(totalSize_ * pctDelta / 100.0);
                    currentSpeed_ = (float)byteDelta / (float)dt;
                    downloadedSize_ = (uint64_t)(totalSize_ * pct / 100.0);
                } else {
                    // 没有真实 totalSize_，不使用假的估算值
                    // 只基于百分比变化估算速度（用于显示趋势）
                    // 但不设置 totalSize_ 和 downloadedSize_，避免显示假数据
                    sizeIsEstimated_ = true;
                    // 使用一个合理的基准来估算速度趋势（仅用于图表）
                    const double SPEED_ESTIMATE_FACTOR = 50.0 * 1024.0 * 1024.0; // 50MB per %
                    currentSpeed_ = (float)(pctDelta * SPEED_ESTIMATE_FACTOR / dt);
                    // 不设置 totalSize_ 和 downloadedSize_，保持为0
                }

                lastPercentage_ = pct;
                lastTime_ = now;
            }

            p.bytesDownloaded = downloadedSize_;
            p.totalBytes = totalSize_;
            p.speed = currentSpeed_;
        }

        if (callback_) callback_(p);
        return;
    }

    // 总下载量: "Total downloaded: 123456 bytes (234567 bytes uncompressed) from 1 depots"
    std::regex totalRegex(R"(Total downloaded:\s*(\d+)\s*bytes.*?(\d+)\s*bytes uncompressed)");
    if (std::regex_search(line, match, totalRegex)) {
        isVerifying_ = false;
        p.state = "completed";
        p.isVerifying = false;
        p.percentage = 100.0;
        p.bytesDownloaded = std::stoull(match[2].str());
        p.totalBytes = p.bytesDownloaded;
        p.finished = true;
        p.success = true;
        if (callback_) callback_(p);
        return;
    }

    // 错误 - 但 "is not available" 只是警告，不是致命错误
    // 某些 depot 可能不可用（如 DLC），但主游戏仍然可以下载
    if (line.find("is not available") != std::string::npos) {
        // 只记录警告，不设置错误状态
        LogDD("[DepotDownloader] Warning: " + line + "\n");
        return;
    }

    if (line.find("Error:") != std::string::npos ||
        line.find("error:") != std::string::npos) {
        p.state = "error";
        p.error = line;
        if (callback_) callback_(p);
        return;
    }
}

} // namespace sf
