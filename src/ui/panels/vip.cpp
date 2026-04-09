#include "ui/panels/vip.h"
#include "ui/iconfonts.h"
#include "core/auth.h"
#include <algorithm>
#include <cmath>

namespace sf {

extern ImFont* g_mainFontLarge;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFont;
extern ImFont* g_iconFont;

void VipPanel::Open() {
    open_ = true;
    anim_ = 0.0f;
    selectedPlan_ = 0;
    statusMsg_.clear();
    msgTimer_ = 0.0f;
}

void VipPanel::Render(float winW, float winH) {
    if (!open_ && anim_ < 0.01f) return;

    float dt = ImGui::GetIO().DeltaTime;
    float target = open_ ? 1.0f : 0.0f;
    anim_ += (target - anim_) * dt * 14.0f;
    anim_ = std::clamp(anim_, 0.0f, 1.0f);
    if (!open_ && anim_ < 0.01f) { anim_ = 0.0f; return; }

    int overlayAlpha = (int)(120 * anim_);

    // Dark overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, overlayAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (ImGui::Begin("##VipOverlay", nullptr, overlayFlags)) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && ImGui::IsMouseClicked(0))
            open_ = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Horizontal panel: 620 x 360
    float pw = 620, ph = 360;
    float px = (winW - pw) * 0.5f;
    float py = (winH - ph) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(18, 22, 34, (int)(255 * anim_)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 70, 100, (int)(80 * anim_)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##VipPanel", nullptr, panelFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
        ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 mp = ImGui::GetMousePos();

        bool isVip = IsVipActive();
        const auto& user = GetCurrentUser();

        // Close button
        float closeX = wp.x + pw - 38, closeY = wp.y + 12;
        bool closeHov = (mp.x >= closeX && mp.x <= closeX + 24 && mp.y >= closeY && mp.y <= closeY + 24);
        if (closeHov)
            dl->AddRectFilled(ImVec2(closeX - 2, closeY - 2), ImVec2(closeX + 26, closeY + 26),
                              IM_COL32(255, 255, 255, 15), 4.0f);
        if (g_iconFont)
            DrawIcon(dl, icon::CLOSE, ImVec2(closeX + 4, closeY + 4),
                     IM_COL32(160, 175, 190, closeHov ? 255 : 150));
        if (closeHov && ImGui::IsMouseClicked(0))
            open_ = false;

        // ════════════════════════════════════════════════
        //  LEFT SIDE (60%) — Selling points
        // ════════════════════════════════════════════════
        float leftW = pw * 0.58f;
        float leftX = wp.x;

        // Gradient background for left side
        dl->AddRectFilledMultiColor(
            ImVec2(leftX, wp.y), ImVec2(leftX + leftW, wp.y + ph),
            IM_COL32(20, 35, 65, (int)(255 * anim_)),
            IM_COL32(15, 25, 50, (int)(255 * anim_)),
            IM_COL32(10, 20, 40, (int)(255 * anim_)),
            IM_COL32(25, 40, 70, (int)(255 * anim_)));

        float lx = leftX + 32;
        float ly = wp.y + 32;

        // Star icon + title
        if (g_iconFont) {
            DrawIcon(dl, icon::STARFILL, ImVec2(lx, ly + 2),
                     IM_COL32(255, 200, 60, (int)(255 * anim_)));
        }

        const char* title = u8"\u661F\u94F8\u4F1A\u5458"; // 星铸会员
        dl->AddText(largeFont, largeFont->FontSize,
                    ImVec2(lx + 24, ly),
                    IM_COL32(255, 220, 100, (int)(255 * anim_)), title);
        ly += largeFont->FontSize + 8;

        const char* subtitle = u8"\u7545\u73A9\u5168\u90E8\u6E38\u620F\u5E93"; // 畅玩全部游戏库
        dl->AddText(font, font->FontSize,
                    ImVec2(lx, ly),
                    IM_COL32(180, 200, 230, (int)(200 * anim_)), subtitle);
        ly += font->FontSize + 24;

        // Separator
        dl->AddLine(ImVec2(lx, ly), ImVec2(lx + leftW - 64, ly),
                    IM_COL32(60, 90, 140, (int)(40 * anim_)));
        ly += 16;

        // Features
        struct Feature { const char* icon; const char* text; };
        Feature features[] = {
            { icon::GAMEPAD,  u8"\u5168\u90E8\u6E38\u620F\u514D\u8D39\u7545\u73A9" },        // 全部游戏免费畅玩
            { icon::DOWNLOAD, u8"\u65B0\u6E38\u9996\u65E5\u5373\u53EF\u4F53\u9A8C" },        // 新游首日即可体验
            { icon::TAG,      u8"\u4F1A\u5458\u4E13\u5C5E\u6298\u6263\u4E0E\u798F\u5229" },  // 会员专属折扣与福利
        };

        for (int i = 0; i < 3; i++) {
            if (g_iconFont)
                DrawIcon(dl, features[i].icon, ImVec2(lx + 4, ly + 2),
                         IM_COL32(102, 192, 244, (int)(200 * anim_)));

            dl->AddText(font, font->FontSize,
                        ImVec2(lx + 30, ly),
                        IM_COL32(200, 215, 235, (int)(220 * anim_)),
                        features[i].text);
            ly += font->FontSize + 16;
        }

        // If already VIP, show status
        if (isVip) {
            ly += 8;
            dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + leftW - 64, ly + 36),
                              IM_COL32(40, 70, 30, (int)(200 * anim_)), 8.0f);
            dl->AddRect(ImVec2(lx, ly), ImVec2(lx + leftW - 64, ly + 36),
                        IM_COL32(87, 203, 100, (int)(80 * anim_)), 8.0f, 0, 1.0f);

            const char* activeIcon = icon::CHECK;
            if (g_iconFont)
                DrawIcon(dl, activeIcon, ImVec2(lx + 10, ly + 10),
                         IM_COL32(87, 203, 100, (int)(255 * anim_)));

            const char* activeText = u8"\u4F1A\u5458\u5DF2\u5F00\u901A"; // 会员已开通
            dl->AddText(font, font->FontSize,
                        ImVec2(lx + 34, ly + 8),
                        IM_COL32(87, 203, 100, (int)(255 * anim_)), activeText);

            // Expiry
            if (!user.vipExpire.empty()) {
                ly += 42;
                std::string expStr = u8"\u5230\u671F\u65F6\u95F4\uFF1A" + user.vipExpire; // 到期时间：
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(lx + 4, ly),
                            IM_COL32(140, 160, 190, (int)(160 * anim_)),
                            expStr.c_str());
            }
        }

        // ════════════════════════════════════════════════
        //  RIGHT SIDE (40%) — Plan selection
        // ════════════════════════════════════════════════
        float rightX = leftX + leftW;
        float rightW = pw - leftW;
        float rx = rightX + 20;
        float ry = wp.y + 28;
        float cardW = rightW - 40;

        // Plan label
        const char* planLabel = u8"\u9009\u62E9\u5957\u9910"; // 选择套餐
        dl->AddText(font, font->FontSize,
                    ImVec2(rx, ry),
                    IM_COL32(180, 195, 215, (int)(200 * anim_)), planLabel);
        ry += font->FontSize + 14;

        // Plan cards
        struct Plan {
            const char* name;
            const char* price;
            const char* note;
            int level;
        };
        Plan plans[] = {
            { u8"\u6708\u5EA6\u4F1A\u5458", u8"\uFFE525/\u6708", u8"", 1 },              // 月度会员, ¥25/月
            { u8"\u5E74\u5EA6\u4F1A\u5458", u8"\uFFE5198/\u5E74", u8"\u7701\u00A5102", 2 }, // 年度会员, ¥198/年, 省¥102
        };

        for (int i = 0; i < 2; i++) {
            float cardH = 100;
            float cy = ry;
            bool sel = (selectedPlan_ == i);

            // Card hover
            bool cardHov = (mp.x >= rx && mp.x <= rx + cardW && mp.y >= cy && mp.y <= cy + cardH);

            // Background
            ImU32 cardBg = sel ? IM_COL32(25, 45, 80, (int)(255 * anim_))
                               : IM_COL32(20, 28, 45, (int)(220 * anim_));
            ImU32 cardBorder = sel ? IM_COL32(102, 192, 244, (int)(200 * anim_))
                                   : IM_COL32(45, 65, 95, (int)(cardHov ? 120 : 60) * anim_);

            dl->AddRectFilled(ImVec2(rx, cy), ImVec2(rx + cardW, cy + cardH),
                              cardBg, 10.0f);
            dl->AddRect(ImVec2(rx, cy), ImVec2(rx + cardW, cy + cardH),
                        cardBorder, 10.0f, 0, sel ? 2.0f : 1.0f);

            // Selection indicator
            if (sel) {
                float dotX = rx + cardW - 24, dotY = cy + 14;
                dl->AddCircleFilled(ImVec2(dotX, dotY), 8, IM_COL32(102, 192, 244, (int)(255 * anim_)));
                if (g_iconFont) {
                    ImVec2 cs = g_iconFont->CalcTextSizeA(g_iconFont->FontSize, FLT_MAX, 0, icon::CHECK);
                    dl->AddText(g_iconFont, g_iconFont->FontSize,
                                ImVec2(dotX - cs.x * 0.5f, dotY - cs.y * 0.5f),
                                IM_COL32(255, 255, 255, (int)(255 * anim_)), icon::CHECK);
                }
            }

            // Plan name
            dl->AddText(font, font->FontSize,
                        ImVec2(rx + 16, cy + 14),
                        IM_COL32(220, 235, 250, (int)(255 * anim_)),
                        plans[i].name);

            // Price
            dl->AddText(largeFont, largeFont->FontSize,
                        ImVec2(rx + 16, cy + 42),
                        IM_COL32(255, 220, 100, (int)(255 * anim_)),
                        plans[i].price);

            // Save badge for yearly
            if (i == 1 && plans[i].note[0] != '\0') {
                ImVec2 ns = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, plans[i].note);
                float bx = rx + 16 + largeFont->CalcTextSizeA(largeFont->FontSize, FLT_MAX, 0, plans[i].price).x + 10;
                float by = cy + 48;
                dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + ns.x + 12, by + ns.y + 6),
                                  IM_COL32(200, 60, 30, (int)(220 * anim_)), 4.0f);
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(bx + 6, by + 3),
                            IM_COL32(255, 255, 255, (int)(255 * anim_)),
                            plans[i].note);
            }

            // Click to select
            if (cardHov && ImGui::IsMouseClicked(0))
                selectedPlan_ = i;

            ry += cardH + 12;
        }

        // ── Subscribe button ──
        float btnW = cardW, btnH = 44;
        float btnX = rx, btnY = ry + 4;
        bool btnHov = (mp.x >= btnX && mp.x <= btnX + btnW && mp.y >= btnY && mp.y <= btnY + btnH);

        // Gradient button
        ImU32 btnLeft = btnHov ? IM_COL32(50, 160, 255, (int)(255 * anim_))
                               : IM_COL32(30, 120, 220, (int)(255 * anim_));
        ImU32 btnRight = btnHov ? IM_COL32(100, 60, 255, (int)(255 * anim_))
                                : IM_COL32(80, 40, 220, (int)(255 * anim_));
        dl->AddRectFilledMultiColor(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH),
                                    btnLeft, btnRight, btnRight, btnLeft);
        // Round corners overlay
        dl->AddRect(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH),
                    IM_COL32(255, 255, 255, (int)(20 * anim_)), 8.0f, 0, 1.0f);

        const char* btnText = isVip ? u8"\u7EED\u8D39" : u8"\u7ACB\u5373\u5F00\u901A"; // 续费 / 立即开通
        ImVec2 bs = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, btnText);
        dl->AddText(font, font->FontSize,
                    ImVec2(btnX + (btnW - bs.x) * 0.5f, btnY + (btnH - bs.y) * 0.5f),
                    IM_COL32(255, 255, 255, (int)(255 * anim_)), btnText);

        if (btnHov && ImGui::IsMouseClicked(0)) {
            int level = (selectedPlan_ == 1) ? 2 : 1;
            if (AuthActivateVip(level)) {
                statusMsg_ = u8"\u5F00\u901A\u6210\u529F\uFF01"; // 开通成功！
            } else {
                statusMsg_ = u8"\u5F00\u901A\u5931\u8D25"; // 开通失败
            }
            msgTimer_ = 3.0f;
        }

        // Status message
        if (msgTimer_ > 0) {
            msgTimer_ -= dt;
            float a = msgTimer_ < 0.5f ? msgTimer_ * 2.0f : 1.0f;
            ImVec2 ms = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, statusMsg_.c_str());
            dl->AddText(smallFont, smallFont->FontSize,
                        ImVec2(btnX + (btnW - ms.x) * 0.5f, btnY + btnH + 8),
                        IM_COL32(87, 203, 100, (int)(220 * a * anim_)),
                        statusMsg_.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
