#pragma once
#include "imgui.h"

namespace sf {

// Apply the Steam-like dark theme to ImGui
void ApplySteamTheme();

// Load fonts (including Chinese support)
void LoadFonts(float dpiScale = 1.0f);

} // namespace sf
