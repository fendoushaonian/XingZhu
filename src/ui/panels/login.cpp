#include "ui/panels/login.h"
#include "core/auth.h"
#include "core/email.h"
#include "core/texture_manager.h"
#include "utils/logger.h"
#include "ui/iconfonts.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <shellapi.h>

namespace sf {

extern ImFont* g_mainFontLarge;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFont;
extern ImFont* g_iconFont;

// Popular game appIds for background promo images
static const char* kBgGameIds[] = {
    "1091500",  // Cyberpunk 2077
    "1245620",  // Elden Ring
    "1174180",  // Red Dead Redemption 2
    "292030",   // The Witcher 3
    "1086940",  // Baldur's Gate 3
    "730",      // CS2
    "570",      // Dota 2
    "1517290",  // Battlefield 2042
    "1593500",  // God of War
    "814380",   // Sekiro
    "1172470",  // Apex Legends
    "271590",   // GTA V
    "2358720",  // Black Myth: Wukong
    "413150",   // Stardew Valley
    "1238810",  // Armored Core VI
    "381210",   // Dead by Daylight
};
static const int kBgGameCount = sizeof(kBgGameIds) / sizeof(kBgGameIds[0]);

// ────────────────────────────────────────────────────────
// Background — game promo image mosaic + frosted overlay
// ────────────────────────────────────────────────────────
void LoginPanel::RenderBackground(ImDrawList* bg, float winW, float winH, float t) {
    // Request textures once
    if (!bgImagesRequested_) {
        bgImagesRequested_ = true;
        for (int i = 0; i < kBgGameCount; i++) {
            std::string key = std::string("login_bg_") + kBgGameIds[i];
            // Use library_hero images (large, cinematic, 3840x1240 but we just need fill)
            std::string url = std::string("https://cdn.akamai.steamstatic.com/steam/apps/")
                              + kBgGameIds[i] + "/library_hero.jpg";
            RequestTextureFromUrl(key, url);
            // Also request header as fallback (460x215)
            std::string key2 = std::string("login_hdr_") + kBgGameIds[i];
            std::string url2 = std::string("https://cdn.akamai.steamstatic.com/steam/apps/")
                               + kBgGameIds[i] + "/header.jpg";
            RequestTextureFromUrl(key2, url2);
        }
    }

    // Dark base
    bg->AddRectFilled(ImVec2(0, 0), ImVec2(winW, winH), IM_COL32(8, 12, 20, 255));

    // Slow scroll
    bgScrollX_ += ImGui::GetIO().DeltaTime * 12.0f;

    // Tile game images in rows
    // Each tile: header image (460x215 aspect) → render at fixed tile size
    float tileW = 320.0f;
    float tileH = 150.0f;
    float gap = 8.0f;
    float stepX = tileW + gap;
    float stepY = tileH + gap;
    int rows = (int)(winH / stepY) + 2;
    int cols = (int)(winW / stepX) + 3;

    int imgIdx = 0;
    for (int row = 0; row < rows; row++) {
        // Alternate row offset for brick pattern
        float rowOffset = (row % 2 == 0) ? 0 : stepX * 0.5f;
        // Each row scrolls at slightly different speed
        float scrollOff = fmodf(bgScrollX_ * (0.6f + row * 0.08f) + rowOffset, stepX);

        for (int col = -1; col < cols; col++) {
            float tx = col * stepX - scrollOff;
            float ty = row * stepY - gap;

            // Skip tiles completely off screen
            if (tx + tileW < -stepX || tx > winW + stepX) continue;
            if (ty + tileH < -stepY || ty > winH + stepY) continue;

            // Pick a game image (cycle through)
            int gi = (imgIdx++) % kBgGameCount;
            std::string heroKey = std::string("login_bg_") + kBgGameIds[gi];
            std::string hdrKey = std::string("login_hdr_") + kBgGameIds[gi];

            ImTextureID tex = GetTextureByKey(heroKey);
            if (!tex) tex = GetTextureByKey(hdrKey);

            if (tex) {
                ImU32 tintCol = IM_COL32(255, 255, 255, 150);
                bg->AddImageRounded(tex, ImVec2(tx, ty), ImVec2(tx + tileW, ty + tileH),
                                    ImVec2(0, 0), ImVec2(1, 1), tintCol, 6.0f);
            } else {
                bg->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + tileW, ty + tileH),
                                  IM_COL32(14, 20, 30, 120), 6.0f);
            }
        }
    }

    // ── Light overlay — just enough to unify, not bury ──
    bg->AddRectFilled(ImVec2(0, 0), ImVec2(winW, winH), IM_COL32(6, 10, 18, 100));

    // Edge vignette (fade to black at borders)
    float vigW = winW * 0.18f;
    float vigH = winH * 0.12f;
    bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(vigW, winH),
                                IM_COL32(4, 8, 16, 160), IM_COL32(4, 8, 16, 0),
                                IM_COL32(4, 8, 16, 0), IM_COL32(4, 8, 16, 160));
    bg->AddRectFilledMultiColor(ImVec2(winW - vigW, 0), ImVec2(winW, winH),
                                IM_COL32(4, 8, 16, 0), IM_COL32(4, 8, 16, 160),
                                IM_COL32(4, 8, 16, 160), IM_COL32(4, 8, 16, 0));
    bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(winW, vigH),
                                IM_COL32(4, 8, 16, 140), IM_COL32(4, 8, 16, 140),
                                IM_COL32(4, 8, 16, 0), IM_COL32(4, 8, 16, 0));
    bg->AddRectFilledMultiColor(ImVec2(0, winH - vigH), ImVec2(winW, winH),
                                IM_COL32(4, 8, 16, 0), IM_COL32(4, 8, 16, 0),
                                IM_COL32(4, 8, 16, 140), IM_COL32(4, 8, 16, 140));

    // A few floating particles on top for polish
    for (int i = 0; i < 20; i++) {
        float seed = i * 137.508f;
        float px = fmodf(seed + t * (2.0f + (i % 5) * 0.6f), winW + 20) - 10;
        float py = fmodf(seed * 0.73f + t * (1.0f + (i % 4) * 0.4f), winH + 20) - 10;
        float sz = 1.0f + (i % 3) * 0.4f;
        float alpha = 25.0f + 15.0f * sinf(t * 0.6f + i * 0.4f);
        bg->AddCircleFilled(ImVec2(px, py), sz, IM_COL32(26, 159, 255, (int)alpha));
    }
}

// ────────────────────────────────────────────────────────
// Card — glass panel with glow border
// ────────────────────────────────────────────────────────
void LoginPanel::RenderCard(ImDrawList* bg, float cx, float cy, float cw, float ch, float t) {
    float r = 16.0f;

    // Outer glow shadow (multiple layers for softness)
    for (int i = 5; i >= 1; i--) {
        float off = (float)i * 4.0f;
        int a = 6 + i * 3;
        bg->AddRectFilled(ImVec2(cx - off, cy - off), ImVec2(cx + cw + off, cy + ch + off),
                          IM_COL32(10, 40, 120, a), r + off);
    }

    // Main card fill — frosted glass
    ImU32 fillTop = IM_COL32(18, 25, 40, 230);
    ImU32 fillBot = IM_COL32(12, 18, 30, 245);
    bg->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + ch),
                                fillTop, fillTop, fillBot, fillBot);
    bg->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + ch),
                      IM_COL32(255, 255, 255, 3), r);

    // Top accent glow bar
    float glowPhase = sinf(t * 1.5f) * 0.3f + 0.7f;
    int glowA = (int)(180 * glowPhase);
    bg->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + 2.5f),
                      IM_COL32(26, 159, 255, glowA), r, ImDrawFlags_RoundCornersTop);

    // Border
    ImU32 borderCol = IM_COL32(40, 80, 140, (int)(50 + 20 * sinf(t * 2.0f)));
    bg->AddRect(ImVec2(cx, cy), ImVec2(cx + cw, cy + ch), borderCol, r, 0, 1.2f);

    // Inner highlight on top edge
    bg->AddLine(ImVec2(cx + 20, cy + 1), ImVec2(cx + cw - 20, cy + 1),
                IM_COL32(100, 160, 255, 25));
}

// ────────────────────────────────────────────────────────
// Logo — PNG image (static, no animation)
// ────────────────────────────────────────────────────────
void LoginPanel::RenderLogo(ImDrawList* bg, float cx, float cy, float t) {
    // Load logo texture - retry if not loaded yet
    static ImTextureID logoTex = (ImTextureID)0;
    static int logoW = 0, logoH = 0;

    if (!logoTex) {
        logoTex = LoadTextureFromLocalFile("login_logo", "resources/icons/login.png");
        GetTextureSize("login_logo", logoW, logoH);
    }

    // Display size - static, no animation
    float size = 190.0f;

    // Draw logo image directly
    if (logoTex && logoW > 0 && logoH > 0) {
        float aspect = (float)logoW / (float)logoH;
        float drawW = size;
        float drawH = size / aspect;
        if (aspect < 1.0f) {
            drawH = size;
            drawW = size * aspect;
        }
        float x0 = cx - drawW * 0.5f;
        float y0 = cy - drawH * 0.5f;
        bg->AddImageRounded(logoTex, ImVec2(x0, y0), ImVec2(x0 + drawW, y0 + drawH),
                            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);
    } else {
        // Fallback: simple text if image not loaded
        const char* text = "SF";
        ImFont* font = g_mainFontLarge ? g_mainFontLarge : ImGui::GetFont();
        ImVec2 textSize = font->CalcTextSizeA(font->FontSize * 1.5f, FLT_MAX, 0, text);
        bg->AddText(font, font->FontSize * 1.5f,
                    ImVec2(cx - textSize.x * 0.5f, cy - textSize.y * 0.5f),
                    IM_COL32(26, 159, 255, 255), text);
    }
}

// ────────────────────────────────────────────────────────
// Custom Input Field
// ────────────────────────────────────────────────────────
bool LoginPanel::RenderInputField(const char* id, const char* label, const char* iconGlyph,
                                   char* buf, int bufSize, float x, float y, float w,
                                   bool isPassword, bool enterReturns) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float fieldH = 44.0f;
    float labelH = 18.0f;

    // Label above field
    ImFont* labelFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
    dl->AddText(labelFont, labelFont->FontSize,
                ImVec2(x + 2, y), IM_COL32(140, 155, 175, 200), label);

    float fy = y + labelH;

    // Focus state from previous frame
    bool isFocused = false;
    if (strcmp(id, "##loginUser") == 0 || strcmp(id, "##regUser") == 0) isFocused = focusUser_;
    else if (strcmp(id, "##loginPass") == 0 || strcmp(id, "##regPass") == 0) isFocused = focusPass_;
    else if (strcmp(id, "##regPass2") == 0) isFocused = focusPass2_;
    else if (strcmp(id, "##regNick") == 0) isFocused = focusNick_;
    else if (strcmp(id, "##regEmail") == 0) isFocused = focusEmail_;
    else if (strcmp(id, "##regCode") == 0) isFocused = focusCode_;

    // Field background
    ImU32 fieldBg = isFocused ? IM_COL32(16, 24, 40, 255) : IM_COL32(10, 16, 28, 255);
    ImU32 fieldBorder = isFocused ? IM_COL32(26, 159, 255, 180) : IM_COL32(40, 60, 90, 100);
    float rounding = 8.0f;

    dl->AddRectFilled(ImVec2(x, fy), ImVec2(x + w, fy + fieldH), fieldBg, rounding);
    dl->AddRect(ImVec2(x, fy), ImVec2(x + w, fy + fieldH), fieldBorder, rounding, 0, 1.2f);

    // Focus glow
    if (isFocused) {
        for (int i = 1; i <= 3; i++) {
            float off = (float)i * 1.5f;
            dl->AddRect(ImVec2(x - off, fy - off), ImVec2(x + w + off, fy + fieldH + off),
                        IM_COL32(26, 159, 255, 15 - i * 4), rounding + off);
        }
    }

    // Icon inside field
    float iconW = 36.0f;
    if (g_iconFont && iconGlyph) {
        ImVec2 iconSize = g_iconFont->CalcTextSizeA(g_iconFont->FontSize, FLT_MAX, 0, iconGlyph);
        float iconX = x + (iconW - iconSize.x) * 0.5f;
        float iconY = fy + (fieldH - iconSize.y) * 0.5f;
        ImU32 iconCol = isFocused ? IM_COL32(26, 159, 255, 220) : IM_COL32(80, 100, 130, 150);
        dl->AddText(g_iconFont, g_iconFont->FontSize, ImVec2(iconX, iconY), iconCol, iconGlyph);

        dl->AddLine(ImVec2(x + iconW, fy + 10), ImVec2(x + iconW, fy + fieldH - 10),
                    IM_COL32(40, 60, 90, 60));
    }

    // Invisible ImGui InputText overlaid
    float inputX = x + iconW + 8;
    float inputW = w - iconW - 16;

    ImGui::SetCursorScreenPos(ImVec2(inputX, fy + 2));

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210, 225, 240, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, (fieldH - ImGui::GetFontSize()) * 0.5f - 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

    ImGui::SetNextItemWidth(inputW);
    ImGuiInputTextFlags flags = 0;
    if (isPassword) flags |= ImGuiInputTextFlags_Password;
    if (enterReturns) flags |= ImGuiInputTextFlags_EnterReturnsTrue;

    bool enterPressed = ImGui::InputText(id, buf, bufSize, flags);

    // Track focus state for next frame
    bool nowFocused = ImGui::IsItemActive();
    if (strcmp(id, "##loginUser") == 0 || strcmp(id, "##regUser") == 0) focusUser_ = nowFocused;
    else if (strcmp(id, "##loginPass") == 0 || strcmp(id, "##regPass") == 0) focusPass_ = nowFocused;
    else if (strcmp(id, "##regPass2") == 0) focusPass2_ = nowFocused;
    else if (strcmp(id, "##regNick") == 0) focusNick_ = nowFocused;
    else if (strcmp(id, "##regEmail") == 0) focusEmail_ = nowFocused;
    else if (strcmp(id, "##regCode") == 0) focusCode_ = nowFocused;

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);

    return enterPressed;
}

// ────────────────────────────────────────────────────────
// Custom Button
// ────────────────────────────────────────────────────────
bool LoginPanel::RenderButton(const char* label, float x, float y, float w, float h,
                               ImU32 color, ImU32 hoverColor, bool isPrimary) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rounding = 10.0f;

    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImU32 bgCol = active ? IM_COL32(
        (int)((color >> 0) & 0xFF) * 0.7f,
        (int)((color >> 8) & 0xFF) * 0.7f,
        (int)((color >> 16) & 0xFF) * 0.7f, 255)
        : hovered ? hoverColor : color;

    if (isPrimary) {
        dl->AddRectFilled(ImVec2(x + 2, y + 3), ImVec2(x + w - 2, y + h + 3),
                          IM_COL32(0, 0, 0, 40), rounding);
    }

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), bgCol, rounding);

    if (isPrimary && !active) {
        ImU32 highlight = hovered ? IM_COL32(255, 255, 255, 30) : IM_COL32(255, 255, 255, 15);
        dl->AddRectFilled(ImVec2(x + 1, y + 1), ImVec2(x + w - 1, y + h * 0.45f),
                          highlight, rounding, ImDrawFlags_RoundCornersTop);
    }

    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                IM_COL32(255, 255, 255, hovered ? 30 : 10), rounding);

    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, label);
    float tx = x + (w - ts.x) * 0.5f;
    float ty = y + (h - ts.y) * 0.5f;
    dl->AddText(font, font->FontSize, ImVec2(tx, ty), IM_COL32(255, 255, 255, 245), label);

    return clicked;
}

// ────────────────────────────────────────────────────────
// ────────────────────────────────────────────────────────
// OAuth Icon Textures (loaded from official icons)
// ────────────────────────────────────────────────────────
static bool s_oauthIconsLoaded = false;
static ImTextureID s_steamIconTex = 0;
static ImTextureID s_githubIconTex = 0;

// Get executable directory
static std::string GetExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string p(path);
    size_t pos = p.find_last_of("\\/");
    return (pos != std::string::npos) ? p.substr(0, pos) : ".";
}

static void LoadOAuthIcons() {
    if (s_oauthIconsLoaded) return;
    s_oauthIconsLoaded = true;

    std::string exeDir = GetExeDir();
    Log(LogLevel::Info, "Loading OAuth icons from: %s", exeDir.c_str());

    // Try multiple paths: exe dir, parent dir (for dev), current dir
    std::vector<std::string> searchPaths = {
        exeDir + "\\resources\\icons\\",
        exeDir + "\\..\\..\\resources\\icons\\",  // build/Release -> project root
        "resources\\icons\\"
    };

    for (const auto& basePath : searchPaths) {
        std::string steamPath = basePath + "steam.png";
        std::string githubPath = basePath + "github-white.png";

        // Check if files exist
        DWORD attr = GetFileAttributesA(steamPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            Log(LogLevel::Info, "Found icons at: %s", basePath.c_str());
            s_steamIconTex = LoadTextureFromLocalFile("oauth_steam", steamPath);
            s_githubIconTex = LoadTextureFromLocalFile("oauth_github", githubPath);
            if (s_steamIconTex && s_githubIconTex) {
                Log(LogLevel::Info, "OAuth icons loaded successfully");
                return;
            }
        }
    }

    Log(LogLevel::Warn, "Failed to load OAuth icons from any path");
}

// ────────────────────────────────────────────────────────
// Icon Button (for OAuth) with official icon textures
// ────────────────────────────────────────────────────────
bool LoginPanel::RenderIconButton(const char* id, float x, float y, float size,
                                   ImU32 bgColor, ImU32 hoverColor, const char* iconType) {
    // Load icons on first call
    LoadOAuthIcons();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float rounding = size * 0.25f;

    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::InvisibleButton(id, ImVec2(size, size));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImU32 bgCol = active ? IM_COL32(
        (int)((bgColor >> 0) & 0xFF) * 0.7f,
        (int)((bgColor >> 8) & 0xFF) * 0.7f,
        (int)((bgColor >> 16) & 0xFF) * 0.7f, 255)
        : hovered ? hoverColor : bgColor;

    // Shadow
    dl->AddRectFilled(ImVec2(x + 2, y + 3), ImVec2(x + size - 2, y + size + 3),
                      IM_COL32(0, 0, 0, 30), rounding);

    // Background
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + size, y + size), bgCol, rounding);

    // Border
    dl->AddRect(ImVec2(x, y), ImVec2(x + size, y + size),
                IM_COL32(255, 255, 255, hovered ? 40 : 15), rounding);

    // Draw icon texture
    float iconSize = size * 0.6f;
    float iconX = x + (size - iconSize) * 0.5f;
    float iconY = y + (size - iconSize) * 0.5f;

    ImTextureID tex = 0;
    if (strcmp(iconType, "steam") == 0) {
        tex = s_steamIconTex;
    } else if (strcmp(iconType, "github") == 0) {
        tex = s_githubIconTex;
    }

    if (tex) {
        dl->AddImageRounded(tex,
                            ImVec2(iconX, iconY),
                            ImVec2(iconX + iconSize, iconY + iconSize),
                            ImVec2(0, 0), ImVec2(1, 1),
                            IM_COL32(255, 255, 255, 255),
                            rounding * 0.3f);
    }

    return clicked;
}

// ────────────────────────────────────────────────────────
// Handle OAuth Callback
// ────────────────────────────────────────────────────────
void LoginPanel::HandleOAuthCallback() {
    if (!oauthWaiting_) return;

    if (IsOAuthCallbackPending()) {
        std::string result = GetOAuthResult();
        std::string err;

        if (oauthType_ == "steam") {
            if (AuthLoginWithSteam(result, err)) {
                // Stop OAuth server BEFORE setting authenticated
                // This ensures clean state before main UI renders
                StopOAuthCallbackServer();
                authenticated_ = true;
                oauthWaiting_ = false;
                oauthType_.clear();
            } else {
                errorMsg_ = err;
                msgTimer_ = 3.0f;
                oauthWaiting_ = false;
            }
        } else if (oauthType_ == "github") {
            if (AuthLoginWithGitHub(result, err)) {
                // Stop OAuth server BEFORE setting authenticated
                StopOAuthCallbackServer();
                authenticated_ = true;
                oauthWaiting_ = false;
                oauthType_.clear();
            } else {
                errorMsg_ = err;
                msgTimer_ = 3.0f;
                oauthWaiting_ = false;
            }
        }
    }
}

// ────────────────────────────────────────────────────────
// Main Render
// ────────────────────────────────────────────────────────
void LoginPanel::Render(float winW, float winH) {
    if (authenticated_) return;

    // Handle OAuth callback
    HandleOAuthCallback();

    float t = (float)ImGui::GetTime();
    float dt = ImGui::GetIO().DeltaTime;

    formAlpha_ = std::min(formAlpha_ + dt * 3.0f, 1.0f);
    cardSlide_ = cardSlide_ + (0.0f - cardSlide_) * std::min(dt * 6.0f, 1.0f);

    ImDrawList* bg = ImGui::GetBackgroundDrawList();

    // ── Background ──
    RenderBackground(bg, winW, winH, t);

    // ── Card ──
    float cardW = 440.0f;
    float cardH = showRegister_ ? 680.0f : 650.0f;
    float cardX = (winW - cardW) * 0.5f;
    float cardY = (winH - cardH) * 0.5f + cardSlide_;

    RenderCard(bg, cardX, cardY, cardW, cardH, t);

    // ── Logo ──
    float logoX = cardX + cardW * 0.5f;
    float logoY = cardY + 85;  // logo位置
    RenderLogo(bg, logoX, logoY, t);

    // ── Title ──
    const char* title = u8"星铸畅玩";
    ImFont* titleFont = g_mainFontLarge ? g_mainFontLarge : ImGui::GetFont();
    float titleFontSize = titleFont->FontSize * 1.8f;  // 字体更大
    ImVec2 ts = titleFont->CalcTextSizeA(titleFontSize, FLT_MAX, 0, title);
    float titleY = logoY + 70;  // 紧凑间距
    bg->AddText(titleFont, titleFontSize,
                ImVec2(logoX - ts.x * 0.5f + 1, titleY + 1),
                IM_COL32(26, 120, 255, 40), title);
    bg->AddText(titleFont, titleFontSize,
                ImVec2(logoX - ts.x * 0.5f, titleY),
                IM_COL32(200, 225, 255, 255), title);

    // ── Subtitle ──
    const char* subtitle = showRegister_ ?
        u8"\u521B\u5EFA\u65B0\u8D26\u6237" :
        u8"\u767B\u5F55\u60A8\u7684\u8D26\u6237";
    ImFont* subFont = g_mainFont ? g_mainFont : ImGui::GetFont();  // 用普通字体，更大
    float subFontSize = subFont->FontSize * 1.1f;  // 稍微放大
    ImVec2 ss = subFont->CalcTextSizeA(subFontSize, FLT_MAX, 0, subtitle);
    bg->AddText(subFont, subFontSize,
                ImVec2(logoX - ss.x * 0.5f, titleY + titleFontSize + 8),
                IM_COL32(120, 140, 170, 180), subtitle);

    // ── Form ──
    float formX = cardX + 40;
    float formW = cardW - 80;
    float formY = titleY + titleFontSize + 36;
    float formH = cardY + cardH - formY - 20;

    ImGui::SetNextWindowPos(ImVec2(formX, formY));
    ImGui::SetNextWindowSize(ImVec2(formW, formH));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("##LoginForm", nullptr, wf)) {
        float cy = formY;

        const char* userIcon = u8"\uE77B";
        const char* lockIcon = u8"\uE72E";
        const char* nameIcon = u8"\uE779";

        if (showRegister_) {
            // Email icon (Segoe MDL2: mail)
            const char* mailIcon = u8"\uE715";

            RenderInputField("##regUser", u8"\u7528\u6237\u540D", userIcon,
                             regUser_, sizeof(regUser_), formX, cy, formW);
            cy += 66;

            RenderInputField("##regNick", u8"\u6635\u79F0", nameIcon,
                             regNick_, sizeof(regNick_), formX, cy, formW);
            cy += 66;

            // ── Email field with embedded send button ──
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float fieldH = 44.0f;
                float labelH = 18.0f;
                float rounding = 8.0f;

                // Label
                ImFont* labelFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
                dl->AddText(labelFont, labelFont->FontSize,
                            ImVec2(formX + 2, cy), IM_COL32(140, 155, 175, 200), u8"\u90AE\u7BB1");
                float fy = cy + labelH;

                bool isFocused = focusEmail_;
                ImU32 fieldBg = isFocused ? IM_COL32(16, 24, 40, 255) : IM_COL32(10, 16, 28, 255);
                ImU32 fieldBorder = isFocused ? IM_COL32(26, 159, 255, 180) : IM_COL32(40, 60, 90, 100);

                // Full-width visual field box
                dl->AddRectFilled(ImVec2(formX, fy), ImVec2(formX + formW, fy + fieldH), fieldBg, rounding);
                dl->AddRect(ImVec2(formX, fy), ImVec2(formX + formW, fy + fieldH), fieldBorder, rounding, 0, 1.2f);
                if (isFocused) {
                    for (int i = 1; i <= 3; i++) {
                        float off = (float)i * 1.5f;
                        dl->AddRect(ImVec2(formX - off, fy - off), ImVec2(formX + formW + off, fy + fieldH + off),
                                    IM_COL32(26, 159, 255, 15 - i * 4), rounding + off);
                    }
                }

                // Mail icon
                float iconW = 36.0f;
                if (g_iconFont) {
                    ImVec2 iconSz = g_iconFont->CalcTextSizeA(g_iconFont->FontSize, FLT_MAX, 0, mailIcon);
                    dl->AddText(g_iconFont, g_iconFont->FontSize,
                                ImVec2(formX + (iconW - iconSz.x) * 0.5f, fy + (fieldH - iconSz.y) * 0.5f),
                                isFocused ? IM_COL32(26, 159, 255, 220) : IM_COL32(80, 100, 130, 150), mailIcon);
                    dl->AddLine(ImVec2(formX + iconW, fy + 10), ImVec2(formX + iconW, fy + fieldH - 10),
                                IM_COL32(40, 60, 90, 60));
                }

                // ── Send code button (inside field, right side) ──
                float btnW = 90.0f;
                float btnH = 30.0f;
                float btnGap = 6.0f;
                float btnX = formX + formW - btnW - btnGap;
                float btnY = fy + (fieldH - btnH) * 0.5f; // vertically centered

                int cooldown = GetCodeCooldown(regEmail_);
                bool canSend = (cooldown == 0 && strlen(regEmail_) > 3);

                // Draw the button FIRST (InvisibleButton before InputText to get priority)
                ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
                ImGui::InvisibleButton("##sendCode", ImVec2(btnW, btnH));
                bool sendClicked = ImGui::IsItemClicked() && canSend;
                bool sendHovered = ImGui::IsItemHovered() && canSend;

                ImU32 btnColor = canSend ? IM_COL32(26, 120, 220, 255) : IM_COL32(40, 50, 70, 180);
                ImU32 btnHoverCol = canSend ? IM_COL32(40, 150, 255, 255) : IM_COL32(40, 50, 70, 180);
                ImU32 bg2 = sendHovered ? btnHoverCol : btnColor;
                dl->AddRectFilled(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH), bg2, 6.0f);
                dl->AddRect(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH),
                            IM_COL32(255, 255, 255, canSend ? 20 : 5), 6.0f);

                char codeBtnLabel[64];
                if (cooldown > 0)
                    snprintf(codeBtnLabel, sizeof(codeBtnLabel), u8"%ds", cooldown);
                else
                    snprintf(codeBtnLabel, sizeof(codeBtnLabel), u8"\u53D1\u9001\u9A8C\u8BC1\u7801");

                ImFont* bf = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
                ImVec2 bts = bf->CalcTextSizeA(bf->FontSize, FLT_MAX, 0, codeBtnLabel);
                dl->AddText(bf, bf->FontSize,
                            ImVec2(btnX + (btnW - bts.x) * 0.5f, btnY + (btnH - bts.y) * 0.5f),
                            IM_COL32(255, 255, 255, canSend ? 240 : 100), codeBtnLabel);

                if (sendClicked) {
                    if (SendVerifyCode(regEmail_)) {
                        codeSent_ = true;
                        errorMsg_.clear();
                        successMsg_.clear();
                    } else {
                        errorMsg_ = u8"\u53D1\u9001\u5931\u8D25\uFF0C\u8BF7\u68C0\u67E5\u90AE\u7BB1";
                        successMsg_.clear();
                        msgTimer_ = 3.0f;
                    }
                }

                // ── Email InputText (shorter, leaves room for button) ──
                float inputX = formX + iconW + 8;
                float inputW = btnX - inputX - 8; // stop before the button

                ImGui::SetCursorScreenPos(ImVec2(inputX, fy + 2));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210, 225, 240, 255));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, (fieldH - ImGui::GetFontSize()) * 0.5f - 2));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
                ImGui::SetNextItemWidth(inputW);
                ImGui::InputText("##regEmail", regEmail_, sizeof(regEmail_));
                focusEmail_ = ImGui::IsItemActive();
                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(5);
            }
            cy += 66;

            // ── Verification code field ──
            const char* codeIcon = u8"\uE8D7"; // shield icon
            RenderInputField("##regCode", u8"\u9A8C\u8BC1\u7801", codeIcon,
                             regCode_, sizeof(regCode_), formX, cy, formW);
            cy += 66;

            // ── Password field ──
            bool enterReg = RenderInputField("##regPass", u8"\u5BC6\u7801", lockIcon,
                             regPass_, sizeof(regPass_), formX, cy, formW, true, true);
            cy += 74;

            // ── Register button ──
            bool doReg = RenderButton(u8"\u6CE8  \u518C", formX, cy, formW, 46,
                                      IM_COL32(26, 120, 220, 255),
                                      IM_COL32(40, 150, 255, 255)) || enterReg;
            cy += 58;

            if (doReg) {
                if (regUser_[0] == '\0') {
                    errorMsg_ = u8"\u8BF7\u8F93\u5165\u7528\u6237\u540D"; // 请输入用户名
                    successMsg_.clear();
                } else if (regPass_[0] == '\0') {
                    errorMsg_ = u8"\u8BF7\u8F93\u5165\u5BC6\u7801"; // 请输入密码
                    successMsg_.clear();
                } else if (strlen(regEmail_) < 4 || !strchr(regEmail_, '@')) {
                    errorMsg_ = u8"\u8BF7\u8F93\u5165\u6709\u6548\u90AE\u7BB1"; // 请输入有效邮箱
                    successMsg_.clear();
                } else if (strlen(regCode_) == 0) {
                    errorMsg_ = u8"\u8BF7\u8F93\u5165\u9A8C\u8BC1\u7801"; // 请输入验证码
                    successMsg_.clear();
                } else if (!CheckVerifyCode(regEmail_, regCode_)) {
                    errorMsg_ = u8"\u9A8C\u8BC1\u7801\u9519\u8BEF\u6216\u5DF2\u8FC7\u671F"; // 验证码错误或已过期
                    successMsg_.clear();
                } else {
                    std::string err;
                    if (AuthRegister(regUser_, regPass_, regNick_, regEmail_, err)) {
                        successMsg_ = u8"\u6CE8\u518C\u6210\u529F\uFF0C\u8BF7\u767B\u5F55";
                        errorMsg_.clear();
                        strncpy(loginUser_, regUser_, sizeof(loginUser_));
                        loginPass_[0] = '\0';
                        showRegister_ = false;
                    } else {
                        errorMsg_ = err;
                        successMsg_.clear();
                    }
                }
                msgTimer_ = 3.0f;
            }

            // Inline error/success message — below register button
            if (msgTimer_ > 0) {
                msgTimer_ -= dt;
                ImFont* mf = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
                ImDrawList* mdl = ImGui::GetWindowDrawList();
                float alpha = msgTimer_ < 0.5f ? msgTimer_ * 2.0f : 1.0f;
                if (!errorMsg_.empty()) {
                    ImVec2 ms = mf->CalcTextSizeA(mf->FontSize, FLT_MAX, 0, errorMsg_.c_str());
                    mdl->AddText(mf, mf->FontSize,
                                ImVec2(formX + (formW - ms.x) * 0.5f, cy),
                                IM_COL32(255, 90, 70, (int)(220 * alpha)), errorMsg_.c_str());
                } else if (!successMsg_.empty()) {
                    ImVec2 ms = mf->CalcTextSizeA(mf->FontSize, FLT_MAX, 0, successMsg_.c_str());
                    mdl->AddText(mf, mf->FontSize,
                                ImVec2(formX + (formW - ms.x) * 0.5f, cy),
                                IM_COL32(100, 220, 120, (int)(220 * alpha)), successMsg_.c_str());
                }
            }
            cy += 22;

            ImGui::SetCursorScreenPos(ImVec2(formX, cy));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(102, 192, 244, 200));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 0, 0, 0));
            if (ImGui::SmallButton(u8"\u5DF2\u6709\u8D26\u6237\uFF1F\u8FD4\u56DE\u767B\u5F55")) {
                showRegister_ = false;
                errorMsg_.clear(); successMsg_.clear();
                cardSlide_ = 20.0f;
            }
            ImGui::PopStyleColor(4);

        } else {
            RenderInputField("##loginUser", u8"\u7528\u6237\u540D", userIcon,
                             loginUser_, sizeof(loginUser_), formX, cy, formW);
            cy += 72;

            bool enterLogin = RenderInputField("##loginPass", u8"\u5BC6\u7801", lockIcon,
                             loginPass_, sizeof(loginPass_), formX, cy, formW, true, true);
            cy += 82;

            bool doLogin = RenderButton(u8"\u767B  \u5F55", formX, cy, formW, 46,
                                        IM_COL32(76, 107, 34, 255),
                                        IM_COL32(96, 135, 44, 255)) || enterLogin;
            cy += 54;

            if (doLogin) {
                if (loginUser_[0] == '\0' || loginPass_[0] == '\0') {
                    errorMsg_ = u8"\u8BF7\u8F93\u5165\u7528\u6237\u540D\u548C\u5BC6\u7801"; // 请输入用户名和密码
                    successMsg_.clear();
                    msgTimer_ = 3.0f;
                } else {
                    std::string err;
                    if (AuthLogin(loginUser_, loginPass_, err)) {
                        authenticated_ = true;
                        errorMsg_.clear();
                    } else {
                        errorMsg_ = err;
                        successMsg_.clear();
                        msgTimer_ = 3.0f;
                    }
                }
            }

            // Inline error/success message — right below login button
            if (msgTimer_ > 0) {
                msgTimer_ -= dt;
                ImFont* mf = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float alpha = msgTimer_ < 0.5f ? msgTimer_ * 2.0f : 1.0f; // fade out
                if (!errorMsg_.empty()) {
                    ImVec2 ms = mf->CalcTextSizeA(mf->FontSize, FLT_MAX, 0, errorMsg_.c_str());
                    dl->AddText(mf, mf->FontSize,
                                ImVec2(formX + (formW - ms.x) * 0.5f, cy - 6),
                                IM_COL32(255, 90, 70, (int)(220 * alpha)), errorMsg_.c_str());
                } else if (!successMsg_.empty()) {
                    ImVec2 ms = mf->CalcTextSizeA(mf->FontSize, FLT_MAX, 0, successMsg_.c_str());
                    dl->AddText(mf, mf->FontSize,
                                ImVec2(formX + (formW - ms.x) * 0.5f, cy - 6),
                                IM_COL32(100, 220, 120, (int)(220 * alpha)), successMsg_.c_str());
                }
            }
            cy += 10;

            // Divider with "or"
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float divY = cy + 2;
            dl->AddLine(ImVec2(formX, divY), ImVec2(formX + formW * 0.38f, divY),
                        IM_COL32(50, 70, 100, 60));
            dl->AddLine(ImVec2(formX + formW * 0.62f, divY), ImVec2(formX + formW, divY),
                        IM_COL32(50, 70, 100, 60));
            const char* orText = u8"\u6216";
            ImFont* orFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
            ImVec2 orSize = orFont->CalcTextSizeA(orFont->FontSize, FLT_MAX, 0, orText);
            dl->AddText(orFont, orFont->FontSize,
                        ImVec2(formX + (formW - orSize.x) * 0.5f, divY - orSize.y * 0.5f),
                        IM_COL32(80, 100, 130, 120), orText);
            cy += 20;

            ImGui::SetCursorScreenPos(ImVec2(formX, cy));
            bool switchReg = RenderButton(u8"\u521B\u5EFA\u65B0\u8D26\u6237",
                                          formX, cy, formW, 46,
                                          IM_COL32(30, 45, 70, 200),
                                          IM_COL32(40, 60, 95, 255), false);
            if (switchReg) {
                showRegister_ = true;
                errorMsg_.clear(); successMsg_.clear();
                cardSlide_ = 20.0f;
            }
            cy += 58;

            // ── OAuth Login Buttons ──
            // Divider
            ImDrawList* dl2 = ImGui::GetWindowDrawList();
            float divY2 = cy;
            dl2->AddLine(ImVec2(formX, divY2), ImVec2(formX + formW * 0.35f, divY2),
                        IM_COL32(50, 70, 100, 60));
            dl2->AddLine(ImVec2(formX + formW * 0.65f, divY2), ImVec2(formX + formW, divY2),
                        IM_COL32(50, 70, 100, 60));
            const char* socialText = u8"\u7B2C\u4E09\u65B9\u767B\u5F55"; // 第三方登录
            ImFont* socialFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
            ImVec2 socialSize = socialFont->CalcTextSizeA(socialFont->FontSize, FLT_MAX, 0, socialText);
            dl2->AddText(socialFont, socialFont->FontSize,
                        ImVec2(formX + (formW - socialSize.x) * 0.5f, divY2 - socialSize.y * 0.5f),
                        IM_COL32(80, 100, 130, 120), socialText);
            cy += 24;

            // OAuth icon buttons
            float iconSize = 48.0f;
            float iconGap = 20.0f;
            float totalIconW = iconSize * 2 + iconGap;
            float iconStartX = formX + (formW - totalIconW) * 0.5f;

            // Steam icon (custom SVG drawing)
            bool steamClicked = RenderIconButton("##steamLogin", iconStartX, cy, iconSize,
                                                  IM_COL32(23, 26, 33, 255),  // Steam dark
                                                  IM_COL32(40, 50, 65, 255),
                                                  "steam");

            // GitHub icon (official icon)
            bool githubClicked = RenderIconButton("##githubLogin", iconStartX + iconSize + iconGap, cy, iconSize,
                                                   IM_COL32(23, 26, 33, 255),  // Same as Steam dark
                                                   IM_COL32(40, 50, 65, 255),
                                                   "github");

            // Draw labels below icons
            ImFont* labelFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
            const char* steamLabel = "Steam";
            const char* githubLabel = "GitHub";
            ImVec2 steamLabelSize = labelFont->CalcTextSizeA(labelFont->FontSize, FLT_MAX, 0, steamLabel);
            ImVec2 githubLabelSize = labelFont->CalcTextSizeA(labelFont->FontSize, FLT_MAX, 0, githubLabel);
            dl2->AddText(labelFont, labelFont->FontSize,
                        ImVec2(iconStartX + (iconSize - steamLabelSize.x) * 0.5f, cy + iconSize + 4),
                        IM_COL32(100, 120, 150, 180), steamLabel);
            dl2->AddText(labelFont, labelFont->FontSize,
                        ImVec2(iconStartX + iconSize + iconGap + (iconSize - githubLabelSize.x) * 0.5f, cy + iconSize + 4),
                        IM_COL32(100, 120, 150, 180), githubLabel);

            // Handle OAuth button clicks
            if (githubClicked) {
                Log(LogLevel::Info, "GitHub button clicked, oauthWaiting_=%d", oauthWaiting_ ? 1 : 0);
            }
            if (steamClicked) {
                Log(LogLevel::Info, "Steam button clicked, oauthWaiting_=%d", oauthWaiting_ ? 1 : 0);
            }

            // Check if OAuth server died unexpectedly and restart it
            if (oauthWaiting_ && !IsOAuthServerRunning()) {
                Log(LogLevel::Warn, "OAuth server died, restarting...");
                StartOAuthCallbackServer();
                if (!IsOAuthServerRunning()) {
                    errorMsg_ = u8"OAuth 服务启动失败，请重试";
                    msgTimer_ = 3.0f;
                    oauthWaiting_ = false;
                    oauthTimeout_ = 0.0f;
                }
            }

            // Reset OAuth state if timeout (120 seconds - Steam login needs more time)
            if (oauthWaiting_) {
                oauthTimeout_ += ImGui::GetIO().DeltaTime;
                if (oauthTimeout_ > 120.0f) {
                    Log(LogLevel::Warn, "OAuth timeout, resetting state");
                    oauthWaiting_ = false;
                    oauthTimeout_ = 0.0f;
                    StopOAuthCallbackServer();
                    errorMsg_ = u8"授权超时，请重试";
                    msgTimer_ = 3.0f;
                }
            }

            // Allow clicking button again to cancel current OAuth and start new one
            if (steamClicked) {
                if (oauthWaiting_) {
                    // Cancel current OAuth
                    Log(LogLevel::Info, "Cancelling current OAuth, starting Steam OAuth...");
                    StopOAuthCallbackServer();
                    oauthWaiting_ = false;
                    oauthTimeout_ = 0.0f;
                }
                Log(LogLevel::Info, "Starting Steam OAuth...");
                StartOAuthCallbackServer();
                // Double-check server is ready before opening browser
                int port = GetOAuthServerPort();
                if (IsOAuthServerRunning() && port > 0) {
                    // Small delay to ensure server is fully ready to accept
                    Sleep(100);
                    oauthWaiting_ = true;
                    oauthTimeout_ = 0.0f;
                    oauthType_ = "steam";
                    std::string url = GetSteamOAuthUrl();
                    Log(LogLevel::Info, "Opening Steam OAuth URL, server on port %d", port);
                    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                } else {
                    Log(LogLevel::Error, "OAuth server not ready: running=%d, port=%d",
                        IsOAuthServerRunning() ? 1 : 0, port);
                    errorMsg_ = u8"OAuth 服务启动失败，请重试";
                    msgTimer_ = 3.0f;
                }
            }

            if (githubClicked) {
                if (oauthWaiting_) {
                    // Cancel current OAuth
                    Log(LogLevel::Info, "Cancelling current OAuth, starting GitHub OAuth...");
                    StopOAuthCallbackServer();
                    oauthWaiting_ = false;
                    oauthTimeout_ = 0.0f;
                }
                Log(LogLevel::Info, "Starting GitHub OAuth...");
                StartOAuthCallbackServer();
                // Double-check server is ready before opening browser
                int port = GetOAuthServerPort();
                if (IsOAuthServerRunning() && port > 0) {
                    // Small delay to ensure server is fully ready to accept
                    Sleep(100);
                    oauthWaiting_ = true;
                    oauthTimeout_ = 0.0f;
                    oauthType_ = "github";
                    std::string url = GetGitHubOAuthUrl();
                    Log(LogLevel::Info, "Opening GitHub OAuth URL, server on port %d: %s", port, url.c_str());
                    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                } else {
                    Log(LogLevel::Error, "OAuth server not ready: running=%d, port=%d",
                        IsOAuthServerRunning() ? 1 : 0, port);
                    errorMsg_ = u8"OAuth 服务启动失败，请重试";
                    msgTimer_ = 3.0f;
                }
            }

            // Show waiting message if OAuth in progress
            if (oauthWaiting_) {
                cy += iconSize + 24;
                int port = GetOAuthServerPort();
                char waitMsg[128];
                snprintf(waitMsg, sizeof(waitMsg), u8"等待授权中... (端口 %d)", port);
                ImVec2 waitSize = labelFont->CalcTextSizeA(labelFont->FontSize, FLT_MAX, 0, waitMsg);
                // Pulsing animation
                float pulse = 0.5f + 0.5f * sinf(t * 3.0f);
                dl2->AddText(labelFont, labelFont->FontSize,
                            ImVec2(formX + (formW - waitSize.x) * 0.5f, cy),
                            IM_COL32(26, 159, 255, (int)(180 * pulse)), waitMsg);
            }
        }

        // (Error/success messages are now rendered inline below buttons)
    }
    ImGui::End();

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(1);

    // Footer
    const char* foot = u8"SteamForge v1.0.0  \u00B7  \u661F\u94F8";
    ImFont* footFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
    ImVec2 fs = footFont->CalcTextSizeA(footFont->FontSize, FLT_MAX, 0, foot);
    bg->AddText(footFont, footFont->FontSize,
                ImVec2(winW * 0.5f - fs.x * 0.5f, winH - 28),
                IM_COL32(60, 80, 110, 100), foot);
}

} // namespace sf
