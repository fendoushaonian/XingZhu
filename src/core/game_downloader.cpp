#include "core/game_downloader.h"
#include "utils/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shlobj.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace sf {

// 默认CDN服务器 (本地开发服务器)
static const char* DEFAULT_CDN_SERVERS[] = {
    "http://127.0.0.1:8080",
    // 添加你的生产CDN服务器:
    // "https://cdn1.yourserver.com",
    // "https://cdn2.yourserver.com",
};

// 全局实例
static GameDownloader* g_downloader = nullptr;

GameDownloader& GetGameDownloader() {
    if (!g_downloader) {
        g_downloader = new GameDownloader();
    }
    return *g_downloader;
}

GameDownloader::GameDownloader() {
    // 初始化默认CDN
    for (const auto& cdn : DEFAULT_CDN_SERVERS) {
        cdnServers_.push_back(cdn);
    }
}

GameDownloader::~GameDownloader() {
    CancelDownload();
    if (downloadThread_.joinable()) {
        downloadThread_.join();
    }
}

void GameDownloader::SetCDNServers(const std::vector<std::string>& servers) {
    cdnServers_ = servers;
    activeCdn_.clear();
}

// HTTP GET 请求
static bool HttpGet(const std::string& url, std::string& response, int64_t rangeStart = -1, int64_t rangeEnd = -1) {
    std::string u = url;
    bool isHttps = true;

    if (u.find("https://") == 0) u = u.substr(8);
    else if (u.find("http://") == 0) { u = u.substr(7); isHttps = false; }

    size_t slashPos = u.find('/');
    std::string host, path;
    if (slashPos == std::string::npos) { host = u; path = "/"; }
    else { host = u.substr(0, slashPos); path = u.substr(slashPos); }

    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"SteamForge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    WinHttpSetTimeouts(hSession, 30000, 30000, 60000, 60000);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    // 添加Range头 (断点续传)
    if (rangeStart >= 0) {
        std::wstringstream rangeHeader;
        rangeHeader << L"Range: bytes=" << rangeStart;
        if (rangeEnd >= 0) rangeHeader << L"-" << rangeEnd;
        else rangeHeader << L"-";
        WinHttpAddRequestHeaders(hRequest, rangeHeader.str().c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    DWORD decompFlags = WINHTTP_DECOMPRESSION_FLAG_ALL;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompFlags, sizeof(decompFlags));

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    response.clear();
    DWORD dwSize = 0, dwDownloaded = 0;
    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;
        char* buf = (char*)malloc(dwSize + 1);
        if (!buf) break;
        memset(buf, 0, dwSize + 1);
        WinHttpReadData(hRequest, buf, dwSize, &dwDownloaded);
        response.append(buf, dwDownloaded);
        free(buf);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// 计算文件SHA256
static std::string ComputeSHA256(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return "";

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    UCHAR hash[32];
    DWORD hashLen = 0, resultLen = 0;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return "";
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &resultLen, 0);
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    char buffer[65536];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        BCryptHashData(hHash, (PUCHAR)buffer, (ULONG)file.gcount(), 0);
    }

    BCryptFinishHash(hHash, hash, hashLen, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    std::ostringstream ss;
    for (DWORD i = 0; i < hashLen; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

// 解析JSON清单 (简单解析器)
static bool ParseManifestJson(const std::string& json, GameManifest& manifest) {
    // 简单的JSON解析 - 实际项目中建议用nlohmann/json
    auto extractString = [&](const char* key) -> std::string {
        std::string needle = std::string("\"") + key + "\":\"";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.length();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    auto extractInt = [&](const char* key) -> int64_t {
        std::string needle = std::string("\"") + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return 0;
        pos += needle.length();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        int64_t val = 0;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
            val = val * 10 + (json[pos] - '0');
            pos++;
        }
        return val;
    };

    manifest.appId = extractString("appId");
    manifest.name = extractString("name");
    manifest.version = extractString("version");
    manifest.totalSize = extractInt("totalSize");
    manifest.totalCompressed = extractInt("totalCompressed");
    manifest.installScript = extractString("installScript");

    // 解析files数组
    size_t filesPos = json.find("\"files\":[");
    if (filesPos == std::string::npos) return false;
    filesPos += 9;

    size_t filesEnd = json.find("]", filesPos);
    std::string filesJson = json.substr(filesPos, filesEnd - filesPos);

    size_t pos = 0;
    while (pos < filesJson.size()) {
        size_t objStart = filesJson.find("{", pos);
        if (objStart == std::string::npos) break;
        size_t objEnd = filesJson.find("}", objStart);
        if (objEnd == std::string::npos) break;

        std::string objJson = filesJson.substr(objStart, objEnd - objStart + 1);

        GameFile file;
        auto extractFileStr = [&](const char* key) -> std::string {
            std::string needle = std::string("\"") + key + "\":\"";
            size_t p = objJson.find(needle);
            if (p == std::string::npos) return "";
            p += needle.length();
            size_t e = objJson.find("\"", p);
            if (e == std::string::npos) return "";
            return objJson.substr(p, e - p);
        };
        auto extractFileInt = [&](const char* key) -> int64_t {
            std::string needle = std::string("\"") + key + "\":";
            size_t p = objJson.find(needle);
            if (p == std::string::npos) return 0;
            p += needle.length();
            while (p < objJson.size() && (objJson[p] == ' ')) p++;
            int64_t v = 0;
            while (p < objJson.size() && objJson[p] >= '0' && objJson[p] <= '9') {
                v = v * 10 + (objJson[p] - '0');
                p++;
            }
            return v;
        };

        file.path = extractFileStr("path");
        file.url = extractFileStr("url");
        file.sha256 = extractFileStr("sha256");
        file.size = extractFileInt("size");
        file.compressedSize = extractFileInt("compressedSize");
        file.compressed = (objJson.find("\"compressed\":true") != std::string::npos);

        if (!file.path.empty()) {
            manifest.files.push_back(file);
        }
        pos = objEnd + 1;
    }

    return !manifest.appId.empty() && !manifest.files.empty();
}

bool GameDownloader::FetchManifest(const std::string& appId, GameManifest& outManifest) {
    if (activeCdn_.empty()) {
        activeCdn_ = SelectFastestCDN();
    }

    std::string url = activeCdn_ + "/api/manifest/" + appId;
    std::string response;

    Log(LogLevel::Info, "Fetching manifest from: %s", url.c_str());

    if (!HttpGet(url, response)) {
        Log(LogLevel::Error, "Failed to fetch manifest for appId=%s", appId.c_str());
        return false;
    }

    if (!ParseManifestJson(response, outManifest)) {
        Log(LogLevel::Error, "Failed to parse manifest JSON");
        return false;
    }

    Log(LogLevel::Info, "Manifest loaded: %s (%d files, %.2f GB)",
        outManifest.name.c_str(), (int)outManifest.files.size(),
        outManifest.totalSize / (1024.0 * 1024.0 * 1024.0));

    return true;
}

std::string GameDownloader::SelectFastestCDN() {
    if (cdnServers_.empty()) return "";
    if (cdnServers_.size() == 1) return cdnServers_[0];

    // 测试每个CDN的延迟
    std::string fastest;
    int64_t fastestTime = INT64_MAX;

    for (const auto& cdn : cdnServers_) {
        auto start = std::chrono::high_resolution_clock::now();
        std::string response;
        if (HttpGet(cdn + "/ping", response)) {
            auto end = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            Log(LogLevel::Debug, "CDN %s latency: %lld ms", cdn.c_str(), ms);
            if (ms < fastestTime) {
                fastestTime = ms;
                fastest = cdn;
            }
        }
    }

    if (fastest.empty() && !cdnServers_.empty()) {
        fastest = cdnServers_[0];
    }

    Log(LogLevel::Info, "Selected CDN: %s", fastest.c_str());
    return fastest;
}

bool GameDownloader::StartDownload(const GameManifest& manifest, const DownloadConfig& config, GameDownloadCallback callback) {
    if (downloading_.load()) {
        Log(LogLevel::Warn, "Download already in progress");
        return false;
    }

    currentManifest_ = manifest;
    currentConfig_ = config;
    progressCallback_ = callback;

    // 初始化进度
    {
        std::lock_guard<std::mutex> lock(progressMtx_);
        progress_ = DownloadProgress();
        progress_.appId = manifest.appId;
        progress_.gameName = manifest.name;
        progress_.bytesTotal = manifest.totalCompressed > 0 ? manifest.totalCompressed : manifest.totalSize;
        progress_.filesTotal = (int)manifest.files.size();
        progress_.status = "preparing";
    }

    // 创建安装目录
    fs::path installDir = config.installPath;
    if (!fs::exists(installDir)) {
        std::error_code ec;
        fs::create_directories(installDir, ec);
        if (ec) {
            Log(LogLevel::Error, "Failed to create install directory: %s", ec.message().c_str());
            return false;
        }
    }

    // 选择CDN
    if (activeCdn_.empty()) {
        activeCdn_ = SelectFastestCDN();
    }

    // 启动下载线程
    downloading_ = true;
    paused_ = false;
    cancelled_ = false;

    if (downloadThread_.joinable()) {
        downloadThread_.join();
    }
    downloadThread_ = std::thread(&GameDownloader::DownloadThreadFunc, this);

    return true;
}

void GameDownloader::DownloadThreadFunc() {
    Log(LogLevel::Info, "Download thread started for: %s", currentManifest_.name.c_str());

    UpdateProgress("downloading");

    std::string cdnBase = currentConfig_.cdnBaseUrl.empty() ? activeCdn_ : currentConfig_.cdnBaseUrl;
    int64_t totalDownloaded = 0;
    int filesCompleted = 0;

    for (const auto& file : currentManifest_.files) {
        if (cancelled_.load()) {
            UpdateProgress("cancelled");
            break;
        }

        // 等待暂停解除
        while (paused_.load() && !cancelled_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (cancelled_.load()) break;

        // 构建目标路径
        fs::path destPath = fs::path(currentConfig_.installPath) / file.path;
        fs::create_directories(destPath.parent_path());

        // 更新当前文件
        {
            std::lock_guard<std::mutex> lock(progressMtx_);
            progress_.currentFile = file.path;
        }

        // 检查文件是否已存在且完整
        if (fs::exists(destPath)) {
            if (currentConfig_.verifyAfterDownload) {
                std::string hash = ComputeSHA256(destPath.string());
                if (hash == file.sha256) {
                    Log(LogLevel::Debug, "File already complete: %s", file.path.c_str());
                    totalDownloaded += file.compressed ? file.compressedSize : file.size;
                    filesCompleted++;
                    continue;
                }
            }
        }

        // 下载文件
        int64_t fileDownloaded = 0;
        bool success = DownloadFile(file, destPath.string(), cdnBase, fileDownloaded);

        if (!success) {
            Log(LogLevel::Error, "Failed to download: %s", file.path.c_str());
            {
                std::lock_guard<std::mutex> lock(progressMtx_);
                progress_.status = "error";
                progress_.error = "Failed to download: " + file.path;
            }
            if (progressCallback_) progressCallback_(progress_);
            break;
        }

        totalDownloaded += fileDownloaded;
        filesCompleted++;

        // 更新进度
        {
            std::lock_guard<std::mutex> lock(progressMtx_);
            progress_.bytesDownloaded = totalDownloaded;
            progress_.filesCompleted = filesCompleted;
            progress_.progress = (float)totalDownloaded / progress_.bytesTotal;
        }
        if (progressCallback_) progressCallback_(progress_);
    }

    if (!cancelled_.load() && progress_.status != "error") {
        // 验证所有文件
        if (currentConfig_.verifyAfterDownload) {
            UpdateProgress("verifying");
            // ... 验证逻辑
        }

        UpdateProgress("done");
        Log(LogLevel::Info, "Download completed: %s", currentManifest_.name.c_str());
    }

    downloading_ = false;
}

bool GameDownloader::DownloadFile(const GameFile& file, const std::string& destPath,
                                   const std::string& cdnBase, int64_t& bytesDownloaded) {
    std::string url = file.url.empty() ? (cdnBase + "/files/" + file.path) : file.url;
    int64_t fileSize = file.compressed ? file.compressedSize : file.size;

    // 检查断点续传
    int64_t existingSize = 0;
    if (fs::exists(destPath + ".part")) {
        existingSize = fs::file_size(destPath + ".part");
    }

    std::string partPath = destPath + ".part";
    std::ofstream outFile(partPath, std::ios::binary | std::ios::app);
    if (!outFile.is_open()) {
        Log(LogLevel::Error, "Cannot open file for writing: %s", partPath.c_str());
        return false;
    }

    // 分块下载
    const int64_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB chunks
    int64_t downloaded = existingSize;
    int retries = 0;

    while (downloaded < fileSize && !cancelled_.load()) {
        while (paused_.load() && !cancelled_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (cancelled_.load()) break;

        int64_t chunkStart = downloaded;
        int64_t chunkEnd = std::min(downloaded + CHUNK_SIZE - 1, fileSize - 1);

        std::string chunkData;
        if (!HttpGet(url, chunkData, chunkStart, chunkEnd)) {
            retries++;
            if (retries >= currentConfig_.maxRetries) {
                Log(LogLevel::Error, "Max retries exceeded for: %s", file.path.c_str());
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 * retries));
            continue;
        }

        outFile.write(chunkData.data(), chunkData.size());
        downloaded += chunkData.size();
        bytesDownloaded += chunkData.size();
        retries = 0;

        // 更新速度
        {
            std::lock_guard<std::mutex> lock(progressMtx_);
            progress_.bytesDownloaded += chunkData.size();
            progress_.progress = (float)progress_.bytesDownloaded / progress_.bytesTotal;

            // 计算速度
            float now = (float)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0f;
            if (lastSpeedCheckTime_ > 0) {
                float dt = now - lastSpeedCheckTime_;
                if (dt > 0.5f) {
                    progress_.speed = (float)(progress_.bytesDownloaded - lastSpeedCheckBytes_) / dt;
                    lastSpeedCheckBytes_ = progress_.bytesDownloaded;
                    lastSpeedCheckTime_ = now;

                    if (progress_.speed > 0) {
                        progress_.eta = (progress_.bytesTotal - progress_.bytesDownloaded) / progress_.speed;
                    }
                }
            } else {
                lastSpeedCheckTime_ = now;
                lastSpeedCheckBytes_ = progress_.bytesDownloaded;
            }
        }
        if (progressCallback_) progressCallback_(progress_);
    }

    outFile.close();

    if (cancelled_.load()) return false;

    // 校验
    if (currentConfig_.verifyAfterDownload) {
        std::string hash = ComputeSHA256(partPath);
        if (hash != file.sha256) {
            Log(LogLevel::Error, "Hash mismatch for: %s", file.path.c_str());
            fs::remove(partPath);
            return false;
        }
    }

    // 重命名为最终文件
    fs::rename(partPath, destPath);
    return true;
}

void GameDownloader::PauseDownload() {
    paused_ = true;
    UpdateProgress("paused");
}

void GameDownloader::ResumeDownload() {
    paused_ = false;
    UpdateProgress("downloading");
}

void GameDownloader::CancelDownload() {
    cancelled_ = true;
    paused_ = false;
}

DownloadProgress GameDownloader::GetProgress() const {
    std::lock_guard<std::mutex> lock(progressMtx_);
    return progress_;
}

void GameDownloader::UpdateProgress(const std::string& status, const std::string& currentFile) {
    {
        std::lock_guard<std::mutex> lock(progressMtx_);
        progress_.status = status;
        if (!currentFile.empty()) progress_.currentFile = currentFile;
    }
    if (progressCallback_) progressCallback_(progress_);
}

bool GameDownloader::VerifyFileSHA256(const std::string& filePath, const std::string& expectedHash) {
    return ComputeSHA256(filePath) == expectedHash;
}

bool GameDownloader::VerifyInstallation(const std::string& installPath, const GameManifest& manifest,
                                         std::vector<std::string>& corruptedFiles) {
    corruptedFiles.clear();
    for (const auto& file : manifest.files) {
        fs::path filePath = fs::path(installPath) / file.path;
        if (!fs::exists(filePath)) {
            corruptedFiles.push_back(file.path);
            continue;
        }
        std::string hash = ComputeSHA256(filePath.string());
        if (hash != file.sha256) {
            corruptedFiles.push_back(file.path);
        }
    }
    return corruptedFiles.empty();
}

bool GameDownloader::RepairFiles(const std::string& installPath, const GameManifest& manifest,
                                  const std::vector<std::string>& files, GameDownloadCallback callback) {
    // 创建只包含需要修复文件的清单
    GameManifest repairManifest = manifest;
    repairManifest.files.clear();

    for (const auto& filePath : files) {
        for (const auto& f : manifest.files) {
            if (f.path == filePath) {
                repairManifest.files.push_back(f);
                break;
            }
        }
    }

    DownloadConfig config;
    config.installPath = installPath;
    config.verifyAfterDownload = true;

    return StartDownload(repairManifest, config, callback);
}

std::string GameDownloader::GetResumeFilePath(const std::string& appId) const {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\SteamForge\\downloads\\" + appId + ".resume";
    }
    return "";
}

} // namespace sf
