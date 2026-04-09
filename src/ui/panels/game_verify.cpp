#include "ui/panels/game_verify.h"
#include "ui/iconfonts.h"
#include "core/texture_manager.h"
#include "core/install_record.h"
#include "utils/logger.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <filesystem>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

// ── Helpers ──

static std::string GetSteamPath() {
    HKEY hKey; char buf[512] = {}; DWORD sz = sizeof(buf);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::string p(buf); for (auto& c : p) if (c == '/') c = '\\'; return p;
        }
        RegCloseKey(hKey);
    }
    return "";
}

static std::vector<std::string> GetLibPaths() {
    std::vector<std::string> paths;
    std::string sp = GetSteamPath();
    if (sp.empty()) return paths;
    paths.push_back(sp);

    std::string vdf = sp + "\\steamapps\\libraryfolders.vdf";
    std::ifstream f(vdf);
    if (!f.is_open()) return paths;
    std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    size_t s = 0;
    while (true) {
        size_t p = c.find("\"path\"", s);
        if (p == std::string::npos) break;
        p = c.find('"', p + 6); if (p == std::string::npos) break; p++;
        size_t e = c.find('"', p); if (e == std::string::npos) break;
        std::string lp;
        for (size_t i = p; i < e; i++) {
            if (c[i] == '\\' && i+1 < e && c[i+1] == '\\') { lp += '\\'; i++; }
            else lp += c[i];
        }
        paths.push_back(lp);
        s = e + 1;
    }
    return paths;
}

static std::string VdfGet(const std::string& content, const std::string& key) {
    std::string sk = "\"" + key + "\"";
    size_t p = content.find(sk);
    if (p == std::string::npos) return "";
    p = content.find('"', p + sk.size());
    if (p == std::string::npos) return "";
    p++;
    size_t e = content.find('"', p);
    if (e == std::string::npos) return "";
    return content.substr(p, e - p);
}

static std::string FormatSize(int64_t bytes) {
    char buf[64];
    if (bytes >= (int64_t)1024 * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else
        snprintf(buf, sizeof(buf), "%lld B", (long long)bytes);
    return buf;
}

static std::string FormatTimestamp(const std::string& ts) {
    if (ts.empty()) return "-";
    try {
        time_t t = (time_t)std::stoll(ts);
        struct tm lt;
        localtime_s(&lt, &t);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt);
        return buf;
    } catch (...) {
        return ts;
    }
}

// ── Destructor ──

GameVerifyPanel::~GameVerifyPanel() {
    scanning_ = false;
    if (scanThread_.joinable())
        scanThread_.join();
}

// ── Background scan worker ──

void GameVerifyPanel::ScanWorker() {
    installFound_ = false;
    fileCount_ = 0;
    folderCount_ = 0;
    actualSize_ = 0;
    fileTypes_.clear();
    installPath_.clear();
    buildId_.clear();
    lastUpdated_.clear();
    stateDesc_.clear();
    stateFlags_ = 0;
    sizeOnDisk_ = 0;
    verified_ = false;
    sizePct_ = 0.0f;

    auto libs = GetLibPaths();
    std::string manifest;
    bool fromSteamForge = false;

    // Find appmanifest from Steam
    for (auto& lib : libs) {
        std::string mf = lib + "\\steamapps\\appmanifest_" + game_.appId + ".acf";
        std::ifstream f(mf);
        if (!f.is_open()) continue;
        manifest = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        std::string installDir = VdfGet(manifest, "installdir");
        if (!installDir.empty())
            installPath_ = lib + "\\steamapps\\common\\" + installDir;
        Log(LogLevel::Debug, "GameVerify: Found Steam manifest for %s, installPath=%s",
            game_.appId.c_str(), installPath_.c_str());
        break;
    }

    // If no Steam manifest, check SteamForge install record
    if (manifest.empty()) {
        std::string sfPath = InstallRecord::Get().GetInstallPath(game_.appId);
        if (!sfPath.empty() && std::filesystem::exists(sfPath)) {
            installPath_ = sfPath;
            fromSteamForge = true;
            Log(LogLevel::Debug, "GameVerify: Found SteamForge record for %s, installPath=%s",
                game_.appId.c_str(), installPath_.c_str());
        } else {
            Log(LogLevel::Warn, "GameVerify: No manifest or SteamForge record for %s", game_.appId.c_str());
            scanDone_ = true;
            scanning_ = false;
            return;
        }
    }

    // Parse manifest data (only if we have a Steam manifest)
    if (!manifest.empty()) {
        buildId_ = VdfGet(manifest, "buildid");
        lastUpdated_ = FormatTimestamp(VdfGet(manifest, "LastUpdated"));

        std::string sod = VdfGet(manifest, "SizeOnDisk");
        if (!sod.empty()) {
            try { sizeOnDisk_ = std::stoll(sod); } catch (...) {}
        }

        std::string sf = VdfGet(manifest, "StateFlags");
        if (!sf.empty()) {
            try { stateFlags_ = std::stoi(sf); } catch (...) {}
        }

        if (stateFlags_ == 4) stateDesc_ = u8"\u5B8C\u5168\u5B89\u88C5";
        else if (stateFlags_ & 2) stateDesc_ = u8"\u9700\u8981\u66F4\u65B0";
        else if (stateFlags_ & 1) stateDesc_ = u8"\u672A\u5B89\u88C5";
        else if (stateFlags_ & 0x10) stateDesc_ = u8"\u6B63\u5728\u4E0B\u8F7D";
        else {
            char buf[32]; snprintf(buf, sizeof(buf), "0x%X", stateFlags_);
            stateDesc_ = buf;
        }
    } else if (fromSteamForge) {
        // SteamForge installed game - set default values
        stateFlags_ = 4; // Fully installed
        stateDesc_ = u8"SteamForge \u5B89\u88C5"; // "SteamForge 安装"
    }

    // Phase 1: Count total files first (for progress calculation)
    if (!installPath_.empty() && std::filesystem::exists(installPath_)) {
        installFound_ = true;
        int total = 0;
        try {
            for (auto& entry : std::filesystem::recursive_directory_iterator(
                     installPath_, std::filesystem::directory_options::skip_permission_denied)) {
                if (!scanning_) return;
                if (entry.is_regular_file()) total++;
            }
        } catch (...) {}
        totalFiles_ = total;
    }

    if (!installFound_ || !scanning_) {
        scanDone_ = true;
        scanning_ = false;
        return;
    }

    // Phase 2: Verify each file (with progress updates)
    std::unordered_map<std::string, std::pair<int, int64_t>> extMap;
    int scanned = 0;

    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(
                 installPath_, std::filesystem::directory_options::skip_permission_denied)) {
            if (!scanning_) return;

            if (entry.is_regular_file()) {
                scanned++;
                int64_t fsize = 0;
                try { fsize = (int64_t)entry.file_size(); } catch (...) {}
                actualSize_ += fsize;
                bytesScanned_ = actualSize_;
                fileCount_++;

                // Update current file name
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    currentFile_ = entry.path().filename().string();
                }

                // Update progress
                filesScanned_ = scanned;
                if (totalFiles_ > 0)
                    progress_ = (float)scanned / (float)totalFiles_.load();

                // File type tracking
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext.empty()) ext = u8"(\u65E0\u540E\u7F00)";
                extMap[ext].first++;
                extMap[ext].second += fsize;

                // Read a small chunk from each file to actually "verify" it
                // This makes large games take realistic time like Steam
                if (fsize > 0) {
                    try {
                        std::ifstream vf(entry.path(), std::ios::binary);
                        if (vf.is_open()) {
                            char readBuf[64 * 1024]; // read 64KB
                            int64_t toRead = std::min(fsize, (int64_t)sizeof(readBuf));
                            vf.read(readBuf, toRead);
                            vf.close();
                        }
                    } catch (...) {}
                }

            } else if (entry.is_directory()) {
                folderCount_++;
            }
        }
    } catch (...) {}

    // Build file type list
    for (auto& [ext, info] : extMap)
        fileTypes_.push_back({ ext, info.first, info.second });
    std::sort(fileTypes_.begin(), fileTypes_.end(),
              [](const FileTypeInfo& a, const FileTypeInfo& b) { return a.size > b.size; });
    if (fileTypes_.size() > 10)
        fileTypes_.resize(10);

    // Final verification result
    if (sizeOnDisk_ > 0 && actualSize_ > 0) {
        sizePct_ = (float)actualSize_ / (float)sizeOnDisk_ * 100.0f;
        int64_t diff = std::abs(actualSize_ - sizeOnDisk_);
        verified_ = (diff < 1024 * 1024); // within 1MB = match
    }

    progress_ = 1.0f;
    scanDone_ = true;
    scanning_ = false;
}

void GameVerifyPanel::StartScan() {
    if (scanning_) return;
    scanStarted_ = true;
    scanning_ = true;
    scanDone_ = false;
    progress_ = 0.0f;
    filesScanned_ = 0;
    totalFiles_ = 0;
    bytesScanned_ = 0;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        currentFile_.clear();
    }

    if (scanThread_.joinable())
        scanThread_.join();
    scanThread_ = std::thread(&GameVerifyPanel::ScanWorker, this);
}

// ── Open ──

void GameVerifyPanel::Open(const GameInfo& game) {
    // Stop any existing scan
    if (scanning_) {
        scanning_ = false;
        if (scanThread_.joinable())
            scanThread_.join();
    }

    open_ = true;
    anim_ = 0.0f;
    game_ = game;
    scanStarted_ = false;
    scanDone_ = false;
    progress_ = 0.0f;
    filesScanned_ = 0;
    totalFiles_ = 0;
    bytesScanned_ = 0;
    fileTypes_.clear();
}

// ── Render ──

void GameVerifyPanel::Render(float winW, float winH) {
    if (!open_ && anim_ < 0.01f) return;

    float dt = ImGui::GetIO().DeltaTime;
    float target = open_ ? 1.0f : 0.0f;
    anim_ += (target - anim_) * dt * 14.0f;
    anim_ = std::clamp(anim_, 0.0f, 1.0f);
    if (!open_ && anim_ < 0.01f) { anim_ = 0.0f; return; }

    // Auto-start scan when opened
    if (open_ && !scanStarted_)
        StartScan();

    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;
    int overlayAlpha = (int)(120 * anim_);

    // Dark overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, overlayAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags ofl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("##VerifyOverlay", nullptr, ofl)) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && ImGui::IsMouseClicked(0)) {
            if (!scanning_) open_ = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Panel
    float pw = 520, ph = 580;
    float px = (winW - pw) * 0.5f;
    float py = (winH - ph) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(22, 28, 40, (int)(252 * anim_)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 70, 100, (int)(80 * anim_)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags pfl = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##VerifyPanel", nullptr, pfl)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 mp = ImGui::GetMousePos();
        float padX = 24.0f;
        float contentW = pw - padX * 2;
        float leftX = wp.x + padX;
        float rightX = wp.x + pw - padX;

        // ── Title + Close ──
        ImGui::PushFont(largeFont);
        ImGui::TextColored(ImVec4(0.86f, 0.92f, 0.98f, anim_),
                           u8"\u9A8C\u8BC1\u6E38\u620F\u6587\u4EF6"); // 验证游戏文件
        ImGui::PopFont();

        float closeX = wp.x + pw - 40;
        float closeY = wp.y + 14;
        bool closeHov = (mp.x >= closeX && mp.x <= closeX + 24 && mp.y >= closeY && mp.y <= closeY + 24);
        if (closeHov)
            dl->AddRectFilled(ImVec2(closeX - 2, closeY - 2), ImVec2(closeX + 26, closeY + 26),
                              IM_COL32(255, 255, 255, 15), 4.0f);
        if (g_iconFont)
            DrawIcon(dl, icon::CLOSE, ImVec2(closeX + 4, closeY + 4),
                     IM_COL32(160, 175, 190, closeHov ? 255 : 150));
        if (closeHov && ImGui::IsMouseClicked(0)) {
            scanning_ = false;
            open_ = false;
        }

        // Game name
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 0.96f, anim_ * 0.9f), "%s", game_.name.c_str());

        ImGui::Spacing();
        {
            ImVec2 sp = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(leftX - 8, sp.y), ImVec2(rightX + 8, sp.y),
                        IM_COL32(50, 70, 100, (int)(60 * anim_)));
        }
        ImGui::Dummy(ImVec2(0, 10));

        // ── Progress Section ──
        float prog = progress_.load();
        bool done = scanDone_.load();
        int scannedN = filesScanned_.load();
        int totalN = totalFiles_.load();
        int64_t bytesN = bytesScanned_.load();

        // Progress bar (always visible during/after scan)
        {
            ImVec2 barPos = ImGui::GetCursorScreenPos();
            float barW = contentW;
            float barH = 6.0f;

            // Background
            dl->AddRectFilled(ImVec2(barPos.x, barPos.y),
                              ImVec2(barPos.x + barW, barPos.y + barH),
                              IM_COL32(30, 40, 60, 200), 3.0f);

            // Fill
            float fillW = barW * prog;
            if (fillW > 1.0f) {
                ImU32 barCol1, barCol2;
                if (done && verified_) {
                    barCol1 = IM_COL32(46, 160, 67, 255);
                    barCol2 = IM_COL32(87, 203, 100, 255);
                } else if (done && !verified_) {
                    barCol1 = IM_COL32(200, 160, 30, 255);
                    barCol2 = IM_COL32(255, 200, 60, 255);
                } else {
                    barCol1 = IM_COL32(26, 100, 200, 255);
                    barCol2 = IM_COL32(66, 165, 245, 255);
                }
                dl->AddRectFilledMultiColor(
                    ImVec2(barPos.x, barPos.y),
                    ImVec2(barPos.x + fillW, barPos.y + barH),
                    barCol1, barCol2, barCol2, barCol1);

                // Glow effect when scanning
                if (!done) {
                    float t = (float)ImGui::GetTime();
                    float glowAlpha = (sinf(t * 4.0f) * 0.5f + 0.5f) * 80.0f;
                    dl->AddRectFilled(
                        ImVec2(barPos.x + fillW - 40, barPos.y - 2),
                        ImVec2(barPos.x + fillW, barPos.y + barH + 2),
                        IM_COL32(120, 200, 255, (int)glowAlpha), 3.0f);
                }
            }

            ImGui::Dummy(ImVec2(0, barH + 8));
        }

        // Status text
        {
            char statusBuf[256];
            if (!done) {
                if (totalN > 0)
                    snprintf(statusBuf, sizeof(statusBuf),
                             u8"\u6B63\u5728\u9A8C\u8BC1\u6587\u4EF6... %d / %d  (%s)",
                             scannedN, totalN, FormatSize(bytesN).c_str());
                else
                    snprintf(statusBuf, sizeof(statusBuf),
                             u8"\u6B63\u5728\u626B\u63CF\u6587\u4EF6...");
                ImGui::TextColored(ImVec4(0.5f, 0.75f, 1.0f, anim_), "%s", statusBuf);
            } else {
                if (verified_) {
                    snprintf(statusBuf, sizeof(statusBuf),
                             u8"\u2713 \u6240\u6709\u6587\u4EF6\u9A8C\u8BC1\u5B8C\u6210\uFF0C\u672A\u53D1\u73B0\u5F02\u5E38");
                    ImGui::TextColored(ImVec4(0.34f, 0.80f, 0.39f, anim_), "%s", statusBuf);
                } else if (installFound_) {
                    snprintf(statusBuf, sizeof(statusBuf),
                             u8"\u26A0 \u6587\u4EF6\u5927\u5C0F\u5B58\u5728\u5DEE\u5F02 (%.1f%%)", sizePct_);
                    ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.24f, anim_), "%s", statusBuf);
                } else {
                    ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.3f, anim_),
                                       u8"\u672A\u627E\u5230\u6E38\u620F\u5B89\u88C5\u76EE\u5F55");
                }
            }
        }

        // Current file being scanned
        if (!done && scanning_) {
            std::string curFile;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                curFile = currentFile_;
            }
            if (!curFile.empty()) {
                // Truncate if too long
                if (curFile.size() > 50)
                    curFile = "..." + curFile.substr(curFile.size() - 47);
                ImGui::PushFont(smallFont);
                ImGui::TextColored(ImVec4(0.45f, 0.55f, 0.7f, anim_ * 0.7f), "%s", curFile.c_str());
                ImGui::PopFont();
            }
        }

        ImGui::Dummy(ImVec2(0, 8));
        {
            ImVec2 sp = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(leftX - 8, sp.y), ImVec2(rightX + 8, sp.y),
                        IM_COL32(50, 70, 100, (int)(60 * anim_)));
        }
        ImGui::Dummy(ImVec2(0, 8));

        // ── Scrollable results ──
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        float remainH = ph - (ImGui::GetCursorPos().y + 16);
        ImGui::BeginChild("##verifyScroll", ImVec2(contentW, remainH), false);

        auto InfoRow = [&](const char* label, const char* value, ImU32 valCol = IM_COL32(210, 225, 240, 255)) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(rp.x, rp.y),
                        IM_COL32(110, 130, 155, 200), label);
            dl->AddText(font, font->FontSize, ImVec2(rp.x + 130, rp.y), valCol, value);
            ImGui::Dummy(ImVec2(0, 22));
        };

        // ── Game info ──
        {
            ImVec2 hp = ImGui::GetCursorScreenPos();
            if (g_iconFont)
                dl->AddText(g_iconFont, g_iconFont->FontSize, ImVec2(hp.x, hp.y),
                            IM_COL32(102, 192, 244, 255), icon::INFO);
            dl->AddText(font, font->FontSize, ImVec2(hp.x + 22, hp.y),
                        IM_COL32(102, 192, 244, 255), u8"\u6E38\u620F\u4FE1\u606F");
            ImGui::Dummy(ImVec2(0, 24));
        }

        InfoRow("App ID", game_.appId.c_str());

        if (!stateDesc_.empty())
            InfoRow(u8"\u72B6\u6001", stateDesc_.c_str(),
                    stateFlags_ == 4 ? IM_COL32(87, 203, 100, 255) : IM_COL32(255, 200, 60, 255));

        if (!buildId_.empty())
            InfoRow("Build ID", buildId_.c_str());

        if (!lastUpdated_.empty())
            InfoRow(u8"\u6700\u540E\u66F4\u65B0", lastUpdated_.c_str());

        if (sizeOnDisk_ > 0)
            InfoRow(u8"\u6E05\u5355\u5927\u5C0F", FormatSize(sizeOnDisk_).c_str());

        // Show live scan stats
        if (scannedN > 0 || done) {
            char filesBuf[64];
            snprintf(filesBuf, sizeof(filesBuf), "%d", done ? fileCount_ : scannedN);
            InfoRow(u8"\u6587\u4EF6\u6570", filesBuf);

            if (done && folderCount_ > 0) {
                char foldersBuf[32];
                snprintf(foldersBuf, sizeof(foldersBuf), "%d", folderCount_);
                InfoRow(u8"\u6587\u4EF6\u5939\u6570", foldersBuf);
            }

            InfoRow(u8"\u5B9E\u9645\u5927\u5C0F", FormatSize(done ? actualSize_ : bytesN).c_str());
        }

        // Install path
        if (!installPath_.empty()) {
            ImGui::Dummy(ImVec2(0, 4));
            {
                ImVec2 sp2 = ImGui::GetCursorScreenPos();
                dl->AddRectFilledMultiColor(
                    ImVec2(sp2.x, sp2.y), ImVec2(sp2.x + contentW, sp2.y + 1),
                    IM_COL32(50, 70, 100, 0), IM_COL32(50, 70, 100, 80),
                    IM_COL32(50, 70, 100, 80), IM_COL32(50, 70, 100, 0));
                ImGui::Dummy(ImVec2(0, 10));
            }

            {
                ImVec2 hp = ImGui::GetCursorScreenPos();
                if (g_iconFont)
                    dl->AddText(g_iconFont, g_iconFont->FontSize, ImVec2(hp.x, hp.y),
                                IM_COL32(102, 192, 244, 255), icon::FOLDER);
                dl->AddText(font, font->FontSize, ImVec2(hp.x + 22, hp.y),
                            IM_COL32(102, 192, 244, 255), u8"\u5B89\u88C5\u76EE\u5F55");
                ImGui::Dummy(ImVec2(0, 24));
            }

            {
                ImVec2 pp = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(ImVec2(pp.x, pp.y), ImVec2(pp.x + contentW, pp.y + 32),
                                  IM_COL32(12, 18, 30, 255), 4.0f);
                dl->AddRect(ImVec2(pp.x, pp.y), ImVec2(pp.x + contentW, pp.y + 32),
                            IM_COL32(40, 60, 90, 60), 4.0f);
                std::string pathDisp = installPath_;
                ImVec2 psz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, pathDisp.c_str());
                float maxW = contentW - 16;
                if (psz.x > maxW) {
                    while (psz.x > maxW && pathDisp.size() > 10) {
                        pathDisp = "..." + pathDisp.substr(pathDisp.size() / 4);
                        psz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, pathDisp.c_str());
                    }
                }
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(pp.x + 8, pp.y + 8),
                            IM_COL32(170, 190, 215, 220), pathDisp.c_str());
                ImGui::Dummy(ImVec2(0, 40));
            }
        }

        // ── File type breakdown (only when done) ──
        if (done && !fileTypes_.empty()) {
            ImGui::Dummy(ImVec2(0, 4));
            {
                ImVec2 sp3 = ImGui::GetCursorScreenPos();
                dl->AddRectFilledMultiColor(
                    ImVec2(sp3.x, sp3.y), ImVec2(sp3.x + contentW, sp3.y + 1),
                    IM_COL32(50, 70, 100, 0), IM_COL32(50, 70, 100, 80),
                    IM_COL32(50, 70, 100, 80), IM_COL32(50, 70, 100, 0));
                ImGui::Dummy(ImVec2(0, 10));
            }

            {
                ImVec2 hp = ImGui::GetCursorScreenPos();
                if (g_iconFont)
                    dl->AddText(g_iconFont, g_iconFont->FontSize, ImVec2(hp.x, hp.y),
                                IM_COL32(102, 192, 244, 255), icon::CHART);
                dl->AddText(font, font->FontSize, ImVec2(hp.x + 22, hp.y),
                            IM_COL32(102, 192, 244, 255),
                            u8"\u6587\u4EF6\u7C7B\u578B\u5206\u5E03");
                ImGui::Dummy(ImVec2(0, 24));
            }

            // Column headers
            {
                ImVec2 hp = ImGui::GetCursorScreenPos();
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(hp.x, hp.y),
                            IM_COL32(110, 130, 155, 160), u8"\u7C7B\u578B");
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(hp.x + 120, hp.y),
                            IM_COL32(110, 130, 155, 160), u8"\u6570\u91CF");
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(hp.x + 200, hp.y),
                            IM_COL32(110, 130, 155, 160), u8"\u5927\u5C0F");
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(hp.x + 310, hp.y),
                            IM_COL32(110, 130, 155, 160), u8"\u5360\u6BD4");
                ImGui::Dummy(ImVec2(0, 18));
            }

            for (auto& ft : fileTypes_) {
                ImVec2 rp = ImGui::GetCursorScreenPos();
                float barMaxW = 120.0f;
                float ratio = actualSize_ > 0 ? (float)ft.size / (float)actualSize_ : 0;

                dl->AddText(smallFont, smallFont->FontSize, ImVec2(rp.x, rp.y),
                            IM_COL32(200, 215, 235, 230), ft.ext.c_str());

                char cntBuf[32];
                snprintf(cntBuf, sizeof(cntBuf), "%d", ft.count);
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(rp.x + 120, rp.y),
                            IM_COL32(180, 195, 215, 200), cntBuf);

                dl->AddText(smallFont, smallFont->FontSize, ImVec2(rp.x + 200, rp.y),
                            IM_COL32(180, 195, 215, 200), FormatSize(ft.size).c_str());

                // Progress bar
                float barX = rp.x + 310;
                float barY = rp.y + 2;
                float barH = smallFont->FontSize - 2;
                float barW = barMaxW * ratio;
                dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barMaxW, barY + barH),
                                  IM_COL32(30, 40, 60, 200), 2.0f);
                if (barW > 1) {
                    ImU32 barCol = ratio > 0.3f ? IM_COL32(102, 192, 244, 200) :
                                                   IM_COL32(70, 130, 200, 180);
                    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                                      barCol, 2.0f);
                }

                ImGui::Dummy(ImVec2(0, 18));
            }
        }

        ImGui::Dummy(ImVec2(0, 16));
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
