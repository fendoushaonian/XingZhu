#include "ui/iconfonts.h"
#include <cstdio>

namespace sf {

ImFont* g_iconFont      = nullptr;
ImFont* g_iconFontLarge = nullptr;
ImFont* g_mainFont      = nullptr;
ImFont* g_mainFontSmall = nullptr;
ImFont* g_mainFontLarge = nullptr;

void LoadAllFonts(float dpiScale) {
    ImGuiIO& io = ImGui::GetIO();

    // ---- Glyph ranges ----
    static const ImWchar cjkRanges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin-1 Supplement
        0x0100, 0x017F,  // Latin Extended-A
        0x0180, 0x024F,  // Latin Extended-B
        0x0400, 0x04FF,  // Cyrillic (Russian, etc.)
        0x2000, 0x206F,  // General Punctuation
        0x2100, 0x214F,  // Letterlike Symbols
        0x2190, 0x21FF,  // Arrows
        0x2200, 0x22FF,  // Mathematical Operators
        0x2300, 0x23FF,  // Miscellaneous Technical
        0x2500, 0x257F,  // Box Drawing
        0x2580, 0x259F,  // Block Elements
        0x25A0, 0x25FF,  // Geometric Shapes
        0x2600, 0x26FF,  // Miscellaneous Symbols
        0x2700, 0x27BF,  // Dingbats
        0x3000, 0x30FF,  // CJK Symbols, Hiragana, Katakana
        0x3100, 0x312F,  // Bopomofo
        0x31F0, 0x31FF,  // Katakana Phonetic Extensions
        0xAC00, 0xD7AF,  // Hangul Syllables (Korean)
        0xFF00, 0xFFEF,  // Halfwidth/Fullwidth
        0x4E00, 0x9FFF,  // CJK Unified Ideographs
        0,
    };

    static const ImWchar iconRanges[] = {
        0xE000, 0xF8FF,  // Private Use Area (where MDL2 icons live)
        0,
    };

    // ---- Main Font: Microsoft YaHei UI ----
    const char* mainFontPaths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyhbd.ttc",
        "C:\\Windows\\Fonts\\segoeui.ttf",
    };

    ImFontConfig mainCfg;
    mainCfg.OversampleH = 2;
    mainCfg.OversampleV = 1;
    mainCfg.PixelSnapH  = true;

    for (auto path : mainFontPaths) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);

            // Normal size (16px — clear for CJK)
            mainCfg.GlyphRanges = cjkRanges;
            g_mainFont = io.Fonts->AddFontFromFileTTF(path, 16.0f * dpiScale, &mainCfg, cjkRanges);

            // Small (13px)
            mainCfg.GlyphRanges = cjkRanges;
            g_mainFontSmall = io.Fonts->AddFontFromFileTTF(path, 13.0f * dpiScale, &mainCfg, cjkRanges);

            // Large / heading (22px)
            mainCfg.GlyphRanges = cjkRanges;
            g_mainFontLarge = io.Fonts->AddFontFromFileTTF(path, 22.0f * dpiScale, &mainCfg, cjkRanges);

            fprintf(stderr, "[INFO]  Main font: %s\n", path);
            break;
        }
    }

    if (!g_mainFont) {
        g_mainFont = io.Fonts->AddFontDefault();
        g_mainFontSmall = g_mainFont;
        g_mainFontLarge = g_mainFont;
        fprintf(stderr, "[WARN]  Using default font\n");
    }

    // ---- Icon Font: Segoe MDL2 Assets ----
    const char* iconFontPaths[] = {
        "C:\\Windows\\Fonts\\segmdl2.ttf",     // Segoe MDL2 Assets
        "C:\\Windows\\Fonts\\SegoeIcons.ttf",   // Segoe Fluent Icons (Win11)
    };

    ImFontConfig iconCfg;
    iconCfg.OversampleH = 2;
    iconCfg.OversampleV = 1;
    iconCfg.PixelSnapH  = true;
    iconCfg.GlyphRanges = iconRanges;

    for (auto path : iconFontPaths) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);

            g_iconFont      = io.Fonts->AddFontFromFileTTF(path, 16.0f * dpiScale, &iconCfg, iconRanges);
            g_iconFontLarge = io.Fonts->AddFontFromFileTTF(path, 24.0f * dpiScale, &iconCfg, iconRanges);

            fprintf(stderr, "[INFO]  Icon font: %s\n", path);
            break;
        }
    }

    if (!g_iconFont) {
        fprintf(stderr, "[WARN]  No icon font found, icons will be text fallback\n");
    }

    io.Fonts->Build();
}

} // namespace sf
