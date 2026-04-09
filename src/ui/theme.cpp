#include "ui/theme.h"
#include "app/config.h"
#include <cstdio>

namespace sf {

void ApplySteamTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding — Steam uses subtle rounding
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    // Sizing
    style.WindowPadding     = ImVec2(12, 12);
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(8, 6);
    style.ScrollbarSize     = 14.0f;
    style.GrabMinSize       = 12.0f;
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    // Anti-aliased
    style.AntiAliasedLines  = true;
    style.AntiAliasedFill   = true;

    ImVec4* c = style.Colors;

    // Window
    c[ImGuiCol_WindowBg]        = ColorU32(colors::BG_DARK);
    c[ImGuiCol_ChildBg]         = ColorU32(colors::BG_PRIMARY);
    c[ImGuiCol_PopupBg]         = ColorU32(colors::BG_DARKEST);

    // Borders
    c[ImGuiCol_Border]          = ColorU32(colors::BORDER);
    c[ImGuiCol_BorderShadow]    = ImVec4(0, 0, 0, 0);

    // Text
    c[ImGuiCol_Text]            = ColorU32(colors::TEXT_PRIMARY);
    c[ImGuiCol_TextDisabled]    = ColorU32(colors::TEXT_SECONDARY);

    // Frame (input boxes, checkboxes, etc.)
    c[ImGuiCol_FrameBg]         = ColorU32(colors::BG_DARKEST);
    c[ImGuiCol_FrameBgHovered]  = ColorU32(colors::BG_HOVER);
    c[ImGuiCol_FrameBgActive]   = ColorU32(colors::BG_SECONDARY);

    // Title bar
    c[ImGuiCol_TitleBg]         = ColorU32(colors::BG_DARKEST);
    c[ImGuiCol_TitleBgActive]   = ColorU32(colors::BG_DARKEST);
    c[ImGuiCol_TitleBgCollapsed]= ColorU32(colors::BG_DARKEST);

    // Menu bar
    c[ImGuiCol_MenuBarBg]       = ColorU32(colors::BG_DARKEST);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]     = ColorU32(colors::BG_DARKEST);
    c[ImGuiCol_ScrollbarGrab]   = ColorU32(colors::BG_HOVER);
    c[ImGuiCol_ScrollbarGrabHovered] = ColorU32(colors::ACCENT);
    c[ImGuiCol_ScrollbarGrabActive]  = ColorU32(colors::ACCENT_HOVER);

    // Buttons
    c[ImGuiCol_Button]          = ColorU32(colors::BG_SECONDARY);
    c[ImGuiCol_ButtonHovered]   = ColorU32(colors::BG_HOVER);
    c[ImGuiCol_ButtonActive]    = ColorU32(colors::ACCENT);

    // Header (collapsible headers, selectable, menu items)
    c[ImGuiCol_Header]          = ColorU32(colors::BG_SECONDARY);
    c[ImGuiCol_HeaderHovered]   = ColorU32(colors::BG_HOVER);
    c[ImGuiCol_HeaderActive]    = ColorU32(colors::ACCENT);

    // Separator
    c[ImGuiCol_Separator]       = ColorU32(colors::SEPARATOR);
    c[ImGuiCol_SeparatorHovered]= ColorU32(colors::ACCENT);
    c[ImGuiCol_SeparatorActive] = ColorU32(colors::ACCENT_HOVER);

    // Resize grip
    c[ImGuiCol_ResizeGrip]          = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]   = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    c[ImGuiCol_ResizeGripActive]    = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

    // Tabs — Steam style
    c[ImGuiCol_Tab]                 = ColorU32(colors::BG_DARKEST);
    c[ImGuiCol_TabHovered]          = ColorU32(colors::BG_HOVER);
    c[ImGuiCol_TabSelected]         = ColorU32(colors::BG_PRIMARY);
    c[ImGuiCol_TabDimmed]           = ColorU32(colors::BG_DARKEST);
    c[ImGuiCol_TabDimmedSelected]   = ColorU32(colors::BG_DARK);

    // Plot
    c[ImGuiCol_PlotLines]           = ColorU32(colors::ACCENT);
    c[ImGuiCol_PlotLinesHovered]    = ColorU32(colors::ACCENT_HOVER);
    c[ImGuiCol_PlotHistogram]       = ColorU32(colors::GREEN);
    c[ImGuiCol_PlotHistogramHovered]= ColorU32(colors::GREEN_HOVER);

    // Table
    c[ImGuiCol_TableHeaderBg]   = ColorU32(colors::BG_DARKEST);
    c[ImGuiCol_TableBorderStrong]= ColorU32(colors::BORDER);
    c[ImGuiCol_TableBorderLight] = ColorU32(colors::SEPARATOR);
    c[ImGuiCol_TableRowBg]      = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]   = ImVec4(1, 1, 1, 0.02f);

    // Checkbox / slider mark
    c[ImGuiCol_CheckMark]       = ColorU32(colors::ACCENT);
    c[ImGuiCol_SliderGrab]      = ColorU32(colors::ACCENT);
    c[ImGuiCol_SliderGrabActive]= ColorU32(colors::ACCENT_HOVER);

    // Nav
    c[ImGuiCol_NavHighlight]    = ColorU32(colors::ACCENT);

    // Misc
    c[ImGuiCol_DragDropTarget]  = ColorU32(colors::ACCENT);
    c[ImGuiCol_TextSelectedBg]  = ImVec4(0.10f, 0.40f, 0.75f, 0.35f);
    c[ImGuiCol_ModalWindowDimBg]= ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
}

void LoadFonts(float dpiScale) {
    ImGuiIO& io = ImGui::GetIO();

    float fontSize = 16.0f * dpiScale;

    // Try loading Microsoft YaHei for Chinese support
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",    // Microsoft YaHei
        "C:\\Windows\\Fonts\\msyhbd.ttc",   // Microsoft YaHei Bold
        "C:\\Windows\\Fonts\\simhei.ttf",   // SimHei
        "C:\\Windows\\Fonts\\segoeui.ttf",  // Segoe UI (fallback)
    };

    ImFont* mainFont = nullptr;
    for (auto path : fontPaths) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);
            ImFontConfig config;
            config.OversampleH = 2;
            config.OversampleV = 1;
            config.PixelSnapH  = true;

            // Full Chinese glyph range
            static const ImWchar ranges[] = {
                0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x2000, 0x206F, // General Punctuation
                0x3000, 0x30FF, // CJK Symbols, Hiragana, Katakana
                0x31F0, 0x31FF, // Katakana Phonetic Extensions
                0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
                0x4E00, 0x9FAF, // CJK Unified Ideographs
                0x0,
            };
            config.GlyphRanges = ranges;

            mainFont = io.Fonts->AddFontFromFileTTF(path, fontSize, &config, ranges);
            if (mainFont) {
                fprintf(stderr, "[INFO]  Loaded font: %s (%.0fpx)\n", path, fontSize);
                break;
            }
        }
    }

    if (!mainFont) {
        fprintf(stderr, "[WARN]  No CJK font found, using default\n");
        io.Fonts->AddFontDefault();
    }

    io.Fonts->Build();
}

} // namespace sf
