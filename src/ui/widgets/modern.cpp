#include "ui/widgets/modern.h"
#include "app/config.h"
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <cmath>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

namespace sf { namespace ui {

// ====================== Animation State Storage ======================

static std::unordered_map<ImGuiID, float> s_animStates;

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float& GetAnimState(ImGuiID id) {
    return s_animStates[id];
}

// ====================== Internal Helpers ======================

static ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    int ra = (a >> 0) & 0xFF, ga = (a >> 8) & 0xFF, ba2 = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
    int rb = (b >> 0) & 0xFF, gb = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ab = (b >> 24) & 0xFF;
    return IM_COL32(
        (int)(ra + (rb - ra) * t),
        (int)(ga + (gb - ga) * t),
        (int)(ba2 + (bb - ba2) * t),
        (int)(aa + (ab - aa) * t)
    );
}

// ====================== Buttons ======================

static bool StyledButton(const char* label, ImVec2 size, ImU32 normalBg, ImU32 hoverBg,
                          ImU32 activeBg, ImU32 textCol, float rounding, icons::DrawFunc icon) {
    ImGuiID id = ImGui::GetID(label);
    float& anim = GetAnimState(id);

    if (size.x == 0) size.x = ImGui::CalcTextSize(label).x + 32 + (icon ? 24 : 0);
    if (size.y == 0) size.y = 36;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    float dt = ImGui::GetIO().DeltaTime;
    float target = active ? 1.0f : (hovered ? 0.6f : 0.0f);
    anim += (target - anim) * dt * 14.0f;
    anim = std::clamp(anim, 0.0f, 1.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background with animation
    ImU32 bg = LerpColor(normalBg, hoverBg, anim);
    if (active) bg = activeBg;

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, rounding);

    // Subtle border glow on hover
    if (anim > 0.01f) {
        ImU32 glowCol = (hoverBg & 0x00FFFFFF) | ((int)(40 * anim) << 24);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), glowCol, rounding, 0, 1.5f);
    }

    // Icon
    float textX = pos.x + 14;
    if (icon) {
        float iconSize = 16.0f;
        float iconY = pos.y + (size.y - iconSize) * 0.5f;
        icon(dl, ImVec2(textX, iconY), iconSize, textCol);
        textX += iconSize + 8;
    }

    // Text (centered vertically)
    ImVec2 textSize = ImGui::CalcTextSize(label);
    float textY = pos.y + (size.y - textSize.y) * 0.5f;
    // Skip "##" hidden part
    const char* displayLabel = label;
    const char* hashPos = strstr(label, "##");
    char tmpBuf[256];
    if (hashPos) {
        int len = (int)(hashPos - label);
        if (len > 255) len = 255;
        memcpy(tmpBuf, label, len);
        tmpBuf[len] = 0;
        displayLabel = tmpBuf;
    }
    dl->AddText(ImVec2(textX, textY), textCol, displayLabel);

    return clicked;
}

bool ButtonPrimary(const char* label, ImVec2 size, icons::DrawFunc icon) {
    return StyledButton(label, size,
        IM_COL32(26, 130, 210, 255),   // normal
        IM_COL32(40, 160, 240, 255),   // hover
        IM_COL32(20, 100, 180, 255),   // active
        IM_COL32(255, 255, 255, 240),
        6.0f, icon);
}

bool ButtonSuccess(const char* label, ImVec2 size, icons::DrawFunc icon) {
    return StyledButton(label, size,
        IM_COL32(76, 107, 34, 255),
        IM_COL32(92, 130, 42, 255),
        IM_COL32(60, 85, 28, 255),
        IM_COL32(255, 255, 255, 240),
        6.0f, icon);
}

bool ButtonDanger(const char* label, ImVec2 size) {
    return StyledButton(label, size,
        IM_COL32(160, 40, 40, 255),
        IM_COL32(200, 55, 55, 255),
        IM_COL32(130, 30, 30, 255),
        IM_COL32(255, 255, 255, 240),
        6.0f, nullptr);
}

bool ButtonGhost(const char* label, ImVec2 size, icons::DrawFunc icon) {
    return StyledButton(label, size,
        IM_COL32(0, 0, 0, 0),
        IM_COL32(255, 255, 255, 15),
        IM_COL32(255, 255, 255, 25),
        IM_COL32(199, 213, 224, 220),
        6.0f, icon);
}

bool ButtonIcon(const char* id, icons::DrawFunc icon, float size, ImU32 color) {
    ImGuiID imId = ImGui::GetID(id);
    float& anim = GetAnimState(imId);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    bool hovered = ImGui::IsItemHovered();

    float dt = ImGui::GetIO().DeltaTime;
    anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 14.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Circular hover bg
    if (anim > 0.01f) {
        dl->AddCircleFilled(
            ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f),
            size * 0.5f,
            IM_COL32(255, 255, 255, (int)(20 * anim))
        );
    }

    ImU32 iconCol = color ? color : IM_COL32(199, 213, 224, (int)(160 + 95 * anim));
    float iconSize = size * 0.6f;
    float iconOff = (size - iconSize) * 0.5f;
    icon(dl, ImVec2(pos.x + iconOff, pos.y + iconOff), iconSize, iconCol);

    return clicked;
}

// ====================== Badges & Tags ======================

void Badge(const char* text, ImU32 bgColor, ImU32 textColor) {
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float padX = 10.0f, padY = 4.0f;
    float height = textSize.y + padY * 2;
    float width = textSize.x + padX * 2;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bgColor, height * 0.5f);
    dl->AddText(ImVec2(pos.x + padX, pos.y + padY), textColor, text);

    ImGui::Dummy(ImVec2(width, height));
}

void StatusDot(const char* text, ImU32 dotColor) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float textH = ImGui::GetTextLineHeight();

    // Dot with glow
    float dotR = 4.0f;
    ImVec2 dotCenter(pos.x + dotR + 2, pos.y + textH * 0.5f);
    dl->AddCircleFilled(dotCenter, dotR + 2, (dotColor & 0x00FFFFFF) | 0x30000000);
    dl->AddCircleFilled(dotCenter, dotR, dotColor);

    // Text
    dl->AddText(ImVec2(pos.x + dotR * 2 + 10, pos.y), IM_COL32(199, 213, 224, 220), text);

    ImGui::Dummy(ImVec2(ImGui::CalcTextSize(text).x + dotR * 2 + 12, textH));
}

void Tag(const char* text, ImU32 color) {
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float padX = 8.0f, padY = 3.0f;
    float height = textSize.y + padY * 2;
    float width = textSize.x + padX * 2;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), color, height * 0.5f, 0, 1.2f);
    dl->AddText(ImVec2(pos.x + padX, pos.y + padY), color, text);

    ImGui::Dummy(ImVec2(width, height));
}

// ====================== Progress ======================

void ProgressBar(float fraction, ImVec2 size, ImU32 fillColor, ImU32 bgColor, const char* overlayText) {
    if (bgColor == 0) bgColor = IM_COL32(30, 36, 44, 255);
    fraction = std::clamp(fraction, 0.0f, 1.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rounding = size.y * 0.5f;

    // Background
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor, rounding);

    // Fill with gradient
    if (fraction > 0.01f) {
        float fillW = size.x * fraction;
        ImU32 fillEnd = LerpColor(fillColor, IM_COL32(255, 255, 255, 60), 0.15f);

        // Two-part gradient fill
        dl->AddRectFilled(pos, ImVec2(pos.x + fillW, pos.y + size.y * 0.5f),
                          fillColor, rounding);
        dl->AddRectFilled(ImVec2(pos.x, pos.y + size.y * 0.5f),
                          ImVec2(pos.x + fillW, pos.y + size.y),
                          LerpColor(fillColor, IM_COL32(0, 0, 0, 40), 0.2f), rounding);

        // Shine highlight
        dl->AddRectFilled(
            ImVec2(pos.x + 1, pos.y + 1),
            ImVec2(pos.x + fillW - 1, pos.y + size.y * 0.35f),
            IM_COL32(255, 255, 255, 18), rounding
        );
    }

    // Overlay text
    if (overlayText) {
        ImVec2 textSize = ImGui::CalcTextSize(overlayText);
        dl->AddText(
            ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
            IM_COL32(255, 255, 255, 200), overlayText
        );
    }

    ImGui::Dummy(size);
}

void ProgressCircle(float fraction, float radius, ImU32 color, float thickness) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center(pos.x + radius, pos.y + radius);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background circle
    dl->AddCircle(center, radius, IM_COL32(40, 46, 54, 255), 36, thickness);

    // Progress arc
    if (fraction > 0.01f) {
        float startAngle = -IM_PI * 0.5f;
        float endAngle = startAngle + IM_PI * 2.0f * fraction;
        dl->PathArcTo(center, radius, startAngle, endAngle, 36);
        dl->PathStroke(color, 0, thickness);
    }

    // Percentage text in center
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f%%", fraction * 100);
    ImVec2 textSize = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                IM_COL32(199, 213, 224, 220), buf);

    ImGui::Dummy(ImVec2(radius * 2, radius * 2));
}

// ====================== Cards & Containers ======================

static bool s_cardHovered = false;

bool BeginCard(const char* id, ImVec2 size, bool hoverable, float rounding) {
    ImGuiID imId = ImGui::GetID(id);
    float& anim = GetAnimState(imId);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    s_cardHovered = hovered;

    float dt = ImGui::GetIO().DeltaTime;
    if (hoverable) {
        anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 12.0f;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Card background with elevation on hover
    ImU32 bg = IM_COL32(27, 40, 56, (int)(180 + 75 * anim));
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, rounding);

    // Subtle shadow (simulated)
    if (anim > 0.01f) {
        ImU32 shadowCol = IM_COL32(0, 0, 0, (int)(30 * anim));
        dl->AddRectFilled(
            ImVec2(pos.x + 2, pos.y + 2),
            ImVec2(pos.x + size.x + 2, pos.y + size.y + 2),
            shadowCol, rounding
        );
        // Redraw card on top of shadow
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, rounding);
    }

    // Border highlight on hover
    if (anim > 0.01f) {
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(26, 159, 255, (int)(80 * anim)), rounding, 0, 1.5f);
    }

    return clicked;
}

void EndCard() {
    // Placeholder for future cleanup
}

void GradientRect(ImDrawList* dl, ImVec2 topLeft, ImVec2 bottomRight,
                  ImU32 topColor, ImU32 bottomColor, float rounding) {
    if (rounding > 0) {
        // Can't do rounded gradient natively, draw two rects
        float midY = (topLeft.y + bottomRight.y) * 0.5f;
        dl->AddRectFilled(topLeft, ImVec2(bottomRight.x, midY), topColor, rounding);
        dl->AddRectFilled(ImVec2(topLeft.x, midY), bottomRight, bottomColor, rounding);
        // Fill middle seam
        dl->AddRectFilled(ImVec2(topLeft.x, midY - 2), ImVec2(bottomRight.x, midY + 2),
                          LerpColor(topColor, bottomColor, 0.5f));
    } else {
        dl->AddRectFilledMultiColor(topLeft, bottomRight, topColor, topColor, bottomColor, bottomColor);
    }
}

// ====================== Separators & Spacing ======================

void SectionHeader(const char* text, ImU32 accentColor) {
    if (accentColor == 0) accentColor = IM_COL32(26, 159, 255, 255);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(text);

    // Accent bar
    float barH = textSize.y + 4;
    dl->AddRectFilled(pos, ImVec2(pos.x + 3, pos.y + barH), accentColor, 1.5f);

    // Text
    dl->AddText(ImVec2(pos.x + 12, pos.y + 2),
                IM_COL32(255, 255, 255, 240), text);

    // Fading line
    float lineY = pos.y + barH + 6;
    float lineW = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilledMultiColor(
        ImVec2(pos.x, lineY),
        ImVec2(pos.x + lineW, lineY + 1),
        IM_COL32(42, 71, 94, 180), IM_COL32(42, 71, 94, 0),
        IM_COL32(42, 71, 94, 0), IM_COL32(42, 71, 94, 180)
    );

    ImGui::Dummy(ImVec2(lineW, barH + 10));
}

void Divider(float width) {
    if (width <= 0) width = ImGui::GetContentRegionAvail().x;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilledMultiColor(
        pos, ImVec2(pos.x + width, pos.y + 1),
        IM_COL32(42, 71, 94, 0), IM_COL32(42, 71, 94, 140),
        IM_COL32(42, 71, 94, 140), IM_COL32(42, 71, 94, 0)
    );

    ImGui::Dummy(ImVec2(width, 8));
}

// ====================== Tooltip ======================

void ModernTooltip(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(20, 24, 30, 240));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(42, 71, 94, 180));
        ImGui::SetTooltip("%s", text);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}

}} // namespace sf::ui
