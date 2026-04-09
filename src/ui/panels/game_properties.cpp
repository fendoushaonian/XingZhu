#include "ui/panels/game_properties.h"
#include "ui/iconfonts.h"
#include "core/texture_manager.h"
#include "core/steam_store.h"
#include "core/navigation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;
extern ImFont* g_iconFontLarge;

void GamePropertiesPanel::Open(const GameInfo& game) {
    open_ = true;
    anim_ = 0.0f;
    game_ = game;
    storeRequested_ = false;
    headerTexRequested_ = false;
}

void GamePropertiesPanel::Render(float winW, float winH) {
    if (!open_ && anim_ < 0.01f) return;

    float dt = ImGui::GetIO().DeltaTime;
    float target = open_ ? 1.0f : 0.0f;
    anim_ += (target - anim_) * dt * 14.0f;
    anim_ = std::clamp(anim_, 0.0f, 1.0f);
    if (!open_ && anim_ < 0.01f) { anim_ = 0.0f; return; }

    // Request store data if not yet done
    if (!storeRequested_) {
        RequestStoreData(game_.appId);
        storeRequested_ = true;
    }

    // Request header texture
    if (!headerTexRequested_) {
        const SteamStoreData* sd = GetStoreData(game_.appId);
        if (sd && sd->loaded && !sd->headerImage.empty()) {
            RequestTextureFromUrl("prop_hdr_" + game_.appId, sd->headerImage);
            headerTexRequested_ = true;
        }
    }

    int overlayAlpha = (int)(120 * anim_);
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;

    // Dark overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, overlayAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    if (ImGui::Begin("##PropOverlay", nullptr, overlayFlags)) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && ImGui::IsMouseClicked(0))
            open_ = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Panel
    float pw = 520, ph = 620;
    float px = (winW - pw) * 0.5f;
    float py = (winH - ph) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(22, 28, 40, (int)(252 * anim_)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 70, 100, (int)(80 * anim_)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##GamePropPanel", nullptr, panelFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 mp = ImGui::GetMousePos();
        float cx = wp.x;
        float cy = wp.y;

        // ── Header image area (top, full width) ──
        float hdrH = 180.0f;
        ImTextureID hdrTex = GetTextureByKey("prop_hdr_" + game_.appId);
        if (!hdrTex) hdrTex = GetGameTexture(game_.appId);

        if (hdrTex) {
            dl->AddImageRounded(hdrTex,
                ImVec2(cx, cy), ImVec2(cx + pw, cy + hdrH),
                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, (int)(255 * anim_)), 10.0f,
                ImDrawFlags_RoundCornersTop);
        } else {
            // Gradient placeholder
            dl->AddRectFilledMultiColor(
                ImVec2(cx, cy), ImVec2(cx + pw, cy + hdrH),
                IM_COL32(30, 45, 70, 255), IM_COL32(20, 30, 55, 255),
                IM_COL32(15, 22, 40, 255), IM_COL32(25, 38, 60, 255));
        }

        // Gradient fade at bottom of header
        dl->AddRectFilledMultiColor(
            ImVec2(cx, cy + hdrH - 60), ImVec2(cx + pw, cy + hdrH),
            IM_COL32(22, 28, 40, 0), IM_COL32(22, 28, 40, 0),
            IM_COL32(22, 28, 40, 255), IM_COL32(22, 28, 40, 255));

        // Close button (top right)
        float closeX = cx + pw - 36;
        float closeY = cy + 8;
        bool closeHov = (mp.x >= closeX && mp.x <= closeX + 28 && mp.y >= closeY && mp.y <= closeY + 28);
        dl->AddRectFilled(ImVec2(closeX, closeY), ImVec2(closeX + 28, closeY + 28),
                          IM_COL32(0, 0, 0, closeHov ? 150 : 100), 6.0f);
        if (g_iconFont)
            DrawIcon(dl, icon::CLOSE, ImVec2(closeX + 6, closeY + 6),
                     IM_COL32(255, 255, 255, closeHov ? 255 : 180));
        if (closeHov && ImGui::IsMouseClicked(0))
            open_ = false;

        // Game title overlay on header
        {
            float titleY = cy + hdrH - 40;
            dl->AddText(largeFont, largeFont->FontSize,
                        ImVec2(cx + 20, titleY),
                        IM_COL32(255, 255, 255, (int)(255 * anim_)), game_.name.c_str());
        }

        // ── Scrollable content below header ──
        ImGui::SetCursorPos(ImVec2(0, hdrH));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::BeginChild("##propContent", ImVec2(pw, ph - hdrH), false);

        float padX = 20.0f;
        float contentW = pw - padX * 2;

        ImGui::Dummy(ImVec2(0, 12));

        // ── Quick info row (playtime, status, achievements) ──
        {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            float cardH = 60.0f;
            float cardW = (contentW - 12) / 3.0f;

            struct QuickStat {
                const char* icon;
                const char* label;
                char value[64];
                ImU32 iconCol;
            } stats[3];

            stats[0] = { icon::CLOCK, u8"\u6E38\u620F\u65F6\u95F4", {}, IM_COL32(102, 192, 244, 255) }; // 游戏时间
            snprintf(stats[0].value, sizeof(stats[0].value), "%.1f %s", game_.playtime, u8"\u5C0F\u65F6");

            stats[1] = { icon::CHECK, u8"\u72B6\u6001", {}, IM_COL32(87, 203, 100, 255) }; // 状态
            if (game_.status == "installed") snprintf(stats[1].value, sizeof(stats[1].value), "%s", u8"\u5DF2\u5B89\u88C5");
            else if (game_.status == "playing") snprintf(stats[1].value, sizeof(stats[1].value), "%s", u8"\u6E38\u620F\u4E2D");
            else snprintf(stats[1].value, sizeof(stats[1].value), "%s", u8"\u672A\u5B89\u88C5");

            stats[2] = { icon::TROPHY, u8"\u6210\u5C31", {}, IM_COL32(255, 200, 60, 255) }; // 成就
            if (game_.achievementsTotal > 0)
                snprintf(stats[2].value, sizeof(stats[2].value), "%d / %d", game_.achievements, game_.achievementsTotal);
            else
                snprintf(stats[2].value, sizeof(stats[2].value), "-");

            for (int i = 0; i < 3; i++) {
                float sx = rp.x + padX + i * (cardW + 6);
                float sy = rp.y;

                dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + cardW, sy + cardH),
                                  IM_COL32(16, 22, 36, 255), 8.0f);
                dl->AddRect(ImVec2(sx, sy), ImVec2(sx + cardW, sy + cardH),
                            IM_COL32(50, 70, 100, 60), 8.0f);

                if (g_iconFont) {
                    ImVec2 isz = g_iconFont->CalcTextSizeA(g_iconFont->FontSize, FLT_MAX, 0, stats[i].icon);
                    dl->AddText(g_iconFont, g_iconFont->FontSize,
                                ImVec2(sx + 12, sy + 10), stats[i].iconCol, stats[i].icon);
                }

                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(sx + 32, sy + 10),
                            IM_COL32(140, 155, 175, 200), stats[i].label);
                dl->AddText(font, font->FontSize,
                            ImVec2(sx + 12, sy + 32),
                            IM_COL32(220, 235, 250, 255), stats[i].value);
            }

            ImGui::Dummy(ImVec2(0, cardH + 12));
        }

        // ── Action buttons (Launch / Store) ──
        {
            ImVec2 bp = ImGui::GetCursorScreenPos();
            float btnH = 36.0f;
            bool isInstalled = (game_.status != "not_installed");

            // Launch / Install button
            float btn1W = 140;
            float btn1X = bp.x + padX;
            float btn1Y = bp.y;
            bool btn1Hov = (mp.x >= btn1X && mp.x < btn1X + btn1W && mp.y >= btn1Y && mp.y < btn1Y + btnH);

            if (isInstalled) {
                dl->AddRectFilled(ImVec2(btn1X, btn1Y), ImVec2(btn1X + btn1W, btn1Y + btnH),
                                  btn1Hov ? IM_COL32(100, 190, 50, 255) : IM_COL32(80, 160, 40, 255), 6.0f);
                const char* playLabel = u8"\u542F\u52A8\u6E38\u620F"; // 启动游戏
                if (g_iconFont) {
                    dl->AddText(g_iconFont, g_iconFont->FontSize,
                                ImVec2(btn1X + 14, btn1Y + 9), IM_COL32(255, 255, 255, 255), icon::PLAY);
                }
                dl->AddText(font, font->FontSize,
                            ImVec2(btn1X + 36, btn1Y + 9), IM_COL32(255, 255, 255, 255), playLabel);
                if (btn1Hov && ImGui::IsMouseClicked(0))
                    Navigate(NavAction::LaunchGame, game_.appId);
            } else {
                dl->AddRectFilled(ImVec2(btn1X, btn1Y), ImVec2(btn1X + btn1W, btn1Y + btnH),
                                  btn1Hov ? IM_COL32(40, 150, 255, 255) : IM_COL32(26, 120, 220, 255), 6.0f);
                const char* installLabel = u8"\u5B89\u88C5\u6E38\u620F"; // 安装游戏
                if (g_iconFont) {
                    dl->AddText(g_iconFont, g_iconFont->FontSize,
                                ImVec2(btn1X + 14, btn1Y + 9), IM_COL32(255, 255, 255, 255), icon::DOWNLOAD);
                }
                dl->AddText(font, font->FontSize,
                            ImVec2(btn1X + 36, btn1Y + 9), IM_COL32(255, 255, 255, 255), installLabel);
                if (btn1Hov && ImGui::IsMouseClicked(0))
                    Navigate(NavAction::InstallGame, game_.appId, game_.name, game_.size);
            }

            // Store page button
            float btn2W = 140;
            float btn2X = btn1X + btn1W + 10;
            bool btn2Hov = (mp.x >= btn2X && mp.x < btn2X + btn2W && mp.y >= btn1Y && mp.y < btn1Y + btnH);
            dl->AddRectFilled(ImVec2(btn2X, btn1Y), ImVec2(btn2X + btn2W, btn1Y + btnH),
                              btn2Hov ? IM_COL32(50, 65, 90, 255) : IM_COL32(35, 48, 70, 255), 6.0f);
            dl->AddRect(ImVec2(btn2X, btn1Y), ImVec2(btn2X + btn2W, btn1Y + btnH),
                        IM_COL32(70, 90, 120, 100), 6.0f);
            if (g_iconFont) {
                dl->AddText(g_iconFont, g_iconFont->FontSize,
                            ImVec2(btn2X + 14, btn1Y + 9), IM_COL32(102, 192, 244, 255), icon::INFO);
            }
            dl->AddText(font, font->FontSize,
                        ImVec2(btn2X + 36, btn1Y + 9), IM_COL32(200, 215, 235, 255),
                        u8"\u5546\u5E97\u9875\u9762"); // 商店页面
            if (btn2Hov && ImGui::IsMouseClicked(0)) {
                Navigate(NavAction::ViewStorePage, game_.appId);
                open_ = false;
            }

            ImGui::Dummy(ImVec2(0, btnH + 16));
        }

        // ── Steam store data section ──
        const SteamStoreData* sd = GetStoreData(game_.appId);
        bool loading = IsStoreDataLoading(game_.appId);

        // Separator
        {
            ImVec2 sp = ImGui::GetCursorScreenPos();
            dl->AddRectFilledMultiColor(
                ImVec2(sp.x + padX, sp.y), ImVec2(sp.x + padX + contentW, sp.y + 1),
                IM_COL32(50, 70, 100, 0), IM_COL32(50, 70, 100, 80),
                IM_COL32(50, 70, 100, 80), IM_COL32(50, 70, 100, 0));
            ImGui::Dummy(ImVec2(0, 12));
        }

        if (loading && (!sd || !sd->loaded)) {
            ImVec2 lp = ImGui::GetCursorScreenPos();
            dl->AddText(font, font->FontSize, ImVec2(lp.x + padX, lp.y),
                        IM_COL32(140, 155, 175, 200), u8"\u6B63\u5728\u52A0\u8F7D\u6E38\u620F\u4FE1\u606F..."); // 正在加载游戏信息...
            ImGui::Dummy(ImVec2(0, 20));
        } else if (sd && sd->loaded) {
            // ── Info rows ──
            auto InfoRow = [&](const char* label, const char* value) {
                if (!value || value[0] == '\0') return;
                ImVec2 rp2 = ImGui::GetCursorScreenPos();
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(rp2.x + padX, rp2.y),
                            IM_COL32(110, 130, 155, 200), label);
                dl->AddText(font, font->FontSize,
                            ImVec2(rp2.x + padX + 100, rp2.y),
                            IM_COL32(210, 225, 240, 255), value);
                ImGui::Dummy(ImVec2(0, 22));
            };

            InfoRow(u8"\u5F00\u53D1\u5546", sd->developer.c_str());         // 开发商
            InfoRow(u8"\u53D1\u884C\u5546", sd->publisher.c_str());         // 发行商
            InfoRow(u8"\u53D1\u884C\u65E5\u671F", sd->releaseDate.c_str()); // 发行日期

            // Genres
            if (!sd->genres.empty()) {
                std::string genreStr;
                for (size_t i = 0; i < sd->genres.size(); i++) {
                    if (i > 0) genreStr += ", ";
                    genreStr += sd->genres[i];
                }
                InfoRow(u8"\u7C7B\u578B", genreStr.c_str()); // 类型
            }

            // Review
            if (sd->reviewScore > 0) {
                char reviewBuf[128];
                snprintf(reviewBuf, sizeof(reviewBuf), "%s (%d%%)", sd->reviewDesc.c_str(), sd->reviewScore);
                ImVec2 rp2 = ImGui::GetCursorScreenPos();
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(rp2.x + padX, rp2.y),
                            IM_COL32(110, 130, 155, 200), u8"\u8BC4\u4EF7"); // 评价
                ImU32 reviewCol = sd->reviewScore >= 70 ? IM_COL32(102, 192, 244, 255) :
                                  sd->reviewScore >= 40 ? IM_COL32(200, 180, 80, 255) :
                                                          IM_COL32(200, 80, 80, 255);
                dl->AddText(font, font->FontSize,
                            ImVec2(rp2.x + padX + 100, rp2.y), reviewCol, reviewBuf);
                ImGui::Dummy(ImVec2(0, 22));
            }

            // Price
            if (!sd->priceFormatted.empty()) {
                ImVec2 rp2 = ImGui::GetCursorScreenPos();
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(rp2.x + padX, rp2.y),
                            IM_COL32(110, 130, 155, 200), u8"\u4EF7\u683C"); // 价格
                if (sd->discountPercent > 0) {
                    char discBuf[32];
                    snprintf(discBuf, sizeof(discBuf), "-%d%%", sd->discountPercent);
                    dl->AddText(font, font->FontSize,
                                ImVec2(rp2.x + padX + 100, rp2.y),
                                IM_COL32(160, 210, 70, 255), discBuf);
                    float dw = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, discBuf).x;
                    dl->AddText(font, font->FontSize,
                                ImVec2(rp2.x + padX + 105 + dw, rp2.y),
                                IM_COL32(210, 225, 240, 255), sd->priceFormatted.c_str());
                } else {
                    dl->AddText(font, font->FontSize,
                                ImVec2(rp2.x + padX + 100, rp2.y),
                                IM_COL32(210, 225, 240, 255), sd->priceFormatted.c_str());
                }
                ImGui::Dummy(ImVec2(0, 22));
            }

            // Platforms
            {
                ImVec2 rp2 = ImGui::GetCursorScreenPos();
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(rp2.x + padX, rp2.y),
                            IM_COL32(110, 130, 155, 200), u8"\u5E73\u53F0"); // 平台
                float px2 = rp2.x + padX + 100;
                if (sd->platformWindows) {
                    dl->AddText(font, font->FontSize, ImVec2(px2, rp2.y),
                                IM_COL32(210, 225, 240, 255), "Windows");
                    px2 += font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, "Windows").x + 12;
                }
                if (sd->platformMac) {
                    dl->AddText(font, font->FontSize, ImVec2(px2, rp2.y),
                                IM_COL32(210, 225, 240, 255), "macOS");
                    px2 += font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, "macOS").x + 12;
                }
                if (sd->platformLinux) {
                    dl->AddText(font, font->FontSize, ImVec2(px2, rp2.y),
                                IM_COL32(210, 225, 240, 255), "Linux");
                }
                ImGui::Dummy(ImVec2(0, 22));
            }

            // Description
            if (!sd->shortDesc.empty()) {
                ImVec2 sp2 = ImGui::GetCursorScreenPos();
                dl->AddRectFilledMultiColor(
                    ImVec2(sp2.x + padX, sp2.y), ImVec2(sp2.x + padX + contentW, sp2.y + 1),
                    IM_COL32(50, 70, 100, 0), IM_COL32(50, 70, 100, 80),
                    IM_COL32(50, 70, 100, 80), IM_COL32(50, 70, 100, 0));
                ImGui::Dummy(ImVec2(0, 10));

                ImVec2 dp = ImGui::GetCursorScreenPos();
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(dp.x + padX, dp.y),
                            IM_COL32(102, 192, 244, 220), u8"\u7B80\u4ECB"); // 简介
                ImGui::Dummy(ImVec2(0, 20));

                // Wrap description text
                ImVec2 dp2 = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos(ImVec2(dp2.x + padX, dp2.y));
                ImGui::PushTextWrapPos(dp2.x + padX + contentW);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 195, 215, 220));
                ImGui::TextWrapped("%s", sd->shortDesc.c_str());
                ImGui::PopStyleColor();
                ImGui::PopTextWrapPos();
            }

            // Categories
            if (!sd->categories.empty()) {
                ImGui::Dummy(ImVec2(0, 12));
                ImVec2 cp2 = ImGui::GetCursorScreenPos();
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(cp2.x + padX, cp2.y),
                            IM_COL32(102, 192, 244, 220), u8"\u529F\u80FD"); // 功能
                ImGui::Dummy(ImVec2(0, 20));

                // Tag pills
                ImVec2 tp = ImGui::GetCursorScreenPos();
                float tx = tp.x + padX;
                float ty2 = tp.y;
                float maxX = tp.x + padX + contentW;
                float totalH = 0;

                for (auto& cat : sd->categories) {
                    ImVec2 ts = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, cat.c_str());
                    float pillW = ts.x + 16;
                    float pillH = ts.y + 8;

                    if (tx + pillW > maxX && tx > tp.x + padX) {
                        tx = tp.x + padX;
                        ty2 += pillH + 4;
                    }

                    dl->AddRectFilled(ImVec2(tx, ty2), ImVec2(tx + pillW, ty2 + pillH),
                                      IM_COL32(35, 50, 75, 255), 4.0f);
                    dl->AddRect(ImVec2(tx, ty2), ImVec2(tx + pillW, ty2 + pillH),
                                IM_COL32(60, 85, 120, 100), 4.0f);
                    dl->AddText(smallFont, smallFont->FontSize,
                                ImVec2(tx + 8, ty2 + 4),
                                IM_COL32(170, 190, 210, 220), cat.c_str());

                    tx += pillW + 6;
                    totalH = ty2 + pillH - tp.y;
                }

                ImGui::Dummy(ImVec2(0, totalH + 8));
            }
        } else if (!loading) {
            ImVec2 lp = ImGui::GetCursorScreenPos();
            dl->AddText(font, font->FontSize, ImVec2(lp.x + padX, lp.y),
                        IM_COL32(140, 155, 175, 160), u8"\u6682\u65E0\u8BE6\u7EC6\u4FE1\u606F"); // 暂无详细信息
            ImGui::Dummy(ImVec2(0, 20));
        }

        ImGui::Dummy(ImVec2(0, 20));
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
