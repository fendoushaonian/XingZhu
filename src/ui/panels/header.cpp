#include "ui/panels/header.h"
#include "ui/iconfonts.h"
#include "app/config.h"
#include "core/auth.h"
#include "core/texture_manager.h"
#include "ui/toast.h"
#include <algorithm>
#include <cmath>
#include <windows.h>
#include <shellapi.h>

namespace sf {

// ==================== SVG Style Icon - Coin (星铸币) ====================
static void DrawIconCoin(ImDrawList* dl, float cx, float cy, float size, ImU32 color) {
    float s = size * 0.45f;

    // 外圈
    dl->AddCircle(ImVec2(cx, cy), s, color, 20, 2.0f);

    // 内圈
    dl->AddCircle(ImVec2(cx, cy), s * 0.7f, color, 16, 1.5f);

    // 中间星星
    float starR = s * 0.4f;
    ImVec2 starPts[10];
    for (int i = 0; i < 10; i++) {
        float angle = -3.14159f / 2 + i * 3.14159f / 5;
        float r = (i % 2 == 0) ? starR : starR * 0.4f;
        starPts[i] = ImVec2(cx + cosf(angle) * r, cy + sinf(angle) * r);
    }
    dl->AddConvexPolyFilled(starPts, 10, color);
}

// ==================== App Logo Icon ====================
// Draws the app logo from login.png image (cover mode - square crop)

static void DrawAppLogo(ImDrawList* dl, float cx, float cy, float size) {
    static ImTextureID logoTex = (ImTextureID)0;
    static int logoW = 0, logoH = 0;

    // 每次尝试加载（如果还没加载成功）
    if (!logoTex) {
        logoTex = LoadTextureFromLocalFile("header_logo", "resources/icons/login.png");
        GetTextureSize("header_logo", logoW, logoH);
    }

    if (logoTex && logoW > 0 && logoH > 0) {
        // Cover mode: draw as square, crop excess
        float drawSize = size * 2;
        float x0 = cx - drawSize * 0.5f;
        float y0 = cy - drawSize * 0.5f;

        // Calculate UV to crop center square from image
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        float aspect = (float)logoW / (float)logoH;
        if (aspect > 1.0f) {
            // Wider than tall - crop left/right
            float visibleFrac = 1.0f / aspect;
            float offset = (1.0f - visibleFrac) * 0.5f;
            u0 = offset;
            u1 = 1.0f - offset;
        } else if (aspect < 1.0f) {
            // Taller than wide - crop top/bottom
            float visibleFrac = aspect;
            float offset = (1.0f - visibleFrac) * 0.5f;
            v0 = offset;
            v1 = 1.0f - offset;
        }

        dl->AddImageRounded(logoTex, ImVec2(x0, y0), ImVec2(x0 + drawSize, y0 + drawSize),
                            ImVec2(u0, v0), ImVec2(u1, v1), IM_COL32(255, 255, 255, 255), 4.0f);
    }
}

// ==================== Header Bar (Single Row) ====================

void HeaderBar::Render(float width, float height) {
    float barH = height;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, barH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 26, 36, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##Header", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float dt = ImGui::GetIO().DeltaTime;
        float centerY = barH * 0.5f;

        // ============================================================
        //  LEFT: App Logo + Brand Name
        // ============================================================

        // Draw the app logo
        DrawAppLogo(dl, 28, centerY, 18.0f);

        // Brand name: "星铸畅玩"
        if (g_mainFontLarge) {
            dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                        ImVec2(52, (barH - g_mainFontLarge->FontSize) * 0.5f),
                        IM_COL32(102, 192, 244, 230),
                        u8"\u661F\u94F8\u7545\u73A9"); // 星铸畅玩
        }

        // ============================================================
        //  CENTER-LEFT: ← → Navigation + Tabs
        // ============================================================

        float navStartX = 130;

        // Back / Forward
        if (g_iconFont) {
            float navX = navStartX;
            const char* navIcons[] = { icon::BACK, icon::FORWARD };
            for (int i = 0; i < 2; i++) {
                float bw = 28.0f;
                ImGui::SetCursorPos(ImVec2(navX, 0));
                ImGui::PushID(200 + i);
                ImGui::InvisibleButton("##nav", ImVec2(bw, barH));
                bool nh = ImGui::IsItemHovered();
                ImGui::PopID();

                if (nh)
                    dl->AddRectFilled(ImVec2(navX, 0), ImVec2(navX + bw, barH),
                                      IM_COL32(255, 255, 255, 10));
                DrawIcon(dl, navIcons[i], ImVec2(navX + 6, centerY - 8),
                         nh ? IM_COL32(255, 255, 255, 220) : IM_COL32(90, 105, 120, 120));
                navX += bw;
            }
        }

        // Subtle separator after nav arrows
        dl->AddLine(ImVec2(navStartX + 58, 12), ImVec2(navStartX + 58, barH - 12),
                    IM_COL32(80, 95, 110, 50));

        // Main tabs: 商店 / 库 / 社区 / 工具箱
        struct TabDef { const char* label; const char* icon; MainTab tab; };
        TabDef tabs[] = {
            { u8"\u5546\u5E97",   icon::STORE,     MainTab::Store },     // 商店
            { u8"\u5E93",         icon::LIBRARY,   MainTab::Library },   // 库
            { u8"\u793E\u533A",   icon::COMMUNITY, MainTab::Community }, // 社区
            { u8"\u5DE5\u5177",   icon::SETTINGS,  MainTab::Tools },     // 工具
        };

        float tabX = navStartX + 64;
        ImFont* tabFont = g_mainFont ? g_mainFont : ImGui::GetFont();

        for (int i = 0; i < 4; i++) {
            bool isActive = (activeTab_ == tabs[i].tab);
            ImVec2 labelSize = tabFont->CalcTextSizeA(tabFont->FontSize, FLT_MAX, 0, tabs[i].label);
            float tabW = labelSize.x + 46;

            ImGui::SetCursorPos(ImVec2(tabX, 0));
            ImGui::PushID(tabs[i].label);
            ImGui::InvisibleButton("##tab", ImVec2(tabW, barH));
            bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) activeTab_ = tabs[i].tab;
            ImGui::PopID();

            // Animation
            float target = (isActive || hovered) ? 1.0f : 0.0f;
            tabAnim_[i] += (target - tabAnim_[i]) * dt * 10.0f;
            tabAnim_[i] = std::clamp(tabAnim_[i], 0.0f, 1.0f);

            // Active background
            if (isActive) {
                dl->AddRectFilled(ImVec2(tabX, 0), ImVec2(tabX + tabW, barH),
                                  IM_COL32(30, 42, 58, 255));
            } else if (tabAnim_[i] > 0.01f) {
                dl->AddRectFilled(ImVec2(tabX, 0), ImVec2(tabX + tabW, barH),
                                  IM_COL32(255, 255, 255, (int)(8 * tabAnim_[i])));
            }

            // Active underline
            if (isActive) {
                dl->AddRectFilled(ImVec2(tabX, barH - 3), ImVec2(tabX + tabW, barH),
                                  IM_COL32(102, 192, 244, 255));
            }

            // Icon + Label
            float ly = (barH - labelSize.y) * 0.5f;
            ImU32 col = isActive ? IM_COL32(255, 255, 255, 255) :
                        IM_COL32(160, 175, 190, (int)(100 + 120 * tabAnim_[i]));

            if (g_iconFont)
                DrawIcon(dl, tabs[i].icon, ImVec2(tabX + 12, ly - 1), col);
            dl->AddText(tabFont, tabFont->FontSize,
                        ImVec2(tabX + 30, ly), col, tabs[i].label);

            tabX += tabW + 2;
        }

        // ============================================================
        //  RIGHT: Bell + Currency + User Profile
        // ============================================================

        // Notification bell (rightmost)
        float bellX = width - 40;
        if (g_iconFont) {
            ImGui::SetCursorPos(ImVec2(bellX, 0));
            ImGui::PushID("notif_bell");
            ImGui::InvisibleButton("##bell", ImVec2(30, barH));
            bool bellHov = ImGui::IsItemHovered();
            ImGui::PopID();

            if (bellHov)
                dl->AddRectFilled(ImVec2(bellX, 0), ImVec2(bellX + 30, barH),
                                  IM_COL32(255, 255, 255, 10));

            DrawIcon(dl, icon::BELL, ImVec2(bellX + 7, centerY - 8),
                     IM_COL32(160, 175, 190, bellHov ? 255 : 130));
            // Red notification dot
            dl->AddCircleFilled(ImVec2(bellX + 19, centerY - 7), 3.5f, IM_COL32(220, 50, 40, 220));
        }

        // ============================================================
        //  Currency display (星铸币)
        // ============================================================
        float coinAreaRight = bellX - 15;
        int coinValue = 0;
        if (IsLoggedIn()) {
            coinValue = GetCurrentUser().coins;
        }
        char coinBuf[32];
        snprintf(coinBuf, sizeof(coinBuf), "%d", coinValue);
        ImVec2 coinTextSize = tabFont->CalcTextSizeA(tabFont->FontSize, FLT_MAX, 0, coinBuf);
        float coinBlockW = 24 + coinTextSize.x + 8;  // icon + text + gap
        float coinAreaLeft = coinAreaRight - coinBlockW;

        ImGui::SetCursorPos(ImVec2(coinAreaLeft, 0));
        ImGui::PushID("currency_area");
        ImGui::InvisibleButton("##coin", ImVec2(coinBlockW, barH));
        bool coinHov = ImGui::IsItemHovered();
        ImGui::PopID();

        if (coinHov)
            dl->AddRectFilled(ImVec2(coinAreaLeft, 0), ImVec2(coinAreaRight, barH),
                              IM_COL32(255, 255, 255, 10));

        // 货币图标
        DrawIconCoin(dl, coinAreaLeft + 12, centerY, 20.0f, IM_COL32(255, 200, 50, coinHov ? 255 : 200));

        // 货币数值
        dl->AddText(tabFont, tabFont->FontSize,
                    ImVec2(coinAreaLeft + 26, (barH - tabFont->FontSize) * 0.5f),
                    IM_COL32(255, 200, 50, coinHov ? 255 : 200), coinBuf);

        // Separator
        dl->AddLine(ImVec2(coinAreaLeft - 8, 12), ImVec2(coinAreaLeft - 8, barH - 12),
                    IM_COL32(80, 95, 110, 50));

        // ============================================================
        //  User profile area (left of currency)
        // ============================================================
        float userAreaRight = coinAreaLeft - 16;

        // Get real username from auth system
        const char* userName = u8"\u661F\u94F8\u7528\u6237"; // 星铸用户 (fallback)
        std::string realName;
        if (IsLoggedIn()) {
            const auto& user = GetCurrentUser();
            realName = user.nickname.empty() ? user.username : user.nickname;
            userName = realName.c_str();
        }

        // 使用正常字体显示用户名
        ImFont* nameFont = g_mainFont ? g_mainFont : tabFont;

        // 用户名最大宽度限制
        float maxNameW = 80.0f;
        std::string displayName = userName;
        ImVec2 nameSize = nameFont->CalcTextSizeA(nameFont->FontSize, FLT_MAX, 0, userName);

        // 如果用户名太长，截断并添加省略号
        if (nameSize.x > maxNameW) {
            std::string truncated;
            float ellipsisW = nameFont->CalcTextSizeA(nameFont->FontSize, FLT_MAX, 0, "...").x;
            float targetW = maxNameW - ellipsisW;
            float currentW = 0;

            const char* p = userName;
            while (*p) {
                // 处理UTF-8多字节字符
                int charLen = 1;
                if ((*p & 0x80) == 0) charLen = 1;
                else if ((*p & 0xE0) == 0xC0) charLen = 2;
                else if ((*p & 0xF0) == 0xE0) charLen = 3;
                else if ((*p & 0xF8) == 0xF0) charLen = 4;

                std::string ch(p, charLen);
                float chW = nameFont->CalcTextSizeA(nameFont->FontSize, FLT_MAX, 0, ch.c_str()).x;

                if (currentW + chW > targetW) break;

                truncated += ch;
                currentW += chW;
                p += charLen;
            }
            truncated += "...";
            displayName = truncated;
            nameSize = nameFont->CalcTextSizeA(nameFont->FontSize, FLT_MAX, 0, displayName.c_str());
        }

        float avatarR = 16.0f;  // 头像半径
        float userBlockW = avatarR * 2 + 12 + nameSize.x + 24 + 20; // avatar + gap + name + arrow + padding
        float userAreaLeft = userAreaRight - userBlockW;

        if (userBlockW > 60) {
            ImGui::SetCursorPos(ImVec2(userAreaLeft, 0));
            ImGui::PushID("user_profile");
            ImGui::InvisibleButton("##user", ImVec2(userBlockW, barH));
            bool userHov = ImGui::IsItemHovered();
            bool userClicked = ImGui::IsItemClicked();
            ImGui::PopID();

            if (userClicked)
                userDropdownOpen_ = !userDropdownOpen_;

            // Close dropdown when clicking elsewhere
            if (userDropdownOpen_ && ImGui::IsMouseClicked(0) && !userHov) {
                // Check if click is inside dropdown area
                ImVec2 mp = ImGui::GetMousePos();
                float ddX = userAreaLeft;
                float ddY = barH;
                float ddW = 120;
                float ddH = 120;
                if (mp.x < ddX || mp.x > ddX + ddW || mp.y < ddY || mp.y > ddY + ddH)
                    userDropdownOpen_ = false;
            }

            if (userHov || userDropdownOpen_)
                dl->AddRectFilled(ImVec2(userAreaLeft, 0), ImVec2(userAreaRight, barH),
                                  IM_COL32(255, 255, 255, 8));

            // Position user info from the RIGHT side
            float uRight = userAreaRight - 8;

            // Dropdown arrow - 垂直居中
            if (g_iconFont) {
                const char* chevIcon = userDropdownOpen_ ? u8"\uE70E" : icon::CHEVDOWN; // up or down
                DrawIcon(dl, chevIcon, ImVec2(uRight - 14, centerY - 8),
                         IM_COL32(120, 135, 150, (userHov || userDropdownOpen_) ? 200 : 80));
            }

            // 用户名位置
            float nameRight = uRight - 24;
            float nameLeft = nameRight - nameSize.x;

            // 用户名 - 垂直水平居中
            float nameY = centerY - nameSize.y / 2;
            dl->AddText(nameFont, nameFont->FontSize,
                        ImVec2(nameLeft, nameY),
                        IM_COL32(200, 215, 230, (userHov || userDropdownOpen_) ? 255 : 190),
                        displayName.c_str());

            // Avatar circle - 垂直居中
            float avatarCX = nameLeft - avatarR - 10;
            float avatarCY = centerY;

            dl->AddCircleFilled(ImVec2(avatarCX, avatarCY), avatarR, IM_COL32(40, 70, 110, 255));
            dl->AddCircle(ImVec2(avatarCX, avatarCY), avatarR, IM_COL32(102, 192, 244, 80), 0, 2.0f);

            if (g_iconFont)
                DrawIcon(dl, icon::USER, ImVec2(avatarCX - 8, avatarCY - 8),
                         IM_COL32(130, 175, 215, 220));

            // Online status dot - 调整位置
            dl->AddCircleFilled(ImVec2(avatarCX + avatarR * 0.7f, avatarCY + avatarR * 0.7f), 5.5f, IM_COL32(20, 26, 36, 255));
            dl->AddCircleFilled(ImVec2(avatarCX + avatarR * 0.7f, avatarCY + avatarR * 0.7f), 4.0f, IM_COL32(87, 203, 100, 255));
        }

        // ============================================================
        //  USER DROPDOWN MENU
        // ============================================================
        float ddTargetAlpha = userDropdownOpen_ ? 1.0f : 0.0f;
        dropdownAnim_ += (ddTargetAlpha - dropdownAnim_) * dt * 12.0f;
        if (dropdownAnim_ < 0.01f) dropdownAnim_ = 0.0f;

        if (dropdownAnim_ > 0.01f) {
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            float ddW = 120.0f;
            float ddX = userAreaRight - ddW;
            float ddY = barH;
            float itemH = 32.0f;
            float ddH = itemH * 3 + 8; // 3 items + padding
            int alpha = (int)(dropdownAnim_ * 255);

            // Shadow
            fg->AddRectFilled(ImVec2(ddX + 3, ddY + 3), ImVec2(ddX + ddW + 3, ddY + ddH + 3),
                              IM_COL32(0, 0, 0, (int)(80 * dropdownAnim_)), 6.0f);
            // Background
            fg->AddRectFilled(ImVec2(ddX, ddY), ImVec2(ddX + ddW, ddY + ddH),
                              IM_COL32(28, 36, 50, alpha), 6.0f);
            // Border
            fg->AddRect(ImVec2(ddX, ddY), ImVec2(ddX + ddW, ddY + ddH),
                        IM_COL32(50, 70, 100, (int)(100 * dropdownAnim_)), 6.0f, 0, 1.0f);

            struct MenuItem { const char* icon; const char* label; int id; };
            MenuItem items[] = {
                { icon::USER,     u8"\u8D26\u6237", 0 },     // 账户
                { icon::STARFILL, u8"\u4F1A\u5458", 1 },     // 会员
                { icon::CLOSE,    u8"\u6CE8\u9500", 2 },     // 注销
            };

            ImVec2 mp = ImGui::GetMousePos();
            float iy = ddY + 4;

            for (int i = 0; i < 3; i++) {
                bool hov = (mp.x >= ddX && mp.x <= ddX + ddW && mp.y >= iy && mp.y < iy + itemH);

                if (hov) {
                    fg->AddRectFilled(ImVec2(ddX + 4, iy), ImVec2(ddX + ddW - 4, iy + itemH),
                                      IM_COL32(50, 70, 110, (int)(180 * dropdownAnim_)), 4.0f);
                }

                // Separator before 注销
                if (i == 2) {
                    fg->AddLine(ImVec2(ddX + 12, iy - 1), ImVec2(ddX + ddW - 12, iy - 1),
                                IM_COL32(60, 80, 110, (int)(60 * dropdownAnim_)));
                }

                ImU32 textCol = (i == 2)
                    ? IM_COL32(220, 80, 60, (int)(alpha * (hov ? 1.0f : 0.8f)))
                    : IM_COL32(200, 215, 230, (int)(alpha * (hov ? 1.0f : 0.75f)));

                if (g_iconFont)
                    DrawIcon(fg, items[i].icon, ImVec2(ddX + 16, iy + (itemH - 16) * 0.5f), textCol);

                if (tabFont)
                    fg->AddText(tabFont, tabFont->FontSize,
                                ImVec2(ddX + 40, iy + (itemH - tabFont->FontSize) * 0.5f),
                                textCol, items[i].label);

                // Handle click
                if (hov && ImGui::IsMouseClicked(0)) {
                    userDropdownOpen_ = false;
                    if (items[i].id == 0) { // 账户
                        profileClicked_ = true;
                    } else if (items[i].id == 1) { // 会员
                        vipClicked_ = true;
                    } else if (items[i].id == 2) { // 注销
                        logoutClicked_ = true;
                    }
                }

                iy += itemH;
            }
        }

        // ============================================================
        //  Bottom border
        // ============================================================
        dl->AddRectFilledMultiColor(
            ImVec2(0, barH - 1), ImVec2(width, barH),
            IM_COL32(0, 0, 0, 30), IM_COL32(0, 0, 0, 50),
            IM_COL32(0, 0, 0, 50), IM_COL32(0, 0, 0, 30));
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

bool HeaderBar::WasLogoutClicked() {
    bool v = logoutClicked_;
    logoutClicked_ = false;
    return v;
}

bool HeaderBar::WasProfileClicked() {
    bool v = profileClicked_;
    profileClicked_ = false;
    return v;
}

bool HeaderBar::WasVipClicked() {
    bool v = vipClicked_;
    vipClicked_ = false;
    return v;
}

// ==================== Status Bar ====================

void StatusBar::Render(float y, float width, float height) {
    float barH = height;

    ImGui::SetNextWindowPos(ImVec2(0, y));
    ImGui::SetNextWindowSize(ImVec2(width, barH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 25, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##StatusBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float ty = y;
        float textH = ImGui::GetTextLineHeight();
        float iconSize = 16.0f;
        float itemH = textH + 4;  // icon + text height

        dl->AddLine(ImVec2(0, ty), ImVec2(width, ty), IM_COL32(0, 0, 0, 60));

        // Left: Add Game (居左，垂直居中)
        float lx = 20;
        float itemY = y + (barH - itemH) * 0.5f;
        ImGui::SetCursorPos(ImVec2(lx, (barH - itemH) * 0.5f));
        ImGui::PushID("add_game");
        ImGui::InvisibleButton("##addg", ImVec2(100, itemH + 8));
        bool addHov = ImGui::IsItemHovered();
        bool addClicked = ImGui::IsItemClicked();
        ImGui::PopID();
        if (addClicked) {
            cdkPopupOpen_ = true;
            memset(cdkInput_, 0, sizeof(cdkInput_));
        }
        if (g_iconFont) DrawIcon(dl, icon::ADD, ImVec2(lx, itemY),
                                  IM_COL32(100, 120, 140, addHov ? 220 : 160));
        dl->AddText(ImVec2(lx + 22, itemY),
                    IM_COL32(100, 120, 140, addHov ? 220 : 160),
                    u8"\u6DFB\u52A0\u6E38\u620F"); // 添加游戏

        // Center: Downloads (居中，垂直居中)
        const char* dlText = u8"\u4E0B\u8F7D"; // 下载
        ImVec2 dlTextSize = ImGui::CalcTextSize(dlText);
        float dlTotalW = 20 + dlTextSize.x;  // icon + text
        float cx = (width - dlTotalW) * 0.5f;
        ImGui::SetCursorPos(ImVec2(cx, (barH - itemH) * 0.5f));
        ImGui::PushID("downloads_btn");
        ImGui::InvisibleButton("##dlb", ImVec2(dlTotalW + 10, itemH + 8));
        bool dlHov = ImGui::IsItemHovered();
        ImGui::PopID();
        if (g_iconFont) DrawIcon(dl, icon::DOWNLOAD, ImVec2(cx, itemY),
                                  IM_COL32(100, 120, 140, dlHov ? 220 : 160));
        dl->AddText(ImVec2(cx + 22, itemY),
                    IM_COL32(100, 120, 140, dlHov ? 220 : 160),
                    dlText);

        // Right: Friends & Chat (居右，垂直居中)
        const char* frText = u8"\u597D\u53CB\u4E0E\u804A\u5929"; // 好友与聊天
        ImVec2 frTextSize = ImGui::CalcTextSize(frText);
        float frTotalW = 20 + frTextSize.x + 30;  // icon + text + badge
        float rx = width - frTotalW - 20;
        ImGui::SetCursorPos(ImVec2(rx, (barH - itemH) * 0.5f));
        ImGui::PushID("friends_btn");
        ImGui::InvisibleButton("##frb", ImVec2(frTotalW, itemH + 8));
        bool frHov = ImGui::IsItemHovered();
        ImGui::PopID();
        if (g_iconFont) DrawIcon(dl, icon::PEOPLE, ImVec2(rx, itemY),
                                  IM_COL32(100, 120, 140, frHov ? 220 : 160));
        dl->AddText(ImVec2(rx + 22, itemY),
                    IM_COL32(100, 120, 140, frHov ? 220 : 160),
                    frText);

        // Friend count badge
        float badgeX = width - 28;
        float badgeY = itemY + 2;
        dl->AddRectFilled(ImVec2(badgeX - 14, badgeY), ImVec2(badgeX + 2, badgeY + 14),
                          IM_COL32(102, 192, 244, 60), 7.0f);
        if (g_mainFontSmall) {
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                        ImVec2(badgeX - 10, badgeY + 1), IM_COL32(102, 192, 244, 200), "3");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // 渲染CDK弹窗
    RenderCdkPopup();
}

void StatusBar::RenderCdkPopup() {
    if (!cdkPopupOpen_) return;

    ImGuiIO& io = ImGui::GetIO();
    float winW = io.DisplaySize.x;
    float winH = io.DisplaySize.y;

    // 弹窗尺寸
    float popupW = 420, popupH = 280;
    float popupX = (winW - popupW) * 0.5f;
    float popupY = (winH - popupH) * 0.5f;

    // 半透明背景遮罩
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 150));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##cdk_overlay", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    // 点击遮罩关闭
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x < popupX || mousePos.x > popupX + popupW ||
            mousePos.y < popupY || mousePos.y > popupY + popupH) {
            cdkPopupOpen_ = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // 弹窗主体
    ImGui::SetNextWindowPos(ImVec2(popupX, popupY));
    ImGui::SetNextWindowSize(ImVec2(popupW, popupH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(30, 35, 45, 250));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 70, 90, 200));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    ImGui::Begin(u8"添加游戏##cdk_popup", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);

    // 标题提示
    ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), u8"请填写星铸许可的CDK授权码");
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::TextColored(ImVec4(0.5f, 0.55f, 0.65f, 1.0f), u8"输入Steam产品密钥以激活游戏");
    ImGui::Dummy(ImVec2(0, 16));

    // CDK输入框
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 25, 35, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(25, 30, 40, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(30, 35, 50, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 80, 120, 150));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    ImGui::SetNextItemWidth(popupW - 48);
    ImGui::InputTextWithHint("##cdk_input", u8"XXXXX-XXXXX-XXXXX", cdkInput_, sizeof(cdkInput_));

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Dummy(ImVec2(0, 24));

    // 按钮区域 - 居中对齐
    float btnW = 100, btnH = 36;
    float btnSpacing = 16;
    float totalBtnW = btnW * 2 + btnSpacing;
    float contentWidth = popupW - 48;  // 减去左右padding
    float btnStartX = (contentWidth - totalBtnW) * 0.5f + 24;  // 24是左padding

    ImGui::SetCursorPosX(btnStartX);

    // 确定按钮
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 133, 244, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(86, 153, 255, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(46, 113, 224, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button(u8"确定", ImVec2(btnW, btnH))) {
        std::string cdk = cdkInput_;
        // 去除空格
        cdk.erase(std::remove(cdk.begin(), cdk.end(), ' '), cdk.end());

        if (cdk.empty()) {
            ShowToast(u8"请输入CDK授权码", ToastType::Warning);
        } else {
            // 调用Steam客户端激活CDK
            // steam://open/activateproduct 或 steam://registerkey/<key>
            std::string steamUrl = "steam://registerkey/" + cdk;
            ShellExecuteA(nullptr, "open", steamUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            ShowToast(u8"正在通过Steam激活CDK...", ToastType::Info);
            cdkPopupOpen_ = false;
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, btnSpacing);

    // 取消按钮
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 65, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(75, 80, 95, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 55, 70, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button(u8"取消", ImVec2(btnW, btnH))) {
        cdkPopupOpen_ = false;
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
