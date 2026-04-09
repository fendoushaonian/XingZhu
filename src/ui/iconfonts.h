#pragma once
#include "imgui.h"

// ============================================================
// Professional Icon Font System
// Uses Windows built-in "Segoe MDL2 Assets" (Win10/11)
// and "Segoe Fluent Icons" (Win11) for crisp vector icons
// ============================================================

namespace sf {

// Global icon font pointer (set during font loading)
extern ImFont* g_iconFont;
extern ImFont* g_iconFontLarge;
extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;

// Load all fonts including icon fonts
void LoadAllFonts(float dpiScale = 1.0f);

// ---- Icon Unicode Codepoints (Segoe MDL2 Assets) ----
namespace icon {
    // Navigation
    constexpr const char* HOME       = u8"\uE80F";
    constexpr const char* BACK       = u8"\uE72B";
    constexpr const char* FORWARD    = u8"\uE72A";
    constexpr const char* REFRESH    = u8"\uE72C";

    // Store / Commerce
    constexpr const char* STORE      = u8"\uE7BF";
    constexpr const char* CART       = u8"\uE7BF";
    constexpr const char* GIFT       = u8"\uECAD";
    constexpr const char* TAG        = u8"\uE8EC";

    // Library / Content
    constexpr const char* LIBRARY    = u8"\uE8F1";
    constexpr const char* GRIDVIEW   = u8"\uE8A9";
    constexpr const char* LISTVIEW   = u8"\uEA37";
    constexpr const char* SORT       = u8"\uE8CB";
    constexpr const char* FILTER     = u8"\uE71C";

    // Media / Gaming
    constexpr const char* PLAY       = u8"\uE768";
    constexpr const char* PAUSE      = u8"\uE769";
    constexpr const char* STOP       = u8"\uE71A";
    constexpr const char* DOWNLOAD   = u8"\uE896";
    constexpr const char* GAMEPAD    = u8"\uE7FC";

    // Social
    constexpr const char* PEOPLE     = u8"\uE716";
    constexpr const char* USER       = u8"\uE77B";
    constexpr const char* CHAT       = u8"\uE8BD";
    constexpr const char* COMMUNITY  = u8"\uE902";

    // System
    constexpr const char* SETTINGS   = u8"\uE713";
    constexpr const char* SEARCH     = u8"\uE721";
    constexpr const char* CLOSE      = u8"\uE711";
    constexpr const char* MINIMIZE   = u8"\uE921";
    constexpr const char* MAXIMIZE   = u8"\uE922";
    constexpr const char* BELL       = u8"\uEA8F";
    constexpr const char* INFO       = u8"\uE946";
    constexpr const char* HELP       = u8"\uE897";

    // Status
    constexpr const char* CHECK      = u8"\uE73E";
    constexpr const char* STAR       = u8"\uE734";
    constexpr const char* STARFILL   = u8"\uE735";
    constexpr const char* HEART      = u8"\uEB51";
    constexpr const char* TROPHY     = u8"\uE7C1";

    // Files / Cloud
    constexpr const char* CLOUD      = u8"\uE753";
    constexpr const char* CLOUDUP    = u8"\uE753";
    constexpr const char* FOLDER     = u8"\uE8B7";
    constexpr const char* SAVE       = u8"\uE74E";

    // Misc
    constexpr const char* SHIELD     = u8"\uE83D";
    constexpr const char* CLOCK      = u8"\uE823";
    constexpr const char* CHART      = u8"\uE9D2";
    constexpr const char* NETWORK    = u8"\uE839";
    constexpr const char* ADD        = u8"\uE710";
    constexpr const char* REMOVE     = u8"\uE738";
    constexpr const char* EDIT       = u8"\uE70F";
    constexpr const char* MORE       = u8"\uE712";
    constexpr const char* CHEVDOWN   = u8"\uE70D";
    constexpr const char* CHEVRIGHT  = u8"\uE76C";
    constexpr const char* CHEVLEFT   = u8"\uE76B";
    constexpr const char* CALENDAR   = u8"\uE787";  // 日历图标
}

// ---- Inline Icon Rendering Helpers ----

// Render icon inline with text (uses icon font, then restores)
inline void Icon(const char* icon, ImU32 color = 0) {
    if (g_iconFont) ImGui::PushFont(g_iconFont);
    if (color) ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(icon);
    if (color) ImGui::PopStyleColor();
    if (g_iconFont) ImGui::PopFont();
}

// Render icon on same line before text
inline void IconText(const char* icon, const char* text, ImU32 iconColor = 0) {
    if (g_iconFont) ImGui::PushFont(g_iconFont);
    if (iconColor) ImGui::PushStyleColor(ImGuiCol_Text, iconColor);
    ImGui::TextUnformatted(icon);
    if (iconColor) ImGui::PopStyleColor();
    if (g_iconFont) ImGui::PopFont();
    ImGui::SameLine(0, 6);
    ImGui::TextUnformatted(text);
}

// Draw icon at specific position on DrawList
inline void DrawIcon(ImDrawList* dl, const char* icon, ImVec2 pos, ImU32 color, ImFont* font = nullptr) {
    if (!font) font = g_iconFont;
    if (font) {
        dl->AddText(font, font->FontSize, pos, color, icon);
    }
}

// Draw large icon
inline void DrawIconLarge(ImDrawList* dl, const char* icon, ImVec2 pos, ImU32 color) {
    if (g_iconFontLarge) {
        dl->AddText(g_iconFontLarge, g_iconFontLarge->FontSize, pos, color, icon);
    }
}

} // namespace sf
