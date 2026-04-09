#pragma once
#include "imgui.h"
#include "ui/icons.h"
#include <string>

namespace sf { namespace ui {

// ====================== Modern Button Styles ======================

// Primary button (Steam blue, rounded, with optional icon)
bool ButtonPrimary(const char* label, ImVec2 size = ImVec2(0, 0), icons::DrawFunc icon = nullptr);

// Success button (green, for Play/Install/Claim)
bool ButtonSuccess(const char* label, ImVec2 size = ImVec2(0, 0), icons::DrawFunc icon = nullptr);

// Danger button (red, for Stop/Delete)
bool ButtonDanger(const char* label, ImVec2 size = ImVec2(0, 0));

// Ghost button (transparent bg, border on hover)
bool ButtonGhost(const char* label, ImVec2 size = ImVec2(0, 0), icons::DrawFunc icon = nullptr);

// Icon-only button (circular)
bool ButtonIcon(const char* id, icons::DrawFunc icon, float size = 32.0f, ImU32 color = 0);

// ====================== Badges & Tags ======================

// Pill badge (colored background + text)
void Badge(const char* text, ImU32 bgColor, ImU32 textColor = IM_COL32(255, 255, 255, 230));

// Status indicator dot + text
void StatusDot(const char* text, ImU32 dotColor);

// Tag pill (outlined)
void Tag(const char* text, ImU32 color);

// ====================== Progress ======================

// Modern progress bar with gradient and optional label
void ProgressBar(float fraction, ImVec2 size, ImU32 fillColor, ImU32 bgColor = 0,
                 const char* overlayText = nullptr);

// Circular progress
void ProgressCircle(float fraction, float radius, ImU32 color, float thickness = 3.0f);

// ====================== Cards & Containers ======================

// Start a modern card container (returns draw position)
bool BeginCard(const char* id, ImVec2 size, bool hoverable = true, float rounding = 8.0f);
void EndCard();

// Gradient overlay (for game card covers)
void GradientRect(ImDrawList* dl, ImVec2 topLeft, ImVec2 bottomRight,
                  ImU32 topColor, ImU32 bottomColor, float rounding = 0);

// ====================== Separators & Spacing ======================

// Section header with line
void SectionHeader(const char* text, ImU32 accentColor = 0);

// Subtle divider
void Divider(float width = 0);

// ====================== Tooltip ======================

void ModernTooltip(const char* text);

// ====================== Animation Helpers ======================

// Smooth lerp for animations
float Lerp(float a, float b, float t);

// Get a unique animation state (stored per-ID)
float& GetAnimState(ImGuiID id);

}} // namespace sf::ui
