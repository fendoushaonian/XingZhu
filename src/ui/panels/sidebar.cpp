#include "ui/panels/sidebar.h"
#include "ui/iconfonts.h"
#include "core/texture_manager.h"
#include "core/auth.h"
#include "core/steam_library.h"
#include "core/navigation.h"
#include "core/steam_detect.h"
#include "core/install_record.h"
#include "core/user_library.h"
#include "core/game_process.h"
#include "ui/toast.h"
#include "app/config.h"
#include "utils/logger.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <windows.h>

namespace sf {

// UTF-8 字符串转 wstring (用于 filesystem 操作)
static std::wstring Utf8ToWideSidebar(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// 安全检查路径是否存在（支持中文路径）
static bool PathExistsSafe(const std::string& path) {
    if (path.empty()) return false;  // 空路径直接返回 false
    try {
        std::wstring wpath = Utf8ToWideSidebar(path);
        if (wpath.empty()) return false;
        return std::filesystem::exists(wpath) && std::filesystem::is_directory(wpath);
    } catch (...) {
        return false;
    }
}

GameListSidebar::GameListSidebar() {
    // 不再使用假数据，游戏列表完全来自 Steam API
    memset(hoverAnim_, 0, sizeof(hoverAnim_));
}

const GameInfo* GameListSidebar::GetSelectedGame() const {
    if (selectedGame_ >= 0 && selectedGame_ < (int)games_.size()) return &games_[selectedGame_];
    return nullptr;
}

// Collect Steam library folder paths (main + additional from libraryfolders.vdf)
static std::vector<std::string> GetSteamLibPaths() {
    std::vector<std::string> paths;
    auto& st = GetSteamStatus();
    if (st.steamPath.empty()) {
        Log(LogLevel::Warn, "GetSteamLibPaths: steamPath is empty!");
        return paths;
    }
    paths.push_back(st.steamPath);
    Log(LogLevel::Debug, "GetSteamLibPaths: main path = %s", st.steamPath.c_str());

    std::string vdf = st.steamPath + "\\steamapps\\libraryfolders.vdf";
    std::ifstream f(vdf);
    if (!f.is_open()) return paths;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    size_t s = 0;
    while (true) {
        size_t p = content.find("\"path\"", s);
        if (p == std::string::npos) break;
        p = content.find('"', p + 6); if (p == std::string::npos) break; p++;
        size_t e = content.find('"', p); if (e == std::string::npos) break;
        std::string lp;
        for (size_t i = p; i < e; i++) {
            if (content[i] == '\\' && i + 1 < e && content[i + 1] == '\\') { lp += '\\'; i++; }
            else lp += content[i];
        }
        paths.push_back(lp);
        s = e + 1;
    }
    return paths;
}

// Check if appmanifest_<appId>.acf exists in any Steam library folder
// OR if the game was installed via SteamForge
static bool IsGameInstalled(const std::vector<std::string>& libPaths, const std::string& appId) {
    // 1. 检查 Steam 的 appmanifest
    for (const auto& lp : libPaths) {
        std::string mf = lp + "\\steamapps\\appmanifest_" + appId + ".acf";
        DWORD attr = GetFileAttributesA(mf.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            Log(LogLevel::Debug, "IsGameInstalled: %s found at %s", appId.c_str(), mf.c_str());
            return true;
        }
    }
    // 2. 检查 SteamForge 安装记录
    if (InstallRecord::Get().IsInstalled(appId)) {
        Log(LogLevel::Debug, "IsGameInstalled: %s found in SteamForge records", appId.c_str());
        return true;
    }
    return false;
}

void GameListSidebar::RefreshFromSteamLibrary() {
    SteamLibraryData lib;
    bool hasSteamLib = GetSteamLibraryCopy(lib) && lib.loaded;

    // 计算当前状态的版本号，用于检测是否需要刷新
    // 包括：Steam库版本 + UserLibrary游戏数量 + InstallRecord数量
    size_t userGameCount = UserLibrary::Get().GetAllGames().size();
    size_t installedCount = InstallRecord::Get().GetAllInstalled().size();
    std::string currentVersion = (hasSteamLib ? lib.steamId : "no_steam") + "_"
        + std::to_string(userGameCount) + "_" + std::to_string(installedCount);

    // 如果版本没变，不需要刷新
    if (currentVersion == loadedSteamId_) return;

    Log(LogLevel::Info, "RefreshFromSteamLibrary: refreshing (version=%s)", currentVersion.c_str());
    loadedSteamId_ = currentVersion;
    games_.clear();
    selectedGame_ = -1;

    std::vector<std::string> libPaths = GetSteamLibPaths();
    Log(LogLevel::Info, "RefreshFromSteamLibrary: got %d library paths", (int)libPaths.size());

    std::unordered_set<std::string> addedAppIds;
    std::vector<std::string> appIds;

    // 1. 添加 Steam 库中的游戏（如果有）
    if (hasSteamLib) {
        for (const auto& sg : lib.games) {
            GameInfo gi;
            gi.name = sg.name;
            gi.appId = sg.appId;
            gi.status = IsGameInstalled(libPaths, sg.appId) ? "installed" : "not_installed";
            gi.playtime = sg.playtimeForever / 60.0f; // minutes -> hours
            gi.size = 0;
            gi.achievements = 0;
            gi.achievementsTotal = 0;
            gi.hasTradingCards = false;
            appIds.push_back(sg.appId);
            games_.push_back(std::move(gi));
            addedAppIds.insert(sg.appId);
        }
    }

    // 2. 合并用户库中的游戏（免费游戏等）
    const auto& userGames = UserLibrary::Get().GetAllGames();
    for (const auto& ug : userGames) {
        if (addedAppIds.count(ug.appId)) continue; // 已添加，跳过

        GameInfo gi;
        gi.name = ug.name;
        gi.appId = ug.appId;
        gi.status = IsGameInstalled(libPaths, ug.appId) ? "installed" : "not_installed";
        gi.playtime = 0;
        gi.size = 0;
        gi.achievements = 0;
        gi.achievementsTotal = 0;
        gi.hasTradingCards = false;
        appIds.push_back(ug.appId);
        games_.push_back(std::move(gi));
        addedAppIds.insert(ug.appId);
    }

    // 3. 合并 InstallRecord 中的游戏（通过 SteamForge 安装的游戏）
    const auto& installRecords = InstallRecord::Get().GetAllRecords();
    for (const auto& ir : installRecords) {
        if (addedAppIds.count(ir.appId)) continue; // 已添加，跳过
        if (ir.installPath.empty()) continue; // 跳过没有安装路径的记录（如 Steam 游戏的启动记录）

        // 检查安装路径是否存在（使用安全的中文路径检查）
        bool installed = PathExistsSafe(ir.installPath);
        if (!installed) continue; // 安装目录不存在，跳过

        GameInfo gi;
        gi.appId = ir.appId;
        // 优先使用记录中的名称，否则使用 appId 作为临时名称
        gi.name = ir.gameName.empty() ? ("Game " + ir.appId) : ir.gameName;
        gi.status = "installed";
        gi.playtime = 0;
        gi.size = 0;
        gi.achievements = 0;
        gi.achievementsTotal = 0;
        gi.hasTradingCards = false;
        appIds.push_back(ir.appId);
        games_.push_back(std::move(gi));
        addedAppIds.insert(ir.appId);
    }

    Log(LogLevel::Info, "RefreshFromSteamLibrary: loaded %d games (Steam=%d, User=%d, Installed=%d)",
        (int)games_.size(), hasSteamLib ? (int)lib.games.size() : 0, (int)userGames.size(), (int)installRecords.size());

    // Pre-request all game textures with HIGH PRIORITY (library loads first)
    if (!appIds.empty()) {
        Log(LogLevel::Info, "RefreshFromSteamLibrary: requesting textures for %d games", (int)appIds.size());
        RequestGameTexturesPriority(appIds);
    }
    Log(LogLevel::Info, "RefreshFromSteamLibrary: done");
}

void GameListSidebar::ForceRefresh() {
    // 清除已加载标记，强制下次 Render 时重新加载
    loadedSteamId_.clear();
}

void GameListSidebar::AddGameToList(const std::string& appId, const std::string& name) {
    // 检查是否已存在
    for (const auto& g : games_) {
        if (g.appId == appId) return;
    }

    std::vector<std::string> libPaths = GetSteamLibPaths();

    GameInfo gi;
    gi.name = name;
    gi.appId = appId;
    gi.status = IsGameInstalled(libPaths, appId) ? "installed" : "not_installed";
    gi.playtime = 0;
    gi.size = 0;
    gi.achievements = 0;
    gi.achievementsTotal = 0;
    gi.hasTradingCards = false;
    games_.push_back(std::move(gi));

    // 请求游戏图片
    RequestGameTexture(appId);
    Log(LogLevel::Info, "AddGameToList: added %s (%s)", name.c_str(), appId.c_str());
}

void GameListSidebar::UpdateGameInstallStatus(const std::string& appId, bool installed) {
    for (auto& game : games_) {
        if (game.appId == appId) {
            game.status = installed ? "installed" : "not_installed";
            break;
        }
    }
}

void GameListSidebar::SelectGameByAppId(const std::string& appId) {
    for (int i = 0; i < (int)games_.size(); i++) {
        if (games_[i].appId == appId) {
            selectedGame_ = i;
            break;
        }
    }
}

void GameListSidebar::Clear() {
    Log(LogLevel::Info, "GameListSidebar::Clear: starting...");
    selectedGame_ = -1;
    contextMenuGame_ = -1;
    loadedSteamId_.clear();
    memset(searchBuf_, 0, sizeof(searchBuf_));
    games_.clear();  // Clear games last
    Log(LogLevel::Info, "GameListSidebar::Clear: done");
}

static void GetGameIconColors(const std::string& appId, ImU32& c1, ImU32& c2) {
    unsigned h = 0;
    for (char c : appId) h = h * 2654435761u + c;

    float hue = (h % 360) / 360.0f;
    float sat = 0.55f + ((h >> 12) % 20) / 100.0f;

    auto hsl = [](float hh, float ss, float ll) -> ImU32 {
        float c = (1.0f - fabsf(2.0f * ll - 1.0f)) * ss;
        float x = c * (1.0f - fabsf(fmodf(hh * 6.0f, 2.0f) - 1.0f));
        float m = ll - c / 2.0f;
        float r, g, b;
        int s = (int)(hh * 6.0f) % 6;
        switch (s) {
            case 0: r=c;g=x;b=0; break; case 1: r=x;g=c;b=0; break;
            case 2: r=0;g=c;b=x; break; case 3: r=0;g=x;b=c; break;
            case 4: r=x;g=0;b=c; break; default:r=c;g=0;b=x; break;
        }
        return IM_COL32((int)((r+m)*255),(int)((g+m)*255),(int)((b+m)*255),255);
    };

    c1 = hsl(hue, sat, 0.30f);
    c2 = hsl(hue, sat, 0.18f);
}

void GameListSidebar::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 29, 38, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##GameList", nullptr, flags)) {
        // Parse raw Steam JSON on main thread (if background fetch completed)
        PollSteamLibrary();
        // Refresh game list from parsed data
        RefreshFromSteamLibrary();

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // ---- Home button ----
        float rowH = 36.0f;
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##home", ImVec2(width * 0.4f, rowH));
        bool homeHov = ImGui::IsItemHovered();
        bool homeClk = ImGui::IsItemClicked();
        if (homeHov)
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width * 0.4f, y + rowH), IM_COL32(255, 255, 255, 10));
        if (homeClk) selectedGame_ = -1;

        if (g_iconFont) DrawIcon(dl, icon::HOME, ImVec2(x + 12, y + 10), IM_COL32(180, 195, 210, homeHov ? 255 : 180));
        dl->AddText(ImVec2(x + 34, y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
                    IM_COL32(199, 213, 224, homeHov ? 255 : 200), u8"\u4E3B\u9875"); // 主页

        // View mode buttons
        float iconBtnX = x + width - 56;
        if (g_iconFont) {
            for (int i = 0; i < 2; i++) {
                ImVec2 bp(iconBtnX + i * 26, y + 6);
                ImGui::SetCursorPos(ImVec2(iconBtnX - x + i * 26, 6));
                ImGui::PushID(300 + i);
                ImGui::InvisibleButton("##vb", ImVec2(24, 24));
                bool vh = ImGui::IsItemHovered();
                ImGui::PopID();
                if (vh) dl->AddRectFilled(bp, ImVec2(bp.x + 24, bp.y + 24), IM_COL32(255, 255, 255, 15), 4.0f);
                const char* ic = (i == 0) ? icon::GRIDVIEW : icon::LISTVIEW;
                DrawIcon(dl, ic, ImVec2(bp.x + 4, bp.y + 4), IM_COL32(160, 170, 180, vh ? 240 : 140));
            }
        }

        // ---- Filter dropdown ----
        ImGui::SetCursorPos(ImVec2(6, rowH + 4));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(30, 38, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(38, 48, 62, 255));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(30, 38, 50, 240));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
        ImGui::SetNextItemWidth(width - 12);
        const char* filters[] = {
            u8"\u6E38\u620F\u548C\u8F6F\u4EF6",   // 游戏和软件
            u8"\u6E38\u620F",                       // 游戏
            u8"\u8F6F\u4EF6",                       // 软件
            u8"\u5DF2\u5B89\u88C5"                  // 已安装
        };
        ImGui::Combo("##filter", &filterMode_, filters, IM_ARRAYSIZE(filters));
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        // ---- Search bar ----
        float searchY = rowH + 32;
        ImGui::SetCursorPos(ImVec2(6, searchY));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(18, 24, 34, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(28, 36, 48, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(28, 36, 48, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(28, 6));
        ImGui::SetNextItemWidth(width - 12);
        ImGui::InputTextWithHint("##search", u8"\u641C\u7D22...", searchBuf_, sizeof(searchBuf_)); // 搜索...
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        if (g_iconFont)
            DrawIcon(dl, icon::SEARCH, ImVec2(x + 14, y + searchY + 4), IM_COL32(100, 120, 140, 180));

        // ---- Section header ----
        float listStartY = searchY + 34;
        char sectionBuf[128];
        snprintf(sectionBuf, sizeof(sectionBuf), u8"\u5168\u90E8\u6E38\u620F (%d)", (int)games_.size()); // 全部游戏
        if (g_mainFontSmall) {
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                        ImVec2(x + 12, y + listStartY + 2), IM_COL32(100, 120, 140, 160), sectionBuf);
        } else {
            dl->AddText(ImVec2(x + 12, y + listStartY + 2), IM_COL32(100, 120, 140, 160), sectionBuf);
        }
        listStartY += 22;

        dl->AddRectFilledMultiColor(
            ImVec2(x + 10, y + listStartY - 4), ImVec2(x + width - 10, y + listStartY - 3),
            IM_COL32(60, 75, 90, 0), IM_COL32(60, 75, 90, 80),
            IM_COL32(60, 75, 90, 80), IM_COL32(60, 75, 90, 0));

        // ---- Game List ----
        ImGui::SetCursorPos(ImVec2(0, listStartY));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::BeginChild("##glist", ImVec2(width, height - listStartY), false);

        // Use child window's DrawList for proper clipping
        ImDrawList* childDl = ImGui::GetWindowDrawList();

        float dt = ImGui::GetIO().DeltaTime;
        std::string filter(searchBuf_);
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        // Use content region width so items don't overlap the scrollbar
        float itemW = ImGui::GetContentRegionAvail().x;

        bool openContextMenu = false;
        int visIdx = 0;
        for (int i = 0; i < (int)games_.size(); i++) {
            if (!filter.empty()) {
                std::string nameLow = games_[i].name;
                std::transform(nameLow.begin(), nameLow.end(), nameLow.begin(), ::tolower);
                if (nameLow.find(filter) == std::string::npos) continue;
            }

            bool isSelected = (selectedGame_ == i);
            float itemH = 42.0f;  // Taller rows for bigger thumbnails

            ImVec2 itemPos = ImGui::GetCursorScreenPos();
            ImGui::PushID(i);
            ImGui::InvisibleButton("##gi", ImVec2(itemW, itemH));
            bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) selectedGame_ = i;
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                selectedGame_ = i;
                contextMenuGame_ = i;
                openContextMenu = true;
            }
            ImGui::PopID();

            int animIdx = visIdx % 64;
            float target = hovered ? 1.0f : 0.0f;
            hoverAnim_[animIdx] += (target - hoverAnim_[animIdx]) * dt * 12.0f;
            float anim = hoverAnim_[animIdx];

            // Selected background
            if (isSelected) {
                childDl->AddRectFilled(itemPos, ImVec2(itemPos.x + itemW, itemPos.y + itemH),
                                  IM_COL32(30, 50, 70, 255));
                childDl->AddRectFilled(itemPos, ImVec2(itemPos.x + 3, itemPos.y + itemH),
                                  IM_COL32(102, 192, 244, 255));
            } else if (anim > 0.01f) {
                childDl->AddRectFilled(itemPos, ImVec2(itemPos.x + itemW, itemPos.y + itemH),
                                  IM_COL32(255, 255, 255, (int)(12 * anim)));
            }

            // Game icon — BIGGER: 56x26
            float iconW = 56.0f;
            float iconH = 26.0f;
            float iconX = itemPos.x + 8;
            float iconY = itemPos.y + (itemH - iconH) * 0.5f;

            ImTextureID iconTex = GetGameTexture(games_[i].appId);
            if (iconTex) {
                childDl->AddImageRounded(iconTex, ImVec2(iconX, iconY), ImVec2(iconX + iconW, iconY + iconH),
                                     ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 3.0f);
                childDl->AddRect(ImVec2(iconX, iconY), ImVec2(iconX + iconW, iconY + iconH),
                            IM_COL32(255, 255, 255, 25), 3.0f);
            } else {
                ImU32 ic1, ic2;
                GetGameIconColors(games_[i].appId, ic1, ic2);
                childDl->AddRectFilledMultiColor(ImVec2(iconX, iconY), ImVec2(iconX + iconW, iconY + iconH),
                                            ic1, ic2, ic2, ic1);
                childDl->AddRect(ImVec2(iconX, iconY), ImVec2(iconX + iconW, iconY + iconH),
                            IM_COL32(255, 255, 255, 15), 3.0f);
                char firstLetter[4] = {};
                // Copy first UTF-8 char (could be multi-byte for CJK)
                const char* src = games_[i].name.c_str();
                int len = 1;
                unsigned char first = (unsigned char)src[0];
                if (first >= 0xC0 && first < 0xE0) len = 2;
                else if (first >= 0xE0 && first < 0xF0) len = 3;
                else if (first >= 0xF0) len = 4;
                for (int b = 0; b < len && src[b]; b++) firstLetter[b] = src[b];
                if (g_mainFontSmall) {
                    ImVec2 ls = g_mainFontSmall->CalcTextSizeA(g_mainFontSmall->FontSize, FLT_MAX, 0, firstLetter);
                    childDl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                                ImVec2(iconX + (iconW - ls.x) * 0.5f, iconY + (iconH - ls.y) * 0.5f),
                                IM_COL32(255, 255, 255, 200), firstLetter);
                }
            }

            // Game name
            ImU32 nameCol;
            if (games_[i].status == "playing") {
                nameCol = IM_COL32(144, 186, 60, 255);
            } else if (isSelected) {
                nameCol = IM_COL32(255, 255, 255, 255);
            } else if (games_[i].status == "not_installed") {
                nameCol = IM_COL32(130, 145, 160, 160);
            } else {
                nameCol = IM_COL32(190, 205, 220, (int)(180 + 60 * anim));
            }

            float nameX = itemPos.x + 72;
            ImFont* nameFont = g_mainFont ? g_mainFont : ImGui::GetFont();
            // Clip name to not overflow (leave room for status indicator + scrollbar)
            float maxNameW = itemW - 72 - 28;

            // Calculate wrapped text height for vertical centering
            ImVec2 textSize = nameFont->CalcTextSizeA(nameFont->FontSize, FLT_MAX, maxNameW, games_[i].name.c_str());
            float textH = textSize.y;
            float nameY = itemPos.y + (itemH - textH) * 0.5f;

            // Use ImGui's text wrapping for multi-line support with vertical centering
            childDl->AddText(nameFont, nameFont->FontSize, ImVec2(nameX, nameY),
                        nameCol, games_[i].name.c_str(), nullptr, maxNameW);

            // Status indicators
            if (games_[i].status == "playing") {
                float dotX = itemPos.x + itemW - 18;
                float dotY = itemPos.y + itemH * 0.5f;
                childDl->AddCircleFilled(ImVec2(dotX, dotY), 4.0f, IM_COL32(144, 186, 60, 255));
                childDl->AddCircleFilled(ImVec2(dotX, dotY), 6.0f, IM_COL32(144, 186, 60, 40));
            } else if (games_[i].status == "not_installed") {
                if (g_iconFont)
                    DrawIcon(childDl, icon::DOWNLOAD, ImVec2(itemPos.x + itemW - 24, itemPos.y + 13),
                             IM_COL32(100, 120, 140, 80));
            }

            visIdx++;
        }

        // ---- Right-click context menu ----
        if (openContextMenu)
            ImGui::OpenPopup("##game_ctx");

        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24, 30, 42, 250));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0)); // we draw our own hover
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210, 220, 230, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 75, 100, 120));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 0), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::BeginPopup("##game_ctx")) {
            if (contextMenuGame_ >= 0 && contextMenuGame_ < (int)games_.size()) {
                auto& game = games_[contextMenuGame_];
                bool isInstalled = (game.status != "not_installed");
                float menuW = ImGui::GetContentRegionAvail().x;
                ImDrawList* mdl = ImGui::GetWindowDrawList();

                // Outer shadow (drawn behind popup)
                ImVec2 wMin = ImGui::GetWindowPos();
                ImVec2 wMax = ImVec2(wMin.x + ImGui::GetWindowWidth(), wMin.y + ImGui::GetWindowHeight());
                for (int s = 1; s <= 6; s++) {
                    ImU32 sc = IM_COL32(0, 0, 0, (int)(40.0f * (1.0f - s / 7.0f)));
                    mdl->AddRect(ImVec2(wMin.x - s, wMin.y - s), ImVec2(wMax.x + s, wMax.y + s), sc, 6.0f + s);
                }

                // Layout constants
                constexpr float iconColW = 32.0f;  // fixed icon column
                constexpr float textX = 38.0f;     // text start
                constexpr float itemH = 28.0f;
                constexpr float iconSz = 18.0f;    // larger icons

                // Game name header (left-aligned, bold style)
                {
                    ImGui::SetCursorPosX(8);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(102, 192, 244, 255));
                    if (g_mainFont) ImGui::PushFont(g_mainFont);
                    ImGui::TextUnformatted(game.name.c_str());
                    if (g_mainFont) ImGui::PopFont();
                    ImGui::PopStyleColor();
                }
                // Thin separator line
                {
                    ImVec2 cp = ImGui::GetCursorScreenPos();
                    mdl->AddRectFilledMultiColor(
                        ImVec2(cp.x + 4, cp.y + 2), ImVec2(cp.x + menuW - 4, cp.y + 3),
                        IM_COL32(70, 90, 120, 0), IM_COL32(70, 90, 120, 140),
                        IM_COL32(70, 90, 120, 140), IM_COL32(70, 90, 120, 0));
                    ImGui::Dummy(ImVec2(0, 6));
                }

                bool steamOk = IsSteamInstalled();

                // Menu item helper: left-aligned icon + text, custom hover
                // disabled=true shows greyed out with tooltip
                auto MenuItem = [&](const char* iconGlyph, const char* label,
                                    ImU32 iconCol, ImU32 iconColHov,
                                    bool disabled = false) -> bool {
                    ImGui::PushID(label);
                    ImVec2 csp = ImGui::GetCursorScreenPos();

                    if (disabled) {
                        // Greyed-out item, still show but not clickable
                        ImGui::BeginDisabled(true);
                        ImGui::Selectable("##mi", false, 0, ImVec2(menuW, itemH));
                        ImGui::EndDisabled();
                        bool hov = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
                        ImVec2 rMin = ImGui::GetItemRectMin();
                        ImVec2 rMax = ImGui::GetItemRectMax();

                        // Icon (dimmed)
                        if (g_iconFont) {
                            ImVec2 isz = g_iconFont->CalcTextSizeA(iconSz, FLT_MAX, 0, iconGlyph);
                            float ix = rMin.x + (iconColW - isz.x) * 0.5f + 4;
                            float iy = rMin.y + (itemH - isz.y) * 0.5f;
                            mdl->AddText(g_iconFont, iconSz, ImVec2(ix, iy),
                                         IM_COL32(80, 90, 100, 100), iconGlyph);
                        }

                        // Label (dimmed) + "需要Steam" suffix
                        ImVec2 lsz = ImGui::CalcTextSize(label);
                        float ty = rMin.y + (itemH - lsz.y) * 0.5f;
                        mdl->AddText(ImVec2(rMin.x + textX, ty),
                                     IM_COL32(100, 110, 125, 120), label);

                        // Small hint text
                        if (g_mainFontSmall) {
                            const char* hint = u8"\u9700\u8981Steam"; // 需要Steam
                            ImVec2 hsz = g_mainFontSmall->CalcTextSizeA(
                                g_mainFontSmall->FontSize, FLT_MAX, 0, hint);
                            mdl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                                         ImVec2(rMin.x + menuW - hsz.x - 8, ty + 2),
                                         IM_COL32(255, 150, 50, 100), hint);
                        }

                        // Tooltip on hover
                        if (hov) {
                            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(30, 35, 48, 240));
                            ImGui::SetTooltip(u8"\u8BF7\u5148\u5B89\u88C5 Steam \u5BA2\u6237\u7AEF"); // 请先安装 Steam 客户端
                            ImGui::PopStyleColor();
                        }

                        ImGui::PopID();
                        return false;
                    }

                    bool clicked = ImGui::Selectable("##mi", false, 0, ImVec2(menuW, itemH));
                    bool hov = ImGui::IsItemHovered();
                    ImVec2 rMin = ImGui::GetItemRectMin();
                    ImVec2 rMax = ImGui::GetItemRectMax();

                    // Hover highlight: rounded, gradient
                    if (hov) {
                        mdl->AddRectFilled(rMin, rMax,
                                           IM_COL32(60, 90, 140, 80), 4.0f);
                        mdl->AddRectFilled(rMin, ImVec2(rMin.x + 3, rMax.y),
                                           IM_COL32(102, 192, 244, 200), 2.0f);
                    }

                    // Icon (centered in icon column)
                    if (g_iconFont) {
                        ImVec2 isz = g_iconFont->CalcTextSizeA(iconSz, FLT_MAX, 0, iconGlyph);
                        float ix = rMin.x + (iconColW - isz.x) * 0.5f + 4;
                        float iy = rMin.y + (itemH - isz.y) * 0.5f;
                        ImU32 ic = hov ? iconColHov : iconCol;
                        mdl->AddText(g_iconFont, iconSz, ImVec2(ix, iy), ic, iconGlyph);
                    }

                    // Label text
                    ImVec2 lsz = ImGui::CalcTextSize(label);
                    float ty = rMin.y + (itemH - lsz.y) * 0.5f;
                    ImU32 tc = hov ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 220, 230, 230);
                    mdl->AddText(ImVec2(rMin.x + textX, ty), tc, label);

                    ImGui::PopID();
                    return clicked;
                };

                // Thin separator helper
                auto Sep = [&]() {
                    ImVec2 cp = ImGui::GetCursorScreenPos();
                    mdl->AddRectFilledMultiColor(
                        ImVec2(cp.x + 4, cp.y + 1), ImVec2(cp.x + menuW - 4, cp.y + 2),
                        IM_COL32(70, 90, 120, 0), IM_COL32(70, 90, 120, 100),
                        IM_COL32(70, 90, 120, 100), IM_COL32(70, 90, 120, 0));
                    ImGui::Dummy(ImVec2(0, 4));
                };

                const std::string& aid = game.appId;

                // Custom vector icon drawing helpers (SVG-quality)
                auto DrawInstallIcon = [&](ImVec2 center, float sz, ImU32 col) {
                    float r = sz * 0.5f;
                    // Arrow shaft (down)
                    mdl->AddLine(ImVec2(center.x, center.y - r * 0.7f),
                                 ImVec2(center.x, center.y + r * 0.2f), col, 2.0f);
                    // Arrow head
                    mdl->AddTriangleFilled(
                        ImVec2(center.x, center.y + r * 0.55f),
                        ImVec2(center.x - r * 0.4f, center.y + r * 0.05f),
                        ImVec2(center.x + r * 0.4f, center.y + r * 0.05f), col);
                    // Tray/base (U shape)
                    ImVec2 p1(center.x - r * 0.65f, center.y + r * 0.25f);
                    ImVec2 p2(center.x - r * 0.65f, center.y + r * 0.75f);
                    ImVec2 p3(center.x + r * 0.65f, center.y + r * 0.75f);
                    ImVec2 p4(center.x + r * 0.65f, center.y + r * 0.25f);
                    mdl->AddLine(p1, p2, col, 2.0f);
                    mdl->AddLine(p2, p3, col, 2.0f);
                    mdl->AddLine(p3, p4, col, 2.0f);
                };

                auto DrawPlayIcon = [&](ImVec2 center, float sz, ImU32 col) {
                    float r = sz * 0.45f;
                    // Filled play triangle (right-pointing)
                    mdl->AddTriangleFilled(
                        ImVec2(center.x - r * 0.5f, center.y - r),
                        ImVec2(center.x - r * 0.5f, center.y + r),
                        ImVec2(center.x + r * 0.8f, center.y), col);
                    // Subtle outline for depth
                    mdl->AddTriangle(
                        ImVec2(center.x - r * 0.5f, center.y - r),
                        ImVec2(center.x - r * 0.5f, center.y + r),
                        ImVec2(center.x + r * 0.8f, center.y),
                        IM_COL32(255, 255, 255, 30), 1.0f);
                };

                auto DrawVerifyIcon = [&](ImVec2 center, float sz, ImU32 col) {
                    float r = sz * 0.45f;
                    // Shield outline
                    ImVec2 top(center.x, center.y - r);
                    ImVec2 bl(center.x - r * 0.75f, center.y - r * 0.3f);
                    ImVec2 br(center.x + r * 0.75f, center.y - r * 0.3f);
                    ImVec2 bot(center.x, center.y + r);
                    mdl->AddQuadFilled(top, br, bot, bl, IM_COL32(
                        (col >> 0) & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF, 40));
                    mdl->AddLine(top, br, col, 1.5f);
                    mdl->AddLine(br, bot, col, 1.5f);
                    mdl->AddLine(bot, bl, col, 1.5f);
                    mdl->AddLine(bl, top, col, 1.5f);
                    // Check mark inside
                    mdl->AddLine(ImVec2(center.x - r * 0.3f, center.y),
                                 ImVec2(center.x - r * 0.05f, center.y + r * 0.3f), col, 2.0f);
                    mdl->AddLine(ImVec2(center.x - r * 0.05f, center.y + r * 0.3f),
                                 ImVec2(center.x + r * 0.35f, center.y - r * 0.2f), col, 2.0f);
                };

                auto DrawUninstallIcon = [&](ImVec2 center, float sz, ImU32 col) {
                    float r = sz * 0.42f;
                    // Trash can lid
                    mdl->AddLine(ImVec2(center.x - r * 0.8f, center.y - r * 0.55f),
                                 ImVec2(center.x + r * 0.8f, center.y - r * 0.55f), col, 2.0f);
                    mdl->AddLine(ImVec2(center.x - r * 0.25f, center.y - r * 0.55f),
                                 ImVec2(center.x + r * 0.25f, center.y - r * 0.55f), col, 2.5f);
                    // Handle on top
                    mdl->AddLine(ImVec2(center.x - r * 0.2f, center.y - r * 0.85f),
                                 ImVec2(center.x + r * 0.2f, center.y - r * 0.85f), col, 1.5f);
                    mdl->AddLine(ImVec2(center.x - r * 0.2f, center.y - r * 0.85f),
                                 ImVec2(center.x - r * 0.2f, center.y - r * 0.55f), col, 1.5f);
                    mdl->AddLine(ImVec2(center.x + r * 0.2f, center.y - r * 0.85f),
                                 ImVec2(center.x + r * 0.2f, center.y - r * 0.55f), col, 1.5f);
                    // Body (trapezoid)
                    mdl->AddLine(ImVec2(center.x - r * 0.65f, center.y - r * 0.4f),
                                 ImVec2(center.x - r * 0.5f, center.y + r * 0.85f), col, 1.5f);
                    mdl->AddLine(ImVec2(center.x - r * 0.5f, center.y + r * 0.85f),
                                 ImVec2(center.x + r * 0.5f, center.y + r * 0.85f), col, 1.5f);
                    mdl->AddLine(ImVec2(center.x + r * 0.5f, center.y + r * 0.85f),
                                 ImVec2(center.x + r * 0.65f, center.y - r * 0.4f), col, 1.5f);
                    // Vertical lines inside
                    mdl->AddLine(ImVec2(center.x, center.y - r * 0.2f),
                                 ImVec2(center.x, center.y + r * 0.65f), col, 1.0f);
                    mdl->AddLine(ImVec2(center.x - r * 0.3f, center.y - r * 0.15f),
                                 ImVec2(center.x - r * 0.25f, center.y + r * 0.6f), col, 1.0f);
                    mdl->AddLine(ImVec2(center.x + r * 0.3f, center.y - r * 0.15f),
                                 ImVec2(center.x + r * 0.25f, center.y + r * 0.6f), col, 1.0f);
                };

                // Enhanced MenuItem with custom draw icon instead of font glyph
                auto MenuItemCustom = [&](auto drawFn, const char* label,
                                          ImU32 iconCol, ImU32 iconColHov,
                                          bool disabled = false) -> bool {
                    ImGui::PushID(label);
                    if (disabled) {
                        ImGui::BeginDisabled(true);
                        ImGui::Selectable("##mi", false, 0, ImVec2(menuW, itemH));
                        ImGui::EndDisabled();
                        bool hov = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
                        ImVec2 rMin = ImGui::GetItemRectMin();
                        ImVec2 rMax = ImGui::GetItemRectMax();

                        ImVec2 iconCenter(rMin.x + iconColW * 0.5f + 4, rMin.y + itemH * 0.5f);
                        drawFn(iconCenter, iconSz, IM_COL32(80, 90, 100, 100));

                        ImVec2 lsz = ImGui::CalcTextSize(label);
                        float ty = rMin.y + (itemH - lsz.y) * 0.5f;
                        mdl->AddText(ImVec2(rMin.x + textX, ty),
                                     IM_COL32(100, 110, 125, 120), label);
                        if (g_mainFontSmall) {
                            const char* hint = u8"\u9700\u8981Steam";
                            ImVec2 hsz = g_mainFontSmall->CalcTextSizeA(
                                g_mainFontSmall->FontSize, FLT_MAX, 0, hint);
                            mdl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                                         ImVec2(rMin.x + menuW - hsz.x - 8, ty + 2),
                                         IM_COL32(255, 150, 50, 100), hint);
                        }
                        if (hov) {
                            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(30, 35, 48, 240));
                            ImGui::SetTooltip(u8"\u8BF7\u5148\u5B89\u88C5 Steam \u5BA2\u6237\u7AEF");
                            ImGui::PopStyleColor();
                        }
                        ImGui::PopID();
                        return false;
                    }

                    bool clicked = ImGui::Selectable("##mi", false, 0, ImVec2(menuW, itemH));
                    bool hov = ImGui::IsItemHovered();
                    ImVec2 rMin = ImGui::GetItemRectMin();
                    ImVec2 rMax = ImGui::GetItemRectMax();

                    if (hov) {
                        mdl->AddRectFilled(rMin, rMax, IM_COL32(60, 90, 140, 80), 4.0f);
                        mdl->AddRectFilled(rMin, ImVec2(rMin.x + 3, rMax.y),
                                           IM_COL32(102, 192, 244, 200), 2.0f);
                    }

                    ImVec2 iconCenter(rMin.x + iconColW * 0.5f + 4, rMin.y + itemH * 0.5f);
                    drawFn(iconCenter, iconSz, hov ? iconColHov : iconCol);

                    ImVec2 lsz = ImGui::CalcTextSize(label);
                    float ty = rMin.y + (itemH - lsz.y) * 0.5f;
                    ImU32 tc = hov ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 220, 230, 230);
                    mdl->AddText(ImVec2(rMin.x + textX, ty), tc, label);

                    ImGui::PopID();
                    return clicked;
                };

                if (isInstalled) {
                    // -- Primary actions (need Steam) --
                    bool isRunning = GameProcessManager::Get().IsGameRunning(aid);
                    if (isRunning) {
                        // 游戏正在运行，显示停止按钮
                        if (MenuItemCustom(DrawPlayIcon, u8"\u505C\u6B62\u6E38\u620F",               // 停止游戏
                                     IM_COL32(230, 90, 90, 255), IM_COL32(255, 120, 120, 255),
                                     false)) {
                            Navigate(NavAction::StopGame, aid);
                            contextMenuGame_ = -1;
                        }
                    } else {
                        // 游戏未运行，显示启动按钮
                        if (MenuItemCustom(DrawPlayIcon, u8"\u542F\u52A8\u6E38\u620F",               // 启动游戏
                                     IM_COL32(120, 200, 50, 255), IM_COL32(160, 230, 70, 255),
                                     !steamOk)) {
                            Navigate(NavAction::LaunchGame, aid);
                            contextMenuGame_ = -1;
                        }
                    }
                    if (MenuItemCustom(DrawVerifyIcon, u8"\u9A8C\u8BC1\u6E38\u620F\u6587\u4EF6", // 验证游戏文件
                                 IM_COL32(80, 160, 220, 255), IM_COL32(102, 192, 244, 255),
                                 !steamOk)) {
                        Navigate(NavAction::ValidateGame, aid);
                        contextMenuGame_ = -1;
                    }
                    Sep();
                    // -- Info actions (always available) --
                    if (MenuItem(icon::SETTINGS, u8"\u5C5E\u6027",                               // 属性
                                 IM_COL32(140, 155, 175, 220), IM_COL32(200, 215, 235, 255))) {
                        Navigate(NavAction::ViewProperties, aid);
                        contextMenuGame_ = -1;
                    }
                    if (MenuItem(icon::INFO, u8"\u67E5\u770B\u5546\u5E97\u9875\u9762",           // 查看商店页面
                                 IM_COL32(140, 155, 175, 220), IM_COL32(200, 215, 235, 255))) {
                        Navigate(NavAction::ViewStorePage, aid);
                        contextMenuGame_ = -1;
                    }
                    if (MenuItem(icon::FOLDER, u8"\u6D4F\u89C8\u672C\u5730\u6587\u4EF6",         // 浏览本地文件
                                 IM_COL32(140, 155, 175, 220), IM_COL32(200, 215, 235, 255),
                                 !steamOk)) {
                        Navigate(NavAction::BrowseLocalFiles, aid);
                        contextMenuGame_ = -1;
                    }
                    Sep();
                    // -- Destructive (need Steam) --
                    if (MenuItemCustom(DrawUninstallIcon, u8"\u5378\u8F7D",                      // 卸载
                                 IM_COL32(180, 70, 70, 220), IM_COL32(230, 90, 90, 255),
                                 !steamOk)) {
                        Navigate(NavAction::UninstallGame, aid);
                        contextMenuGame_ = -1;
                    }
                } else {
                    // ── Prominent Install Button ──
                    {
                        constexpr float installBtnH = 36.0f;
                        ImGui::PushID("install_btn");
                        ImVec2 btnPos = ImGui::GetCursorScreenPos();

                        bool installDisabled = !steamOk;
                        if (installDisabled) ImGui::BeginDisabled(true);
                        bool installClicked = ImGui::Selectable("##install", false, 0,
                                                                ImVec2(menuW, installBtnH));
                        if (installDisabled) ImGui::EndDisabled();
                        bool installHov = installDisabled
                            ? ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)
                            : ImGui::IsItemHovered();
                        ImVec2 iMin = ImGui::GetItemRectMin();
                        ImVec2 iMax = ImGui::GetItemRectMax();

                        // Gradient background
                        if (!installDisabled) {
                            ImU32 bgL = installHov ? IM_COL32(26, 130, 230, 140) : IM_COL32(26, 100, 200, 60);
                            ImU32 bgR = installHov ? IM_COL32(26, 100, 200, 100) : IM_COL32(26, 80, 160, 30);
                            mdl->AddRectFilledMultiColor(iMin, iMax, bgL, bgR, bgR, bgL);
                            mdl->AddRectFilled(iMin, ImVec2(iMin.x + 3, iMax.y),
                                               IM_COL32(66, 165, 245, installHov ? 255 : 180), 2.0f);
                        }

                        // Custom install icon (centered, larger)
                        float bigIconSz = 22.0f;
                        ImVec2 iconCenter(iMin.x + 22, iMin.y + installBtnH * 0.5f);
                        ImU32 iCol = installDisabled ? IM_COL32(80, 90, 100, 100)
                                    : installHov     ? IM_COL32(120, 210, 255, 255)
                                                     : IM_COL32(66, 165, 245, 255);
                        DrawInstallIcon(iconCenter, bigIconSz, iCol);

                        // Label
                        const char* installLabel = u8"\u5B89\u88C5\u6E38\u620F"; // 安装游戏
                        ImVec2 lsz = ImGui::CalcTextSize(installLabel);
                        float ty = iMin.y + (installBtnH - lsz.y) * 0.5f;
                        if (installDisabled) {
                            mdl->AddText(ImVec2(iMin.x + 40, ty),
                                         IM_COL32(100, 110, 125, 120), installLabel);
                            if (g_mainFontSmall) {
                                const char* hint = u8"\u9700\u8981Steam";
                                ImVec2 hsz = g_mainFontSmall->CalcTextSizeA(
                                    g_mainFontSmall->FontSize, FLT_MAX, 0, hint);
                                mdl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                                             ImVec2(iMin.x + menuW - hsz.x - 8, ty + 2),
                                             IM_COL32(255, 150, 50, 100), hint);
                            }
                        } else {
                            ImU32 tc = installHov ? IM_COL32(255, 255, 255, 255)
                                                  : IM_COL32(210, 230, 250, 240);
                            mdl->AddText(ImVec2(iMin.x + 40, ty), tc, installLabel);
                        }

                        if (installClicked && !installDisabled) {
                            Navigate(NavAction::InstallGame, aid, game.name, game.size);
                            contextMenuGame_ = -1;
                        }
                        ImGui::PopID();
                    }
                    Sep();
                    if (MenuItem(icon::INFO, u8"\u67E5\u770B\u5546\u5E97\u9875\u9762",           // 查看商店页面
                                 IM_COL32(140, 155, 175, 220), IM_COL32(200, 215, 235, 255))) {
                        Navigate(NavAction::ViewStorePage, aid);
                        contextMenuGame_ = -1;
                    }
                }
            }

            ImGui::EndPopup();
        } else {
            contextMenuGame_ = -1;
        }

        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(4);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Right edge shadow
        dl->AddRectFilledMultiColor(
            ImVec2(x + width - 3, y), ImVec2(x + width, y + height),
            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 40),
            IM_COL32(0, 0, 0, 40), IM_COL32(0, 0, 0, 0));
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace sf
