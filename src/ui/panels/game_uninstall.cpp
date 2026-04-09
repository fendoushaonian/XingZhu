#include "ui/panels/game_uninstall.h"
#include "ui/iconfonts.h"
#include "ui/toast.h"
#include "core/install_record.h"
#include "core/texture_manager.h"
#include "utils/logger.h"
#include <filesystem>
#include <fstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

namespace sf {

// 删除游戏快捷方式（桌面和开始菜单）
static void DeleteGameShortcuts(const std::string& gameName) {
    namespace fs = std::filesystem;
    std::wstring wName(gameName.begin(), gameName.end());
    std::wstring lnkName = wName + L".lnk";

    // 删除桌面快捷方式
    wchar_t desktopPath[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktopPath) == S_OK) {
        std::wstring desktopLnk = std::wstring(desktopPath) + L"\\" + lnkName;
        std::error_code ec;
        fs::remove(desktopLnk, ec);
        if (!ec) {
            Log(LogLevel::Info, "Deleted desktop shortcut for %s", gameName.c_str());
        }
    }

    // 删除开始菜单快捷方式
    wchar_t startMenuPath[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, startMenuPath) == S_OK) {
        std::wstring startMenuLnk = std::wstring(startMenuPath) + L"\\" + lnkName;
        std::error_code ec;
        fs::remove(startMenuLnk, ec);
        if (!ec) {
            Log(LogLevel::Info, "Deleted start menu shortcut for %s", gameName.c_str());
        }
    }
}

// 声明全局函数（定义在 application.cpp）
void NotifyGameUninstalled(const std::string& appId);

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

GameUninstallPanel::~GameUninstallPanel() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void GameUninstallPanel::Open(const std::string& appId, const std::string& gameName) {
    std::string installPath = InstallRecord::Get().GetInstallPath(appId);
    if (installPath.empty()) {
        ShowToast(u8"\u627E\u4E0D\u5230\u6E38\u620F\u5B89\u88C5\u8DEF\u5F84", ToastType::Error, 3.0f);
        return;
    }

    state_ = UninstallState{};
    state_.appId = appId;
    state_.gameName = gameName;
    state_.installPath = installPath;
    showDialog_ = true;
    dialogAnim_ = 0.0f;
    Log(LogLevel::Info, "GameUninstallPanel::Open appId=%s path=%s", appId.c_str(), installPath.c_str());
}

static std::string FormatBytes(int64_t bytes) {
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

void GameUninstallPanel::Render(float winW, float winH) {
    if (!showDialog_) return;

    // 检查是否需要通知主线程更新游戏状态（在主线程中执行）
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (state_.needsNotify && !state_.failed) {
            state_.needsNotify = false;
            NotifyGameUninstalled(state_.appId);
        }
    }

    float dt = ImGui::GetIO().DeltaTime;
    dialogAnim_ += (1.0f - dialogAnim_) * dt * 10.0f;
    if (dialogAnim_ > 0.99f) dialogAnim_ = 1.0f;

    // Dim background
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0, 0), ImVec2(winW, winH),
        IM_COL32(0, 0, 0, (int)(180 * dialogAnim_)));

    float dlgW = 480.0f;
    float dlgH = state_.active ? 280.0f : 220.0f;
    float dlgX = (winW - dlgW) * 0.5f;
    float dlgY = (winH - dlgH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(dlgX, dlgY + 20 * (1.0f - dialogAnim_)));
    ImGui::SetNextWindowSize(ImVec2(dlgW, dlgH));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(28, 35, 48, 250));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##UninstallDialog", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();

        std::lock_guard<std::mutex> lock(mtx_);

        if (!state_.active && !state_.completed) {
            // Confirmation dialog
            if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), u8"\u5378\u8F7D\u6E38\u620F");
            if (g_mainFontLarge) ImGui::PopFont();

            ImGui::Spacing();
            ImGui::TextWrapped(u8"\u786E\u5B9A\u8981\u5378\u8F7D \"%s\" \u5417\uFF1F", state_.gameName.c_str());
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.7f, 1.0f), u8"\u5B89\u88C5\u8DEF\u5F84: %s", state_.installPath.c_str());

            ImGui::Spacing();
            ImGui::Spacing();

            float btnW = 120.0f;
            float btnH = 36.0f;
            float spacing = 16.0f;
            float totalW = btnW * 2 + spacing;
            ImGui::SetCursorPosX((dlgW - totalW) * 0.5f - 24);
            ImGui::SetCursorPosY(dlgH - btnH - 30);

            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(180, 60, 60, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(210, 80, 80, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(150, 50, 50, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            if (ImGui::Button(u8"\u5378\u8F7D", ImVec2(btnW, btnH))) {
                StartUninstall();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0, spacing);

            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 70, 85, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 95, 115, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 60, 75, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            if (ImGui::Button(u8"\u53D6\u6D88", ImVec2(btnW, btnH))) {
                showDialog_ = false;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

        } else if (state_.active) {
            // Progress dialog
            if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), u8"\u6B63\u5728\u5378\u8F7D...");
            if (g_mainFontLarge) ImGui::PopFont();

            ImGui::Spacing();
            ImGui::Text(u8"\u6E38\u620F: %s", state_.gameName.c_str());

            ImGui::Spacing();
            ImGui::Spacing();

            // Progress bar
            float barX = wp.x + 24;
            float barY = wp.y + 120;
            float barW = dlgW - 48;
            float barH = 8.0f;

            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                              IM_COL32(40, 50, 65, 255), 4.0f);
            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * state_.progress, barY + barH),
                              IM_COL32(255, 140, 60, 255), 4.0f);

            ImGui::SetCursorPosY(135);
            ImGui::Text(u8"\u8FDB\u5EA6: %.1f%%", state_.progress * 100.0f);
            ImGui::Text(u8"\u5DF2\u5220\u9664: %lld / %lld \u6587\u4EF6",
                        (long long)state_.deletedFiles, (long long)state_.totalFiles);
            ImGui::Text(u8"\u5DF2\u91CA\u653E: %s", FormatBytes(state_.deletedBytes).c_str());

            if (!state_.currentFile.empty()) {
                ImGui::Spacing();
                std::string shortFile = state_.currentFile;
                if (shortFile.length() > 50) {
                    shortFile = "..." + shortFile.substr(shortFile.length() - 47);
                }
                ImGui::TextColored(ImVec4(0.5f, 0.55f, 0.6f, 1.0f), "%s", shortFile.c_str());
            }

        } else if (state_.completed) {
            // Completed dialog
            if (state_.failed) {
                if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), u8"\u5378\u8F7D\u5931\u8D25");
                if (g_mainFontLarge) ImGui::PopFont();
                ImGui::Spacing();
                ImGui::TextWrapped(u8"\u9519\u8BEF: %s", state_.errorMsg.c_str());
            } else {
                if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), u8"\u5378\u8F7D\u5B8C\u6210");
                if (g_mainFontLarge) ImGui::PopFont();
                ImGui::Spacing();
                ImGui::Text(u8"\u5DF2\u5220\u9664 %lld \u4E2A\u6587\u4EF6\uFF0C\u91CA\u653E %s \u7A7A\u95F4",
                            (long long)state_.deletedFiles, FormatBytes(state_.deletedBytes).c_str());
            }

            ImGui::Spacing();
            ImGui::Spacing();

            float btnW = 100.0f;
            float btnH = 32.0f;
            ImGui::SetCursorPosX((dlgW - btnW) * 0.5f - 24);
            ImGui::SetCursorPosY(dlgH - btnH - 30);

            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 130, 180, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 160, 220, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            if (ImGui::Button(u8"\u786E\u5B9A", ImVec2(btnW, btnH))) {
                showDialog_ = false;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void GameUninstallPanel::StartUninstall() {
    state_.active = true;
    state_.confirmed = true;

    if (worker_.joinable()) {
        worker_.join();
    }
    worker_ = std::thread(&GameUninstallPanel::UninstallWorker, this);
}

void GameUninstallPanel::UninstallWorker() {
    Log(LogLevel::Info, "UninstallWorker: Starting for %s at %s",
        state_.appId.c_str(), state_.installPath.c_str());

    try {
        namespace fs = std::filesystem;

        // First pass: count files and total size
        int64_t totalFiles = 0;
        int64_t totalBytes = 0;
        for (const auto& entry : fs::recursive_directory_iterator(state_.installPath)) {
            if (entry.is_regular_file()) {
                totalFiles++;
                totalBytes += entry.file_size();
            }
        }

        {
            std::lock_guard<std::mutex> lock(mtx_);
            state_.totalFiles = totalFiles;
            state_.totalBytes = totalBytes;
        }

        Log(LogLevel::Info, "UninstallWorker: Found %lld files, %lld bytes",
            (long long)totalFiles, (long long)totalBytes);

        // Second pass: delete files
        int64_t deletedFiles = 0;
        int64_t deletedBytes = 0;

        for (const auto& entry : fs::recursive_directory_iterator(state_.installPath)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                int64_t fileSize = entry.file_size();

                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    state_.currentFile = entry.path().filename().string();
                }

                std::error_code ec;
                fs::remove(entry.path(), ec);
                if (!ec) {
                    deletedFiles++;
                    deletedBytes += fileSize;

                    std::lock_guard<std::mutex> lock(mtx_);
                    state_.deletedFiles = deletedFiles;
                    state_.deletedBytes = deletedBytes;
                    state_.progress = totalFiles > 0 ? (float)deletedFiles / totalFiles : 0.0f;
                }
            }
        }

        // Remove empty directories
        std::error_code ec;
        fs::remove_all(state_.installPath, ec);

        // Remove from install record
        InstallRecord::Get().RemoveInstalled(state_.appId);
        InstallRecord::Get().Save();

        // Delete desktop and start menu shortcuts
        DeleteGameShortcuts(state_.gameName);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            state_.active = false;
            state_.completed = true;
            state_.progress = 1.0f;
            state_.currentFile.clear();
            state_.needsNotify = true;  // 标记需要通知
        }

        Log(LogLevel::Info, "UninstallWorker: Completed, deleted %lld files", (long long)deletedFiles);

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(mtx_);
        state_.active = false;
        state_.completed = true;
        state_.failed = true;
        state_.errorMsg = e.what();
        Log(LogLevel::Error, "UninstallWorker: Exception: %s", e.what());
    }
}

} // namespace sf
