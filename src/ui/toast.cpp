#include "ui/toast.h"
#include "ui/iconfonts.h"
#include "core/texture_manager.h"
#include "core/navigation.h"
#include <algorithm>
#include <cmath>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

static std::mutex g_toastMtx;
static std::vector<ToastMessage> g_toasts;

static std::mutex g_gameCardMtx;
static std::vector<GameCardNotification> g_gameCardNotifications;

void ShowToast(const std::string& text, ToastType type, float duration) {
    std::lock_guard<std::mutex> lock(g_toastMtx);
    g_toasts.push_back({ text, type, duration, 0.0f });
}

void RenderToasts(float winW, float winH) {
    std::lock_guard<std::mutex> lock(g_toastMtx);
    if (g_toasts.empty()) return;

    float dt = ImGui::GetIO().DeltaTime;
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;

    float toastW = 380.0f;
    float padX = 16.0f;
    float padY = 12.0f;
    float spacing = 8.0f;
    float startX = winW - toastW - 20.0f;
    float startY = 60.0f; // below header

    int idx = 0;
    for (auto it = g_toasts.begin(); it != g_toasts.end(); ) {
        it->elapsed += dt;

        // Fade in/out animation
        float fadeIn = std::min(it->elapsed / 0.3f, 1.0f);
        float fadeOut = std::max((it->lifetime - it->elapsed) / 0.5f, 0.0f);
        float alpha = std::min(fadeIn, fadeOut);
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        if (it->elapsed >= it->lifetime) {
            it = g_toasts.erase(it);
            continue;
        }

        // Slide in from right
        float slideOffset = (1.0f - fadeIn) * 100.0f;
        float tx = startX + slideOffset;

        // Calculate text size for auto height
        ImVec2 textSz = font->CalcTextSizeA(font->FontSize, toastW - padX * 2 - 30, toastW - padX * 2 - 30, it->text.c_str());
        float toastH = textSz.y + padY * 2;
        if (toastH < 48.0f) toastH = 48.0f;

        float ty = startY + idx * (toastH + spacing);

        int a = (int)(alpha * 255);

        // Colors by type
        ImU32 bgCol, accentCol, iconCol;
        const char* iconGlyph = icon::INFO;
        switch (it->type) {
            case ToastType::Success:
                bgCol = IM_COL32(20, 40, 30, (int)(240 * alpha));
                accentCol = IM_COL32(46, 160, 67, a);
                iconCol = IM_COL32(87, 203, 100, a);
                iconGlyph = icon::CHECK;
                break;
            case ToastType::Warning:
                bgCol = IM_COL32(45, 38, 18, (int)(240 * alpha));
                accentCol = IM_COL32(200, 160, 30, a);
                iconCol = IM_COL32(255, 200, 60, a);
                iconGlyph = icon::INFO;
                break;
            case ToastType::Error:
                bgCol = IM_COL32(45, 20, 20, (int)(240 * alpha));
                accentCol = IM_COL32(200, 60, 60, a);
                iconCol = IM_COL32(240, 80, 80, a);
                iconGlyph = icon::CLOSE;
                break;
            default: // Info
                bgCol = IM_COL32(22, 30, 45, (int)(240 * alpha));
                accentCol = IM_COL32(26, 120, 220, a);
                iconCol = IM_COL32(102, 192, 244, a);
                iconGlyph = icon::INFO;
                break;
        }

        // Shadow
        for (int s = 1; s <= 4; s++) {
            ImU32 sc = IM_COL32(0, 0, 0, (int)(30.0f * alpha * (1.0f - s / 5.0f)));
            fg->AddRectFilled(
                ImVec2(tx + s, ty + s),
                ImVec2(tx + toastW + s, ty + toastH + s),
                sc, 8.0f);
        }

        // Background
        fg->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + toastW, ty + toastH), bgCol, 8.0f);

        // Left accent bar
        fg->AddRectFilled(ImVec2(tx, ty + 4), ImVec2(tx + 3, ty + toastH - 4), accentCol, 2.0f);

        // Border
        fg->AddRect(ImVec2(tx, ty), ImVec2(tx + toastW, ty + toastH),
                     IM_COL32(60, 75, 100, (int)(60 * alpha)), 8.0f);

        // Icon
        if (g_iconFont) {
            fg->AddText(g_iconFont, 18.0f, ImVec2(tx + padX, ty + (toastH - 18) * 0.5f),
                        iconCol, iconGlyph);
        }

        // Text
        float textX = tx + padX + (g_iconFont ? 26.0f : 0.0f);
        float textY = ty + (toastH - textSz.y) * 0.5f;
        fg->AddText(font, font->FontSize,
                    ImVec2(textX, textY),
                    IM_COL32(220, 230, 240, a),
                    it->text.c_str(), nullptr, toastW - padX * 2 - 30);

        // Progress bar at bottom
        float progW = toastW * (1.0f - it->elapsed / it->lifetime);
        fg->AddRectFilled(
            ImVec2(tx + 4, ty + toastH - 3),
            ImVec2(tx + 4 + progW, ty + toastH - 1),
            IM_COL32(255, 255, 255, (int)(30 * alpha)), 1.0f);

        idx++;
        ++it;
    }
}

void ShowGameCardNotification(const std::string& appId, const std::string& gameName,
                               const std::string& message, float duration) {
    std::lock_guard<std::mutex> lock(g_gameCardMtx);
    g_gameCardNotifications.push_back({ appId, gameName, message, duration, 0.0f });
}

void RenderGameCardNotifications(float winW, float winH) {
    std::lock_guard<std::mutex> lock(g_gameCardMtx);
    if (g_gameCardNotifications.empty()) return;

    float dt = ImGui::GetIO().DeltaTime;
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;

    // 卡片尺寸
    float cardW = 320.0f;
    float coverH = 100.0f;  // 封面高度
    float infoH = 60.0f;    // 信息区高度
    float cardH = coverH + infoH;
    float spacing = 12.0f;
    float margin = 20.0f;

    float startX = winW - cardW - margin;
    float startY = 60.0f;  // header 下方

    int idx = 0;
    for (auto it = g_gameCardNotifications.begin(); it != g_gameCardNotifications.end(); ) {
        it->elapsed += dt;

        // 动画：前0.3秒淡入，最后1秒淡出
        float fadeInDuration = 0.3f;
        float fadeOutDuration = 1.0f;
        float fadeOutStart = it->lifetime - fadeOutDuration;

        float alpha = 1.0f;
        if (it->elapsed < fadeInDuration) {
            alpha = it->elapsed / fadeInDuration;
        } else if (it->elapsed > fadeOutStart) {
            alpha = (it->lifetime - it->elapsed) / fadeOutDuration;
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        if (it->elapsed >= it->lifetime) {
            it = g_gameCardNotifications.erase(it);
            continue;
        }

        // 从右侧滑入
        float slideOffset = (1.0f - std::min(it->elapsed / fadeInDuration, 1.0f)) * 80.0f;
        float cx = startX + slideOffset;
        float cy = startY + idx * (cardH + spacing);

        int a = (int)(alpha * 255);

        // 检测点击 - 使用 ImGui 的鼠标检测
        ImVec2 mousePos = ImGui::GetMousePos();
        bool isHovered = (mousePos.x >= cx && mousePos.x <= cx + cardW &&
                          mousePos.y >= cy && mousePos.y <= cy + cardH);
        bool isClicked = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        // 点击后跳转到库中的游戏
        if (isClicked) {
            Navigate(NavAction::ViewLibraryGame, it->appId, it->gameName);
            // 立即移除通知
            it = g_gameCardNotifications.erase(it);
            continue;
        }

        // 悬停时高亮效果
        float hoverBrightness = isHovered ? 1.15f : 1.0f;

        // 阴影
        for (int s = 1; s <= 6; s++) {
            ImU32 sc = IM_COL32(0, 0, 0, (int)(40.0f * alpha * (1.0f - s / 7.0f)));
            fg->AddRectFilled(
                ImVec2(cx + s * 1.5f, cy + s * 1.5f),
                ImVec2(cx + cardW + s * 1.5f, cy + cardH + s * 1.5f),
                sc, 10.0f);
        }

        // 卡片背景
        fg->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                          IM_COL32(27, 40, 56, (int)(250 * alpha)), 10.0f);

        // 游戏封面图
        ImTextureID coverTex = GetGameTexture(it->appId);
        if (coverTex) {
            fg->AddImageRounded(coverTex,
                                ImVec2(cx, cy),
                                ImVec2(cx + cardW, cy + coverH),
                                ImVec2(0, 0), ImVec2(1, 1),
                                IM_COL32(255, 255, 255, a),
                                10.0f, ImDrawFlags_RoundCornersTop);
        } else {
            // 无封面时显示占位
            fg->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + coverH),
                              IM_COL32(42, 55, 71, (int)(200 * alpha)),
                              10.0f, ImDrawFlags_RoundCornersTop);
        }

        // 封面底部渐变遮罩（让文字更清晰）
        fg->AddRectFilledMultiColor(
            ImVec2(cx, cy + coverH - 30),
            ImVec2(cx + cardW, cy + coverH),
            IM_COL32(27, 40, 56, 0),
            IM_COL32(27, 40, 56, 0),
            IM_COL32(27, 40, 56, (int)(250 * alpha)),
            IM_COL32(27, 40, 56, (int)(250 * alpha)));

        // 信息区背景
        float infoY = cy + coverH;
        fg->AddRectFilled(ImVec2(cx, infoY), ImVec2(cx + cardW, cy + cardH),
                          IM_COL32(27, 40, 56, (int)(250 * alpha)),
                          10.0f, ImDrawFlags_RoundCornersBottom);

        // 成功图标和状态文字
        float iconSize = 20.0f;
        float padX = 16.0f;
        float textY = infoY + 10.0f;

        // 成功图标（绿色勾）
        if (g_iconFont) {
            fg->AddText(g_iconFont, iconSize,
                        ImVec2(cx + padX, textY + 2),
                        IM_COL32(87, 203, 100, a),
                        icon::CHECK);
        }

        // 状态消息（如"安装完成"）
        float msgX = cx + padX + (g_iconFont ? iconSize + 8 : 0);
        fg->AddText(font, font->FontSize,
                    ImVec2(msgX, textY + 2),
                    IM_COL32(87, 203, 100, a),
                    it->message.c_str());

        // 游戏名
        float nameY = textY + font->FontSize + 6;
        ImVec2 nameSz = font->CalcTextSizeA(font->FontSize, cardW - padX * 2, 0, it->gameName.c_str());
        // 截断过长的名字
        std::string displayName = it->gameName;
        if (nameSz.x > cardW - padX * 2) {
            while (nameSz.x > cardW - padX * 2 - 20 && displayName.length() > 5) {
                displayName = displayName.substr(0, displayName.length() - 4) + "...";
                nameSz = font->CalcTextSizeA(font->FontSize, cardW - padX * 2, 0, displayName.c_str());
            }
        }
        fg->AddText(font, font->FontSize,
                    ImVec2(cx + padX, nameY),
                    IM_COL32(199, 213, 224, a),
                    displayName.c_str());

        // 边框
        fg->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                    IM_COL32(87, 203, 100, (int)(80 * alpha)), 10.0f, 0, 1.5f);

        idx++;
        ++it;
    }
}

} // namespace sf
