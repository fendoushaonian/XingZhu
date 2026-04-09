#include "ui/panels/store.h"
#include "app/config.h"
#include "app/application.h"
#include "ui/widgets/modern.h"
#include "ui/icons.h"
#include "ui/iconfonts.h"
#include "core/user_library.h"
#include "core/auth.h"
#include "ui/toast.h"
#include <cstdio>
#include <cstdlib>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

// 声明全局函数
void NotifyGameAddedToLibrary(const std::string& appId, const std::string& name);

// ═══════════════════════════════════════════════════════════════════════════
//  SVG 风格图标 - 畅玩图标（火箭/游戏手柄组合）
// ═══════════════════════════════════════════════════════════════════════════
static void DrawIconFreePlay(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float s = size * 0.5f;

    // 火箭主体 - 流线型设计
    // 火箭头部（三角形）
    ImVec2 tip(cx + s * 0.5f, cy - s * 0.1f);
    ImVec2 topLeft(cx - s * 0.1f, cy - s * 0.5f);
    ImVec2 botLeft(cx - s * 0.1f, cy + s * 0.3f);
    dl->AddTriangleFilled(tip, topLeft, botLeft, color);

    // 火箭身体（圆角矩形）
    dl->AddRectFilled(ImVec2(cx - s * 0.4f, cy - s * 0.4f),
                      ImVec2(cx + s * 0.1f, cy + s * 0.2f),
                      color, s * 0.15f);

    // 火箭窗口（小圆）
    dl->AddCircleFilled(ImVec2(cx - s * 0.15f, cy - s * 0.15f), s * 0.12f,
                        IM_COL32(20, 24, 32, 255));
    dl->AddCircle(ImVec2(cx - s * 0.15f, cy - s * 0.15f), s * 0.12f, color, 12, 1.5f);

    // 火箭尾翼
    dl->AddTriangleFilled(
        ImVec2(cx - s * 0.4f, cy + s * 0.1f),
        ImVec2(cx - s * 0.6f, cy + s * 0.5f),
        ImVec2(cx - s * 0.3f, cy + s * 0.3f),
        color);

    // 火焰效果（渐变三角形）
    ImU32 flameOuter = IM_COL32(255, 150, 50, 200);
    ImU32 flameInner = IM_COL32(255, 220, 100, 255);
    dl->AddTriangleFilled(
        ImVec2(cx - s * 0.35f, cy + s * 0.25f),
        ImVec2(cx - s * 0.5f, cy + s * 0.7f),
        ImVec2(cx - s * 0.2f, cy + s * 0.25f),
        flameOuter);
    dl->AddTriangleFilled(
        ImVec2(cx - s * 0.32f, cy + s * 0.28f),
        ImVec2(cx - s * 0.4f, cy + s * 0.55f),
        ImVec2(cx - s * 0.23f, cy + s * 0.28f),
        flameInner);

    // 速度线（表示快速）
    dl->AddLine(ImVec2(cx + s * 0.3f, cy - s * 0.4f),
                ImVec2(cx + s * 0.6f, cy - s * 0.55f),
                color, 1.5f);
    dl->AddLine(ImVec2(cx + s * 0.35f, cy - s * 0.15f),
                ImVec2(cx + s * 0.65f, cy - s * 0.25f),
                color, 1.5f);
    dl->AddLine(ImVec2(cx + s * 0.3f, cy + s * 0.1f),
                ImVec2(cx + s * 0.55f, cy + s * 0.05f),
                color, 1.5f);
}

// ==================== StorePanel ====================

StorePanel::StorePanel() {
    featured_ = {
        {{"Black Myth: Wukong",    "2358720", "installed", 45.2f, 130.0f, 20, 36, false}, "$59.99", "", false},
        {{"GTA VI",                "0000001", "not_installed", 0.0f, 150.0f, 0, 0, false}, "$69.99", "", false},
        {{"Elden Ring: Nightreign","1245621", "not_installed", 0.0f, 50.0f, 0, 0, false}, "$39.99", "", false},
    };
    specials_ = {
        {{"Cyberpunk 2077",     "1091500", "not_installed", 0, 70, 0, 53, false}, "$29.99", "-50%", false},
        {{"Red Dead Redemption 2","1174180","not_installed", 0, 120, 0, 52, false}, "$19.99", "-67%", false},
        {{"Hades II",           "1145361", "not_installed", 0, 20, 0, 0, false}, "$24.99", "-20%", false},
        {{"Hollow Knight: Silksong","481100","not_installed", 0, 15, 0, 0, false}, "$29.99", "-10%", false},
    };
    newReleases_ = {
        {{"Kingdom Come: Deliverance II", "2461430", "not_installed", 0, 50, 0, 0, false}, "$49.99", "", false},
        {{"Monster Hunter Wilds",  "2246340", "not_installed", 0, 60, 0, 0, false}, "$59.99", "", false},
        {{"Doom: The Dark Ages",   "0000002", "not_installed", 0, 80, 0, 0, false}, "$69.99", "", false},
    };

    // 初始化模拟数据
    onlineUsers_ = 1234 + rand() % 500;
    totalAccounts_ = 8888;
    idleAccounts_ = 3456 + rand() % 200;
}

void StorePanel::Render(float x, float y, float width, float height) {
    // 侧边栏宽度
    float sidebarW = 220.0f;

    // 渲染侧边栏
    RenderSidebar(x, y, sidebarW, height);

    // 渲染主内容区
    RenderContent(x + sidebarW, y, width - sidebarW, height);
}

void StorePanel::RenderSidebar(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 24, 32, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 16));

    if (ImGui::Begin("##StoreSidebar", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float dt = ImGui::GetIO().DeltaTime;

        // 获取窗口实际位置
        ImVec2 winPos = ImGui::GetWindowPos();
        float wx = winPos.x;
        float wy = winPos.y;

        // 更新统计数据（模拟实时变化）
        statsUpdateTimer_ += dt;
        if (statsUpdateTimer_ > 5.0f) {
            statsUpdateTimer_ = 0.0f;
            onlineUsers_ += (rand() % 21) - 10;  // -10 到 +10
            idleAccounts_ += (rand() % 11) - 5;  // -5 到 +5
            if (onlineUsers_ < 1000) onlineUsers_ = 1000;
            if (idleAccounts_ < 2000) idleAccounts_ = 2000;
        }

        // ═══════════════════════════════════════════════════════════════
        //  统计卡片
        // ═══════════════════════════════════════════════════════════════
        float cardX = wx + 12;
        float cardY = wy + 16;
        float cardW = width - 24;
        float cardH = 140.0f;

        // 卡片背景
        dl->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
                          IM_COL32(30, 38, 52, 255), 10.0f);
        dl->AddRect(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
                    IM_COL32(60, 80, 110, 80), 10.0f);

        // 卡片标题
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;

        dl->AddText(font, font->FontSize, ImVec2(cardX + 14, cardY + 12),
                    IM_COL32(102, 192, 244, 255), u8"平台状态");

        // 分隔线
        dl->AddLine(ImVec2(cardX + 14, cardY + 38), ImVec2(cardX + cardW - 14, cardY + 38),
                    IM_COL32(60, 80, 110, 100));

        // 统计项
        float statY = cardY + 48;
        float statH = 28.0f;

        // 在线人数
        dl->AddCircleFilled(ImVec2(cardX + 22, statY + statH / 2), 4.0f, IM_COL32(87, 203, 100, 255));
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(cardX + 34, statY + 4),
                    IM_COL32(160, 175, 190, 255), u8"在线人数");
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", onlineUsers_);
        ImVec2 numSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, buf);
        dl->AddText(font, font->FontSize, ImVec2(cardX + cardW - 14 - numSize.x, statY + 2),
                    IM_COL32(87, 203, 100, 255), buf);

        statY += statH;

        // 总账号数
        dl->AddCircleFilled(ImVec2(cardX + 22, statY + statH / 2), 4.0f, IM_COL32(102, 192, 244, 255));
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(cardX + 34, statY + 4),
                    IM_COL32(160, 175, 190, 255), u8"游戏账号");
        snprintf(buf, sizeof(buf), "%d", totalAccounts_);
        numSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, buf);
        dl->AddText(font, font->FontSize, ImVec2(cardX + cardW - 14 - numSize.x, statY + 2),
                    IM_COL32(102, 192, 244, 255), buf);

        statY += statH;

        // 空闲账号
        dl->AddCircleFilled(ImVec2(cardX + 22, statY + statH / 2), 4.0f, IM_COL32(230, 180, 50, 255));
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(cardX + 34, statY + 4),
                    IM_COL32(160, 175, 190, 255), u8"空闲账号");
        snprintf(buf, sizeof(buf), "%d", idleAccounts_);
        numSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, buf);
        dl->AddText(font, font->FontSize, ImVec2(cardX + cardW - 14 - numSize.x, statY + 2),
                    IM_COL32(230, 180, 50, 255), buf);

        // ═══════════════════════════════════════════════════════════════
        //  分类按钮
        // ═══════════════════════════════════════════════════════════════
        float btnY = cardY + cardH + 20;
        float btnW = cardW;
        float btnH = 44.0f;
        float btnGap = 8.0f;

        struct CategoryBtn {
            const char* label;
            const char* icon;  // nullptr 表示使用自定义图标
            StoreCategory cat;
            ImU32 color;
            bool useCustomIcon;
        };
        CategoryBtn buttons[] = {
            {u8"畅玩",     nullptr,        StoreCategory::FreePlay, IM_COL32(255, 180, 50, 255),  true},
            {u8"正版游戏", icon::STORE,    StoreCategory::Official, IM_COL32(102, 192, 244, 255), false},
            {u8"离线游戏", icon::DOWNLOAD, StoreCategory::Offline,  IM_COL32(87, 203, 100, 255),  false},
            {u8"盗版破解", icon::GAMEPAD,  StoreCategory::Cracked,  IM_COL32(230, 100, 100, 255), false},
        };

        for (int i = 0; i < 4; i++) {
            float bx = cardX;
            float by = btnY + i * (btnH + btnGap);
            bool isActive = (currentCategory_ == buttons[i].cat);

            ImGui::SetCursorScreenPos(ImVec2(bx, by));
            ImGui::PushID(i);
            ImGui::InvisibleButton("##catbtn", ImVec2(btnW, btnH));
            bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                currentCategory_ = buttons[i].cat;
            }
            ImGui::PopID();

            // 按钮背景
            ImU32 bgColor;
            if (isActive) {
                bgColor = IM_COL32(40, 55, 75, 255);
            } else if (hovered) {
                bgColor = IM_COL32(35, 45, 60, 255);
            } else {
                bgColor = IM_COL32(28, 36, 48, 255);
            }
            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + btnW, by + btnH), bgColor, 8.0f);

            // 选中指示条
            if (isActive) {
                dl->AddRectFilled(ImVec2(bx, by + 8), ImVec2(bx + 3, by + btnH - 8),
                                  buttons[i].color, 2.0f);
            }

            // 图标
            ImU32 iconColor = isActive ? buttons[i].color : IM_COL32(140, 155, 170, 200);
            if (buttons[i].useCustomIcon) {
                // 使用自定义 SVG 图标（畅玩火箭）
                DrawIconFreePlay(dl, bx + 22, by + btnH / 2, 28.0f, iconColor, 2.0f);
            } else if (g_iconFont && buttons[i].icon) {
                DrawIcon(dl, buttons[i].icon, ImVec2(bx + 14, by + (btnH - 16) / 2), iconColor);
            }

            // 文字
            ImU32 textColor = isActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 195, 210, 220);
            dl->AddText(font, font->FontSize, ImVec2(bx + 40, by + (btnH - font->FontSize) / 2),
                        textColor, buttons[i].label);

            // 悬停边框
            if (hovered && !isActive) {
                dl->AddRect(ImVec2(bx, by), ImVec2(bx + btnW, by + btnH),
                            IM_COL32(80, 100, 130, 100), 8.0f);
            }
        }

        // ═══════════════════════════════════════════════════════════════
        //  签到按钮
        // ═══════════════════════════════════════════════════════════════
        float checkInY = btnY + 4 * (btnH + btnGap) + 20;
        float checkInBtnW = cardW;
        float checkInBtnH = 44.0f;

        ImGui::SetCursorScreenPos(ImVec2(cardX, checkInY));
        ImGui::PushID("checkin_sidebar_btn");
        ImGui::InvisibleButton("##checkinbtn", ImVec2(checkInBtnW, checkInBtnH));
        bool checkInHovered = ImGui::IsItemHovered();
        bool checkInClicked = ImGui::IsItemClicked();
        ImGui::PopID();

        if (checkInClicked) {
            if (IsLoggedIn()) {
                if (auto* app = Application::GetInstance()) {
                    app->OpenCheckInPanel();
                }
            } else {
                ShowToast(u8"请先登录", ToastType::Warning);
            }
        }

        // 签到按钮背景 - 金色渐变
        ImU32 checkInBg = checkInHovered ? IM_COL32(60, 50, 30, 255) : IM_COL32(45, 40, 30, 255);
        dl->AddRectFilled(ImVec2(cardX, checkInY), ImVec2(cardX + checkInBtnW, checkInY + checkInBtnH),
                          checkInBg, 8.0f);

        // 金色边框
        dl->AddRect(ImVec2(cardX, checkInY), ImVec2(cardX + checkInBtnW, checkInY + checkInBtnH),
                    IM_COL32(255, 200, 50, checkInHovered ? 200 : 100), 8.0f);

        // 签到图标 (日历)
        if (g_iconFont) {
            DrawIcon(dl, icon::CALENDAR, ImVec2(cardX + 14, checkInY + (checkInBtnH - 16) / 2),
                     IM_COL32(255, 200, 50, checkInHovered ? 255 : 200));
        }

        // 签到文字
        ImU32 checkInTextColor = checkInHovered ? IM_COL32(255, 220, 100, 255) : IM_COL32(255, 200, 50, 220);
        dl->AddText(font, font->FontSize, ImVec2(cardX + 40, checkInY + (checkInBtnH - font->FontSize) / 2),
                    checkInTextColor, u8"每日签到");

        // 签到状态提示
        bool checkedToday = HasCheckedInToday();
        if (checkedToday) {
            ImVec2 statusSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, u8"已签");
            dl->AddText(smallFont, smallFont->FontSize,
                        ImVec2(cardX + checkInBtnW - statusSize.x - 12, checkInY + (checkInBtnH - smallFont->FontSize) / 2),
                        IM_COL32(87, 203, 100, 200), u8"已签");
        } else {
            // 红点提示
            dl->AddCircleFilled(ImVec2(cardX + checkInBtnW - 16, checkInY + checkInBtnH / 2), 5.0f,
                                IM_COL32(220, 50, 40, 220));
        }

        // ═══════════════════════════════════════════════════════════════
        //  底部提示
        // ═══════════════════════════════════════════════════════════════
        float tipY = wy + height - 60;
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(cardX + 8, tipY),
                    IM_COL32(100, 115, 130, 150), u8"提示: 正版游戏需要");
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(cardX + 8, tipY + 18),
                    IM_COL32(100, 115, 130, 150), u8"Steam 账号登录");
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void StorePanel::RenderContent(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##StoreContent", nullptr, flags)) {
        // 根据当前分类显示不同标题
        const char* title = u8"正版游戏商店";
        if (currentCategory_ == StoreCategory::FreePlay) {
            title = u8"畅玩";
        } else if (currentCategory_ == StoreCategory::Offline) {
            title = u8"离线游戏";
        } else if (currentCategory_ == StoreCategory::Cracked) {
            title = u8"盗版破解";
        }
        ui::SectionHeader(title);
        ImGui::Spacing();

        ImGui::BeginChild("##storeScroll", ImVec2(width - 48, height - 60), false);

        if (currentCategory_ == StoreCategory::FreePlay) {
            // 畅玩功能介绍
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();

            // 绘制大图标
            DrawIconFreePlay(dl, pos.x + 60, pos.y + 50, 80.0f, IM_COL32(255, 180, 50, 255), 3.0f);

            ImGui::SetCursorPosX(130);
            ImGui::SetCursorPosY(20);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 180, 50, 255));
            if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
            ImGui::Text(u8"畅玩模式");
            if (g_mainFontLarge) ImGui::PopFont();
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(130);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 195, 210, 220));
            ImGui::TextWrapped(u8"无需购买，即可畅玩海量正版游戏！");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::SetCursorPosY(120);

            // 功能特点卡片
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 215, 230, 255));

            ImGui::Bullet(); ImGui::SameLine();
            ImGui::TextWrapped(u8"共享账号池 - 使用平台提供的游戏账号畅玩");

            ImGui::Spacing();
            ImGui::Bullet(); ImGui::SameLine();
            ImGui::TextWrapped(u8"即点即玩 - 无需等待下载，云端秒开游戏");

            ImGui::Spacing();
            ImGui::Bullet(); ImGui::SameLine();
            ImGui::TextWrapped(u8"海量游戏 - 支持数千款热门正版游戏");

            ImGui::Spacing();
            ImGui::Bullet(); ImGui::SameLine();
            ImGui::TextWrapped(u8"存档同步 - 游戏进度自动保存到云端");

            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 180, 50, 200));
            ImGui::TextWrapped(u8"🚀 畅玩功能即将上线，敬请期待！");
            ImGui::PopStyleColor();

        } else if (currentCategory_ == StoreCategory::Official) {
            // 正版游戏内容
            RenderFeatured(width - 68);
            ImGui::Spacing();
            ImGui::Spacing();
            RenderSpecials(width - 68);
            ImGui::Spacing();
            ImGui::Spacing();
            RenderNewReleases(width - 68);
        } else if (currentCategory_ == StoreCategory::Offline) {
            // 离线游戏提示
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 175, 190, 200));
            ImGui::TextWrapped(u8"离线游戏功能正在开发中...");
            ImGui::TextWrapped(u8"此功能将允许您在没有网络连接的情况下游玩已下载的游戏。");
            ImGui::PopStyleColor();
        } else if (currentCategory_ == StoreCategory::Cracked) {
            // 盗版破解提示
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 100, 100, 200));
            ImGui::TextWrapped(u8"⚠ 警告：盗版游戏可能包含恶意软件，且违反版权法。");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 175, 190, 200));
            ImGui::TextWrapped(u8"此功能仅供学习研究使用，请支持正版游戏。");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void StorePanel::RenderFeatured(float width) {
    ui::SectionHeader("FEATURED & RECOMMENDED", IM_COL32(26, 159, 255, 255));
    ImGui::Spacing();

    for (auto& item : featured_) {
        ImGui::PushID(item.info.appId.c_str());

        ImGuiID id = ImGui::GetID("##feat");
        float& anim = ui::GetAnimState(id);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float cardH = 110.0f;

        ImGui::InvisibleButton("##feat", ImVec2(width, cardH));
        bool hovered = ImGui::IsItemHovered();

        float dt = ImGui::GetIO().DeltaTime;
        anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 10.0f;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Card bg with hover
        ImU32 bg = IM_COL32(22, 32, 45, (int)(180 + 75 * anim));
        dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + cardH), bg, 8.0f);

        if (anim > 0.01f) {
            dl->AddRect(cursor, ImVec2(cursor.x + width, cursor.y + cardH),
                        IM_COL32(26, 159, 255, (int)(60 * anim)), 8.0f, 0, 1.2f);
        }

        // Cover area with gradient
        unsigned hash = 0;
        for (char c : item.info.appId) hash = hash * 31 + c;
        ImU32 c1 = IM_COL32(30 + (hash % 50), 50 + ((hash >> 8) % 70), 90 + ((hash >> 16) % 70), 255);
        ImU32 c2 = IM_COL32(20 + (hash % 30), 35 + ((hash >> 8) % 50), 65 + ((hash >> 16) % 50), 255);

        dl->AddRectFilledMultiColor(
            ImVec2(cursor.x, cursor.y),
            ImVec2(cursor.x + 200, cursor.y + cardH),
            c1, c2, c2, c1
        );

        // Name on cover
        ImVec2 nameSize = ImGui::CalcTextSize(item.info.name.c_str());
        float nameY = cursor.y + (cardH - nameSize.y) * 0.5f;
        dl->AddText(ImVec2(cursor.x + 17, nameY + 1), IM_COL32(0, 0, 0, 120), item.info.name.c_str());
        dl->AddText(ImVec2(cursor.x + 16, nameY), IM_COL32(255, 255, 255, 220), item.info.name.c_str());

        // Info area
        dl->AddText(ImVec2(cursor.x + 220, cursor.y + 16),
                    IM_COL32(255, 255, 255, 240), item.info.name.c_str());

        // Tags
        ImVec2 tagPos(cursor.x + 220, cursor.y + 40);
        const char* tags[] = {"Action", "RPG", "Open World"};
        for (int t = 0; t < 3; t++) {
            ImVec2 tagSize = ImGui::CalcTextSize(tags[t]);
            float tw = tagSize.x + 12;
            float th = tagSize.y + 4;
            dl->AddRectFilled(tagPos, ImVec2(tagPos.x + tw, tagPos.y + th),
                              IM_COL32(42, 71, 94, 200), th * 0.5f);
            dl->AddText(ImVec2(tagPos.x + 6, tagPos.y + 2), IM_COL32(143, 152, 160, 220), tags[t]);
            tagPos.x += tw + 6;
        }

        // Price
        dl->AddText(ImVec2(cursor.x + 220, cursor.y + cardH - 30),
                    IM_COL32(87, 203, 100, 255), item.price.c_str());

        ImGui::PopID();
        ImGui::Spacing();
    }
}

void StorePanel::RenderSpecials(float width) {
    ui::SectionHeader("SPECIAL OFFERS", IM_COL32(76, 107, 34, 255));
    ImGui::Spacing();

    float cardW = 200.0f;
    float cardH = 90.0f;
    float spacing = 12.0f;

    int col = 0;
    int maxCols = std::max(1, (int)((width + spacing) / (cardW + spacing)));

    for (auto& item : specials_) {
        if (col > 0) ImGui::SameLine(0, spacing);

        ImGui::PushID(item.info.appId.c_str());

        ImGuiID id = ImGui::GetID("##spec");
        float& anim = ui::GetAnimState(id);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##spec", ImVec2(cardW, cardH));
        bool hovered = ImGui::IsItemHovered();

        float dt = ImGui::GetIO().DeltaTime;
        anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 10.0f;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(cursor, ImVec2(cursor.x + cardW, cursor.y + cardH),
                          IM_COL32(22, 32, 45, (int)(180 + 75 * anim)), 8.0f);

        if (anim > 0.01f) {
            dl->AddRect(cursor, ImVec2(cursor.x + cardW, cursor.y + cardH),
                        IM_COL32(76, 107, 34, (int)(80 * anim)), 8.0f, 0, 1.2f);
        }

        dl->AddText(ImVec2(cursor.x + 12, cursor.y + 12), IM_COL32(255, 255, 255, 230), item.info.name.c_str());

        // Discount badge
        if (!item.discount.empty()) {
            float badgeY = cursor.y + cardH - 30;
            ImVec2 discSize = ImGui::CalcTextSize(item.discount.c_str());
            float dw = discSize.x + 14;
            float dh = discSize.y + 8;
            dl->AddRectFilled(ImVec2(cursor.x + 12, badgeY),
                              ImVec2(cursor.x + 12 + dw, badgeY + dh),
                              IM_COL32(76, 107, 34, 255), dh * 0.5f);
            dl->AddText(ImVec2(cursor.x + 19, badgeY + 4),
                        IM_COL32(190, 230, 20, 255), item.discount.c_str());

            dl->AddText(ImVec2(cursor.x + 18 + dw, badgeY + 4),
                        IM_COL32(87, 203, 100, 255), item.price.c_str());
        }

        ImGui::PopID();
        col++;
        if (col >= maxCols) col = 0;
    }
}

void StorePanel::RenderNewReleases(float width) {
    ui::SectionHeader("NEW RELEASES", IM_COL32(102, 192, 244, 255));
    ImGui::Spacing();

    for (auto& item : newReleases_) {
        ImGui::PushID(item.info.appId.c_str());

        ImGuiID id = ImGui::GetID("##newrel");
        float& anim = ui::GetAnimState(id);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float h = 52.0f;

        ImGui::InvisibleButton("##newrel", ImVec2(width, h));
        bool hovered = ImGui::IsItemHovered();

        float dt = ImGui::GetIO().DeltaTime;
        anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 10.0f;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (anim > 0.01f) {
            dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + h),
                              IM_COL32(255, 255, 255, (int)(10 * anim)), 6.0f);
        }

        dl->AddText(ImVec2(cursor.x + 14, cursor.y + 8), IM_COL32(255, 255, 255, 230), item.info.name.c_str());
        dl->AddText(ImVec2(cursor.x + 14, cursor.y + 28), IM_COL32(143, 152, 160, 160), "Action, Adventure");
        dl->AddText(ImVec2(cursor.x + width - 80, cursor.y + 16), IM_COL32(87, 203, 100, 255), item.price.c_str());

        // Subtle separator
        dl->AddRectFilledMultiColor(
            ImVec2(cursor.x + 14, cursor.y + h - 1), ImVec2(cursor.x + width - 14, cursor.y + h),
            IM_COL32(42, 71, 94, 0), IM_COL32(42, 71, 94, 60),
            IM_COL32(42, 71, 94, 60), IM_COL32(42, 71, 94, 0)
        );

        ImGui::PopID();
    }
}

// ==================== FreeGamesPanel ====================

FreeGamesPanel::FreeGamesPanel() {
    freeGames_ = {
        {"Counter-Strike 2",          "730",    "game",     "Steam",  false},
        {"Dota 2",                     "570",    "game",     "Steam",  false},
        {"Apex Legends",               "1172470","game",     "Steam",  false},
        {"Path of Exile",              "238960", "game",     "Steam",  false},
        {"Team Fortress 2",            "440",    "game",     "Steam",  false},
        {"Warframe",                   "230410", "game",     "Steam",  false},
        {"Destiny 2",                  "1085660","game",     "Steam",  false},
        {"Lost Ark",                   "1599340","game",     "Steam",  false},
        {"Genshin Impact",             "2842960","game",     "Steam",  false},
        {"Wallpaper Engine - Free DLC","431960", "dlc",      "PICS",   false},
        {"Arma 3 Zeus DLC",            "275700", "dlc",      "Reddit", false},
        {"Payday 2 DLC Bundle",        "218620", "dlc",      "Reddit", false},
    };

    // 检查哪些游戏已经在用户库中
    for (auto& game : freeGames_) {
        if (UserLibrary::Get().HasGame(game.appId)) {
            game.claimed = true;
        }
    }
}

void FreeGamesPanel::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##FreeGames", nullptr, flags)) {
        ui::SectionHeader("FREE GAMES DISCOVERY");
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(143, 152, 160, 180));
        ImGui::Text("Auto-discover free games, DLCs, and software on Steam");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Spacing();

        // Action bar with modern buttons
        ui::ButtonPrimary("  Scan for Free Games", ImVec2(200, 38), icons::DrawSearch);
        ImGui::SameLine(0, 12);
        ui::ButtonSuccess("  Claim All", ImVec2(160, 38), icons::DrawCheck);

        ImGui::SameLine(0, 16);
        int unclaimed = 0;
        for (auto& g : freeGames_) if (!g.claimed) unclaimed++;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d unclaimed", unclaimed);
        ui::StatusDot(buf, IM_COL32(230, 180, 50, 255));

        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        // Modern table
        ImGui::BeginChild("##freeList", ImVec2(width - 48, height - 160), false);

        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32(18, 22, 28, 255));
        ImGui::PushStyleColor(ImGuiCol_TableRowBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, IM_COL32(255, 255, 255, 5));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 8));

        if (ImGui::BeginTable("##freeTable", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {

            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("App ID", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            for (auto& game : freeGames_) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", game.name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(143, 152, 160, 180));
                ImGui::Text("%s", game.appId.c_str());
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                if (game.type == "game") {
                    ImGui::PushID(game.appId.c_str());
                    ui::Badge("Game", IM_COL32(26, 130, 210, 200));
                    ImGui::PopID();
                } else if (game.type == "dlc") {
                    ImGui::PushID(game.appId.c_str());
                    ui::Badge("DLC", IM_COL32(180, 130, 20, 200));
                    ImGui::PopID();
                } else {
                    ImGui::Text("%s", game.type.c_str());
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", game.source.c_str());

                ImGui::TableSetColumnIndex(4);
                ImGui::PushID(game.appId.c_str());
                if (game.claimed) {
                    ui::StatusDot(u8"\u5DF2\u5165\u5E93", IM_COL32(87, 203, 100, 255)); // 已入库
                } else {
                    if (ui::ButtonSuccess(u8"\u5165\u5E93", ImVec2(70, 26))) { // 入库
                        // 添加到用户库
                        UserLibrary::Get().AddGame(game.appId, game.name, game.type, "free");
                        game.claimed = true;
                        // 通知侧边栏添加游戏
                        NotifyGameAddedToLibrary(game.appId, game.name);
                        ShowToast(u8"\u5DF2\u5C06 " + game.name + u8" \u6DFB\u52A0\u5230\u5E93", ToastType::Success); // 已将 xxx 添加到库
                    }
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace sf
