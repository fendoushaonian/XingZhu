#pragma once
#include <string>

namespace sf {

// Application metadata
constexpr const char* APP_NAME    = u8"\u661F\u94F8"; // 星铸
constexpr const char* APP_VERSION = "1.0.0";
constexpr int         APP_WIDTH   = 1400;
constexpr int         APP_HEIGHT  = 900;

// Steam color palette (matches Steam client 2024/2025)
namespace colors {
    // Backgrounds
    constexpr unsigned int BG_DARKEST   = 0xFF0E141B;  // sidebar / deepest bg
    constexpr unsigned int BG_DARK      = 0xFF171A21;  // main background
    constexpr unsigned int BG_PRIMARY   = 0xFF1B2838;  // primary panels
    constexpr unsigned int BG_SECONDARY = 0xFF213345;  // elevated panels
    constexpr unsigned int BG_HOVER     = 0xFF2A475E;  // hover state

    // Accent
    constexpr unsigned int ACCENT       = 0xFF1A9FFF;  // Steam blue
    constexpr unsigned int ACCENT_HOVER = 0xFF66C0F4;  // lighter blue
    constexpr unsigned int GREEN        = 0xFF4C6B22;  // install/play button
    constexpr unsigned int GREEN_HOVER  = 0xFF5C7E2A;

    // Text
    constexpr unsigned int TEXT_PRIMARY   = 0xFFC7D5E0;  // main text
    constexpr unsigned int TEXT_SECONDARY = 0xFF8F98A0;  // muted text
    constexpr unsigned int TEXT_BRIGHT    = 0xFFFFFFFF;  // headings

    // Borders / separators
    constexpr unsigned int BORDER       = 0xFF2A3F5F;
    constexpr unsigned int SEPARATOR    = 0xFF1E2C3A;
}

// ImGui color helpers: convert ARGB u32 to ImVec4
inline ImVec4 ColorU32(unsigned int argb) {
    return ImVec4(
        ((argb >> 16) & 0xFF) / 255.0f,  // R
        ((argb >>  8) & 0xFF) / 255.0f,  // G
        ((argb      ) & 0xFF) / 255.0f,  // B
        ((argb >> 24) & 0xFF) / 255.0f   // A
    );
}

} // namespace sf
