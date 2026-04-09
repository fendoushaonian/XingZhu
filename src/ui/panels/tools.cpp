#include "ui/panels/tools.h"
#include "app/config.h"
#include "app/application.h"
#include "ui/widgets/modern.h"
#include "ui/icons.h"
#include "core/texture_manager.h"
#include <cstdio>
#include <algorithm>

namespace sf {

// ==================== ToolsPanel (Hub) ====================

void ToolsPanel::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##Tools", nullptr, flags)) {
        // 标题
        ui::SectionHeader(u8"\u5DE5\u5177\u7BB1", IM_COL32(102, 192, 244, 255));  // 工具箱
        ImGui::Spacing();
        ImGui::Spacing();

        struct ToolCard {
            const char* name;
            const char* desc;
            const char* coverImage;  // 封面图路径
            icons::DrawFunc icon;
            ImU32 gradientTop;
            ImU32 gradientBottom;
        };

        ToolCard tools[] = {
            {u8"\u5185\u5B58\u4FEE\u6539\u5668",   // 内存修改器
             u8"\u626B\u63CF\u5E76\u4FEE\u6539\u6E38\u620F\u5185\u5B58\u6570\u636E\n\u8F7B\u677E\u5B9E\u73B0\u65E0\u9650\u8D44\u6E90",  // 扫描并修改游戏内存数据\n轻松实现无限资源
             "resources/tools/memory.png",
             icons::DrawMemory,
             IM_COL32(180, 60, 60, 255), IM_COL32(120, 30, 30, 255)},

            {u8"\u6302\u5361\u52A9\u624B",         // 挂卡助手
             u8"\u81EA\u52A8\u6302\u673A\u83B7\u53D6Steam\u96C6\u6362\u5361\n\u8F7B\u677E\u8D5A\u53D6\u989D\u5916\u6536\u76CA",  // 自动挂机获取Steam集换卡\n轻松赚取额外收益
             "resources/tools/cards.png",
             icons::DrawCards,
             IM_COL32(76, 130, 50, 255), IM_COL32(40, 80, 25, 255)},

            {u8"\u6210\u5C31\u7BA1\u7406",         // 成就管理
             u8"\u67E5\u770B\u3001\u89E3\u9501\u548C\u7BA1\u7406\u6E38\u620F\u6210\u5C31\n\u5B8C\u7F8E\u4E3B\u4E49\u8005\u5FC5\u5907",  // 查看、解锁和管理游戏成就\n完美主义者必备
             "resources/tools/achievement.png",
             icons::DrawAchievement,
             IM_COL32(26, 140, 220, 255), IM_COL32(15, 80, 140, 255)},

            {u8"\u4E91\u5B58\u6863\u7BA1\u7406",   // 云存档管理
             u8"\u5907\u4EFD\u548C\u7BA1\u7406Steam\u4E91\u5B58\u6863\n\u6570\u636E\u5B89\u5168\u6709\u4FDD\u969C",  // 备份和管理Steam云存档\n数据安全有保障
             "resources/tools/cloud.png",
             icons::DrawCloud,
             IM_COL32(100, 80, 180, 255), IM_COL32(60, 40, 120, 255)},

            {u8"\u514D\u8D39\u6E38\u620F",         // 免费游戏
             u8"\u53D1\u73B0\u5E76\u81EA\u52A8\u9886\u53D6\u514D\u8D39\u5185\u5BB9\n\u4E0D\u9519\u8FC7\u4EFB\u4F55\u798F\u5229",  // 发现并自动领取免费内容\n不错过任何福利
             "resources/tools/free.png",
             icons::DrawFreeGames,
             IM_COL32(200, 160, 40, 255), IM_COL32(140, 100, 20, 255)},

            {u8"\u7F51\u7EDC\u52A0\u901F",         // 网络加速
             u8"\u4F18\u5316Steam\u4E0B\u8F7D\u901F\u5EA6\u548C\u8DEF\u7531\n\u6781\u901F\u4E0B\u8F7D\u4F53\u9A8C",  // 优化Steam下载速度和路由\n极速下载体验
             "resources/tools/network.png",
             icons::DrawNetwork,
             IM_COL32(220, 100, 70, 255), IM_COL32(160, 50, 30, 255)},

            {u8"\u4E8C\u6B65\u9A8C\u8BC1",         // 二步验证
             u8"Steam Guard\u8EAB\u4EFD\u9A8C\u8BC1\u5668\n\u4FDD\u62A4\u8D26\u6237\u5B89\u5168",  // Steam Guard身份验证器\n保护账户安全
             "resources/tools/shield.png",
             icons::DrawShield,
             IM_COL32(60, 180, 130, 255), IM_COL32(30, 120, 80, 255)},

            {u8"\u4E2A\u4EBA\u8D44\u6599",         // 个人资料
             u8"\u81EA\u5B9A\u4E49\u4E2A\u4EBA\u8D44\u6599\u3001\u5C55\u67DC\u548C\u80CC\u666F\n\u6253\u9020\u4E13\u5C5E\u98CE\u683C",  // 自定义个人资料、展柜和背景\n打造专属风格
             "resources/tools/profile.png",
             icons::DrawUser,
             IM_COL32(180, 130, 70, 255), IM_COL32(120, 80, 40, 255)},

            {u8"\u5E02\u573A\u76D1\u63A7",         // 市场监控
             u8"\u76D1\u63A7Steam\u5E02\u573A\u4EF7\u683C\u53D8\u52A8\n\u667A\u80FD\u4E70\u5356\u8D5A\u5DEE\u4EF7",  // 监控Steam市场价格变动\n智能买卖赚差价
             "resources/tools/market.png",
             icons::DrawChart,
             IM_COL32(140, 80, 200, 255), IM_COL32(90, 40, 140, 255)},
        };

        // 卡片尺寸 - 更大更美观
        float cardW = 280.0f;
        float cardH = 180.0f;
        float coverH = 100.0f;  // 封面图高度
        float spacing = 20.0f;
        int maxCols = std::max(1, (int)((width - 48 + spacing) / (cardW + spacing)));

        int col = 0;
        int toolIndex = 0;
        for (auto& tool : tools) {
            if (col > 0) ImGui::SameLine(0, spacing);

            // 换行处理
            if (col == 0 && toolIndex > 0) {
                ImGui::Dummy(ImVec2(0, spacing));
            }

            ImGui::PushID(tool.name);

            ImGuiID id = ImGui::GetID("##toolcard");
            float& anim = ui::GetAnimState(id);

            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##toolcard", ImVec2(cardW, cardH));
            bool hovered = ImGui::IsItemHovered();
            bool clicked = ImGui::IsItemClicked();

            // 处理点击 - 打开对应工具
            if (clicked) {
                if (toolIndex == 0) {  // 内存修改器
                    Application::GetInstance()->OpenMemoryModifierPanel();
                }
                // 其他工具暂未实现
            }

            float dt = ImGui::GetIO().DeltaTime;
            anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 10.0f;
            anim = std::clamp(anim, 0.0f, 1.0f);

            ImDrawList* dl = ImGui::GetWindowDrawList();

            // 卡片阴影
            if (anim > 0.01f) {
                dl->AddRectFilled(
                    ImVec2(cursor.x + 4, cursor.y + 4),
                    ImVec2(cursor.x + cardW + 4, cursor.y + cardH + 4),
                    IM_COL32(0, 0, 0, (int)(40 * anim)), 12.0f);
            }

            // 卡片背景
            dl->AddRectFilled(cursor, ImVec2(cursor.x + cardW, cursor.y + cardH),
                              IM_COL32(30, 38, 50, 255), 12.0f);

            // 封面区域 - 渐变背景
            dl->AddRectFilledMultiColor(
                cursor,
                ImVec2(cursor.x + cardW, cursor.y + coverH),
                tool.gradientTop, tool.gradientTop,
                tool.gradientBottom, tool.gradientBottom);

            // 封面圆角裁剪
            dl->AddRectFilled(cursor, ImVec2(cursor.x + cardW, cursor.y + 12),
                              tool.gradientTop, 12.0f, ImDrawFlags_RoundCornersTop);

            // 封面图标 - 居中大图标
            float iconSize = 48.0f;
            float iconX = cursor.x + (cardW - iconSize) * 0.5f;
            float iconY = cursor.y + (coverH - iconSize) * 0.5f;

            // 图标背景光晕
            dl->AddCircleFilled(
                ImVec2(iconX + iconSize * 0.5f, iconY + iconSize * 0.5f),
                iconSize * 0.6f,
                IM_COL32(255, 255, 255, 30));

            // 绘制图标
            tool.icon(dl, ImVec2(iconX, iconY), iconSize, IM_COL32(255, 255, 255, 230));

            // 标题
            ImFont* font = ImGui::GetFont();
            float titleY = cursor.y + coverH + 12;
            dl->AddText(font, font->FontSize * 1.1f,
                        ImVec2(cursor.x + 16, titleY),
                        IM_COL32(255, 255, 255, 245), tool.name);

            // 描述
            float descY = titleY + font->FontSize * 1.1f + 6;
            dl->AddText(font, font->FontSize * 0.85f,
                        ImVec2(cursor.x + 16, descY),
                        IM_COL32(160, 170, 180, 200),
                        tool.desc, nullptr, cardW - 32);

            // 悬停边框
            if (anim > 0.01f) {
                dl->AddRect(cursor, ImVec2(cursor.x + cardW, cursor.y + cardH),
                            IM_COL32(102, 192, 244, (int)(150 * anim)),
                            12.0f, 0, 2.0f);
            }

            // 悬停时显示"打开"提示
            if (anim > 0.3f) {
                const char* openText = u8"\u70B9\u51FB\u6253\u5F00 \u2192";  // 点击打开 →
                ImVec2 textSize = font->CalcTextSizeA(font->FontSize * 0.9f, FLT_MAX, 0, openText);
                dl->AddText(font, font->FontSize * 0.9f,
                            ImVec2(cursor.x + cardW - textSize.x - 16, cursor.y + cardH - 28),
                            IM_COL32(102, 192, 244, (int)(220 * anim)),
                            openText);
            }

            ImGui::PopID();

            col++;
            toolIndex++;
            if (col >= maxCols) col = 0;
        }
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ==================== CardFarmerPanel ====================

CardFarmerPanel::CardFarmerPanel() {
    farmGames_ = {
        {"Counter-Strike 2",    "730",     3, 5,  2847.5f, false},
        {"Dota 2",              "570",     2, 8,  5621.0f, false},
        {"The Witcher 3",       "292030",  5, 5,  445.2f,  false},
        {"Terraria",            "105600",  1, 4,  678.9f,  false},
        {"Stardew Valley",      "413150",  4, 5,  234.0f,  false},
        {"Hades",               "1145360", 2, 3,  156.7f,  false},
        {"Path of Exile",       "238960",  6, 8,  1567.3f, false},
    };
}

void CardFarmerPanel::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##CardFarmer", nullptr, flags)) {
        ui::SectionHeader("TRADING CARD FARMER", IM_COL32(76, 107, 34, 255));

        ImGui::Spacing();

        int totalCards = 0;
        for (auto& g : farmGames_) totalCards += g.cardsRemaining;

        // Stats row with badges
        ui::Badge("Cards Left", IM_COL32(180, 130, 20, 200));
        ImGui::SameLine(0, 4);
        char numBuf[16];
        snprintf(numBuf, sizeof(numBuf), "%d", totalCards);
        ImGui::Text("%s", numBuf);

        ImGui::SameLine(0, 20);
        ui::Badge("Dropped", IM_COL32(76, 107, 34, 200));
        ImGui::SameLine(0, 4);
        snprintf(numBuf, sizeof(numBuf), "%d", totalCardsDropped_);
        ImGui::Text("%s", numBuf);

        ImGui::SameLine(0, 20);
        ui::Badge("Games", IM_COL32(26, 130, 210, 200));
        ImGui::SameLine(0, 4);
        snprintf(numBuf, sizeof(numBuf), "%d", (int)farmGames_.size());
        ImGui::Text("%s", numBuf);

        ImGui::Spacing();

        // Controls
        if (isRunning_) {
            ui::ButtonDanger("  Stop Farming", ImVec2(160, 38));
        } else {
            ui::ButtonSuccess("  Start Farming", ImVec2(160, 38), icons::DrawPlay);
        }

        ImGui::SameLine(0, 12);
        static int farmMode = 0;
        const char* modes[] = { "Normal (one at a time)", "Fast (all at once)", "Smart (2hr then single)" };
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(30, 36, 44, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(280);
        ImGui::Combo("##farmMode", &farmMode, modes, IM_ARRAYSIZE(modes));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        // Table
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32(18, 22, 28, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 8));

        if (ImGui::BeginTable("##farmTable", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {

            ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Cards", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Hours", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthFixed, 150);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableHeadersRow();

            for (auto& game : farmGames_) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", game.name.c_str());

                ImGui::TableSetColumnIndex(1);
                char cardBuf[32];
                snprintf(cardBuf, sizeof(cardBuf), "%d / %d", game.cardsTotal - game.cardsRemaining, game.cardsTotal);
                ImGui::Text("%s", cardBuf);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.0f", game.hoursPlayed);

                ImGui::TableSetColumnIndex(3);
                float progress = 1.0f - (float)game.cardsRemaining / (float)game.cardsTotal;
                ImU32 barCol = (game.cardsRemaining == 0) ? IM_COL32(255, 215, 0, 255) : IM_COL32(76, 107, 34, 255);
                char pctBuf[8];
                snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", progress * 100);
                ui::ProgressBar(progress, ImVec2(130, 16), barCol, 0, pctBuf);

                ImGui::TableSetColumnIndex(4);
                if (game.isFarming) {
                    ui::StatusDot("Active", IM_COL32(87, 203, 100, 255));
                } else if (game.cardsRemaining == 0) {
                    ui::StatusDot("Done", IM_COL32(143, 152, 160, 180));
                } else {
                    ui::StatusDot("Idle", IM_COL32(100, 100, 100, 180));
                }
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ==================== AchievementsPanel ====================

AchievementsPanel::AchievementsPanel() {
    games_ = {
        {"Counter-Strike 2",    "730",     98,  167},
        {"Dota 2",              "570",     156, 200},
        {"Elden Ring",          "1245620", 32,  42},
        {"Cyberpunk 2077",      "1091500", 44,  53},
        {"Baldur's Gate 3",     "1086940", 28,  54},
        {"The Witcher 3",       "292030",  52,  78},
        {"Terraria",            "105600",  87,  104},
        {"Stardew Valley",      "413150",  28,  40},
        {"Hades",               "1145360", 49,  49},
        {"Hollow Knight",       "367520",  56,  63},
    };
}

void AchievementsPanel::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##Achievements", nullptr, flags)) {
        ui::SectionHeader("ACHIEVEMENT MANAGER", IM_COL32(26, 159, 255, 255));
        ImGui::Spacing();

        // Overall stats
        int totalUnlocked = 0, totalAll = 0;
        int perfectCount = 0;
        for (auto& g : games_) {
            totalUnlocked += g.unlocked;
            totalAll += g.total;
            if (g.unlocked == g.total) perfectCount++;
        }

        // Stats circles
        float circleR = 36.0f;
        ui::ProgressCircle(totalAll > 0 ? (float)totalUnlocked / totalAll : 0, circleR,
                           IM_COL32(26, 159, 255, 255), 4.0f);

        ImGui::SameLine(0, 24);
        ImGui::BeginGroup();
        char statBuf[64];
        snprintf(statBuf, sizeof(statBuf), "%d / %d achievements", totalUnlocked, totalAll);
        ImGui::Text("%s", statBuf);
        snprintf(statBuf, sizeof(statBuf), "%d perfect games", perfectCount);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 215, 0, 255));
        ImGui::Text("%s", statBuf);
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        ImGui::BeginChild("##achList", ImVec2(width - 48, height - 150), false);

        for (auto& game : games_) {
            ImGui::PushID(game.appId.c_str());

            ImGuiID id = ImGui::GetID("##achRow");
            float& anim = ui::GetAnimState(id);

            float progress = (float)game.unlocked / (float)game.total;
            bool perfect = (game.unlocked == game.total);

            ImVec2 cursor = ImGui::GetCursorScreenPos();
            float rowH = 58.0f;

            ImGui::InvisibleButton("##achRow", ImVec2(width - 68, rowH));
            bool hovered = ImGui::IsItemHovered();

            float dt = ImGui::GetIO().DeltaTime;
            anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 10.0f;

            ImDrawList* dl = ImGui::GetWindowDrawList();

            if (anim > 0.01f) {
                dl->AddRectFilled(cursor, ImVec2(cursor.x + width - 68, cursor.y + rowH),
                                  IM_COL32(255, 255, 255, (int)(8 * anim)), 6.0f);
            }

            // Trophy icon for perfect games
            if (perfect) {
                icons::DrawAchievement(dl, ImVec2(cursor.x + 8, cursor.y + 12), 20,
                                       IM_COL32(255, 215, 0, 230));
            }

            // Game name
            float nameX = perfect ? cursor.x + 36 : cursor.x + 14;
            ImU32 nameCol = perfect ? IM_COL32(255, 215, 0, 255) : IM_COL32(255, 255, 255, 230);
            dl->AddText(ImVec2(nameX, cursor.y + 8), nameCol, game.name.c_str());

            // Count text
            char countBuf[32];
            snprintf(countBuf, sizeof(countBuf), "%d / %d", game.unlocked, game.total);
            dl->AddText(ImVec2(nameX, cursor.y + 30), IM_COL32(143, 152, 160, 180), countBuf);

            // Progress bar on right
            float barX = cursor.x + width - 320;
            float barW = 200.0f;
            float barH = 8.0f;
            float barY = cursor.y + rowH * 0.5f - barH * 0.5f;

            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                              IM_COL32(30, 36, 44, 255), 4.0f);
            ImU32 barCol = perfect ? IM_COL32(255, 215, 0, 255) : IM_COL32(26, 159, 255, 255);
            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * progress, barY + barH),
                              barCol, 4.0f);
            // Shine
            if (progress > 0.01f) {
                dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * progress, barY + barH * 0.4f),
                                  IM_COL32(255, 255, 255, 20), 4.0f);
            }

            // Percentage
            char pctBuf[16];
            snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", progress * 100);
            dl->AddText(ImVec2(barX + barW + 12, cursor.y + rowH * 0.5f - 8),
                        IM_COL32(199, 213, 224, 200), pctBuf);

            // Subtle separator
            dl->AddRectFilledMultiColor(
                ImVec2(cursor.x + 14, cursor.y + rowH - 1),
                ImVec2(cursor.x + width - 82, cursor.y + rowH),
                IM_COL32(42, 71, 94, 0), IM_COL32(42, 71, 94, 50),
                IM_COL32(42, 71, 94, 50), IM_COL32(42, 71, 94, 0)
            );

            ImGui::PopID();
        }

        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ==================== CloudSavesPanel ====================

CloudSavesPanel::CloudSavesPanel() {
    saves_ = {
        {"Elden Ring",          "1245620", "2026-03-09 14:30", 24.5f, false},
        {"Cyberpunk 2077",      "1091500", "2026-03-08 20:15", 156.2f, false},
        {"Baldur's Gate 3",     "1086940", "2026-03-07 11:00", 89.3f, true},
        {"Stardew Valley",      "413150",  "2026-03-09 09:45", 2.1f, false},
        {"Hades",               "1145360", "2026-03-06 18:20", 5.8f, false},
        {"The Witcher 3",       "292030",  "2026-02-28 22:10", 45.7f, false},
        {"Hollow Knight",       "367520",  "2026-03-05 16:40", 3.2f, false},
        {"Celeste",             "504230",  "2026-03-01 12:30", 0.8f, false},
    };
}

void CloudSavesPanel::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##CloudSaves", nullptr, flags)) {
        ui::SectionHeader("CLOUD SAVE MANAGER", IM_COL32(100, 65, 165, 255));
        ImGui::Spacing();

        ui::ButtonPrimary("  Sync All", ImVec2(120, 34), icons::DrawCloud);
        ImGui::SameLine(0, 10);
        ui::ButtonGhost("  Backup All", ImVec2(130, 34), icons::DrawDownload);
        ImGui::SameLine(0, 10);
        ui::ButtonGhost("Open Folder", ImVec2(120, 34));

        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32(18, 22, 28, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10, 8));

        if (ImGui::BeginTable("##saveTable", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {

            ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Last Sync", ImGuiTableColumnFlags_WidthFixed, 160);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 200);
            ImGui::TableHeadersRow();

            for (auto& save : saves_) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", save.gameName.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(143, 152, 160, 180));
                ImGui::Text("%s", save.lastSync.c_str());
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(2);
                char sizeBuf[32];
                snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", save.sizeMB);
                ImGui::Text("%s", sizeBuf);

                ImGui::TableSetColumnIndex(3);
                ImGui::PushID(save.appId.c_str());
                if (save.hasConflict) {
                    ui::StatusDot("Conflict", IM_COL32(220, 60, 60, 255));
                } else {
                    ui::StatusDot("Synced", IM_COL32(87, 203, 100, 255));
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(4);
                ImGui::PushID((save.appId + "_act").c_str());
                ui::ButtonGhost("DL", ImVec2(40, 24));
                ImGui::SameLine(0, 4);
                ui::ButtonGhost("UP", ImVec2(40, 24));
                ImGui::SameLine(0, 4);
                ui::ButtonGhost("Del", ImVec2(40, 24));
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace sf
