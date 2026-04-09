#include "ui/panels/library.h"
#include "ui/iconfonts.h"
#include "ui/icons.h"
#include "core/texture_manager.h"
#include "core/steam_store.h"
#include "core/steam_library.h"
#include "core/video_player.h"
#include "core/install_record.h"
#include "core/navigation.h"
#include "core/game_process.h"
#include "core/auth.h"
#include "app/application.h"
#include "app/config.h"
#include "ui/toast.h"
#include "utils/logger.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <functional>
#include <vector>
#include <unordered_set>
#include <windows.h>
#include <shellapi.h>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

namespace sf {

// ════════════════════════════════════════════════════════════════════════════
//  SVG-STYLE VECTOR ICONS - High quality scalable icons drawn with ImDrawList
// ════════════════════════════════════════════════════════════════════════════

// Draw a modern gear/settings icon (SVG style)
static void DrawIconGear(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float r1 = size * 0.5f;   // outer radius
    float r2 = size * 0.32f;  // inner radius
    float r3 = size * 0.18f;  // center hole
    int teeth = 8;

    // Draw gear teeth
    for (int i = 0; i < teeth; i++) {
        float a1 = (float)i / teeth * 2.0f * 3.14159f;
        float a2 = ((float)i + 0.35f) / teeth * 2.0f * 3.14159f;
        float a3 = ((float)i + 0.65f) / teeth * 2.0f * 3.14159f;
        float a4 = ((float)i + 1.0f) / teeth * 2.0f * 3.14159f;

        ImVec2 p1(cx + cosf(a1) * r2, cy + sinf(a1) * r2);
        ImVec2 p2(cx + cosf(a2) * r1, cy + sinf(a2) * r1);
        ImVec2 p3(cx + cosf(a3) * r1, cy + sinf(a3) * r1);
        ImVec2 p4(cx + cosf(a4) * r2, cy + sinf(a4) * r2);

        dl->AddLine(p1, p2, color, thickness);
        dl->AddLine(p2, p3, color, thickness);
        dl->AddLine(p3, p4, color, thickness);
    }
    // Center circle
    dl->AddCircle(ImVec2(cx, cy), r3, color, 12, thickness);
}

// Draw a modern store/shopping bag icon
static void DrawIconStore(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float w = size * 0.6f;
    float h = size * 0.7f;
    float handleH = size * 0.25f;

    // Bag body
    dl->AddRect(ImVec2(cx - w/2, cy - h/2 + handleH), ImVec2(cx + w/2, cy + h/2),
                color, size * 0.1f, 0, thickness);
    // Handle
    dl->AddBezierQuadratic(
        ImVec2(cx - w * 0.3f, cy - h/2 + handleH),
        ImVec2(cx, cy - h/2 - handleH * 0.3f),
        ImVec2(cx + w * 0.3f, cy - h/2 + handleH),
        color, thickness);
}

// Draw a modern play icon (rounded triangle)
static void DrawIconPlay(ImDrawList* dl, float cx, float cy, float size, ImU32 color, bool filled = true) {
    float s = size * 0.5f;
    ImVec2 p1(cx - s * 0.4f, cy - s * 0.6f);
    ImVec2 p2(cx - s * 0.4f, cy + s * 0.6f);
    ImVec2 p3(cx + s * 0.6f, cy);

    if (filled) {
        dl->AddTriangleFilled(p1, p2, p3, color);
    } else {
        dl->AddTriangle(p1, p2, p3, color, 2.0f);
    }
}

// Draw a modern download icon
static void DrawIconDownload(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float s = size * 0.5f;
    // Arrow shaft
    dl->AddLine(ImVec2(cx, cy - s * 0.7f), ImVec2(cx, cy + s * 0.2f), color, thickness);
    // Arrow head
    dl->AddLine(ImVec2(cx - s * 0.4f, cy - s * 0.1f), ImVec2(cx, cy + s * 0.4f), color, thickness);
    dl->AddLine(ImVec2(cx + s * 0.4f, cy - s * 0.1f), ImVec2(cx, cy + s * 0.4f), color, thickness);
    // Base line
    dl->AddLine(ImVec2(cx - s * 0.5f, cy + s * 0.6f), ImVec2(cx + s * 0.5f, cy + s * 0.6f), color, thickness);
}

// Draw a modern trophy/achievement icon
static void DrawIconTrophy(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float s = size * 0.5f;
    // Cup body
    dl->AddBezierQuadratic(
        ImVec2(cx - s * 0.5f, cy - s * 0.5f),
        ImVec2(cx - s * 0.6f, cy + s * 0.1f),
        ImVec2(cx - s * 0.2f, cy + s * 0.3f),
        color, thickness);
    dl->AddBezierQuadratic(
        ImVec2(cx + s * 0.5f, cy - s * 0.5f),
        ImVec2(cx + s * 0.6f, cy + s * 0.1f),
        ImVec2(cx + s * 0.2f, cy + s * 0.3f),
        color, thickness);
    // Top rim
    dl->AddLine(ImVec2(cx - s * 0.5f, cy - s * 0.5f), ImVec2(cx + s * 0.5f, cy - s * 0.5f), color, thickness);
    // Bottom
    dl->AddLine(ImVec2(cx - s * 0.2f, cy + s * 0.3f), ImVec2(cx + s * 0.2f, cy + s * 0.3f), color, thickness);
    // Stem
    dl->AddLine(ImVec2(cx, cy + s * 0.3f), ImVec2(cx, cy + s * 0.55f), color, thickness);
    // Base
    dl->AddLine(ImVec2(cx - s * 0.3f, cy + s * 0.55f), ImVec2(cx + s * 0.3f, cy + s * 0.55f), color, thickness);
}

// Draw a modern news/article icon
static void DrawIconNews(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float s = size * 0.5f;
    // Paper outline
    dl->AddRect(ImVec2(cx - s * 0.5f, cy - s * 0.6f), ImVec2(cx + s * 0.5f, cy + s * 0.6f),
                color, size * 0.08f, 0, thickness);
    // Text lines
    dl->AddLine(ImVec2(cx - s * 0.3f, cy - s * 0.35f), ImVec2(cx + s * 0.3f, cy - s * 0.35f), color, thickness * 0.8f);
    dl->AddLine(ImVec2(cx - s * 0.3f, cy - s * 0.1f), ImVec2(cx + s * 0.3f, cy - s * 0.1f), color, thickness * 0.8f);
    dl->AddLine(ImVec2(cx - s * 0.3f, cy + s * 0.15f), ImVec2(cx + s * 0.1f, cy + s * 0.15f), color, thickness * 0.8f);
}

// Draw a modern community/users icon
static void DrawIconCommunity(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float s = size * 0.5f;
    // Main person (center)
    dl->AddCircle(ImVec2(cx, cy - s * 0.35f), s * 0.25f, color, 12, thickness);
    dl->AddBezierQuadratic(
        ImVec2(cx - s * 0.35f, cy + s * 0.5f),
        ImVec2(cx, cy + s * 0.1f),
        ImVec2(cx + s * 0.35f, cy + s * 0.5f),
        color, thickness);
    // Left person (smaller)
    dl->AddCircle(ImVec2(cx - s * 0.55f, cy - s * 0.2f), s * 0.18f, color, 10, thickness * 0.8f);
    // Right person (smaller)
    dl->AddCircle(ImVec2(cx + s * 0.55f, cy - s * 0.2f), s * 0.18f, color, 10, thickness * 0.8f);
}

// Draw a modern live/broadcast icon
static void DrawIconLive(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float s = size * 0.5f;
    // Center dot
    dl->AddCircleFilled(ImVec2(cx, cy), s * 0.15f, color);
    // Signal waves
    for (int i = 1; i <= 3; i++) {
        float r = s * 0.25f * i;
        float startAngle = -0.7f;
        float endAngle = 0.7f;
        int segments = 12;
        for (int j = 0; j < segments; j++) {
            float a1 = startAngle + (endAngle - startAngle) * j / segments;
            float a2 = startAngle + (endAngle - startAngle) * (j + 1) / segments;
            dl->AddLine(
                ImVec2(cx + cosf(a1) * r, cy + sinf(a1) * r),
                ImVec2(cx + cosf(a2) * r, cy + sinf(a2) * r),
                IM_COL32((color & 0xFF), ((color >> 8) & 0xFF), ((color >> 16) & 0xFF), 255 - i * 50),
                thickness * (1.0f - i * 0.2f));
        }
    }
}

// Draw a modern cloud icon (SVG style) - beautiful fluffy cloud
static void DrawIconCloud(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f, bool filled = false) {
    float s = size * 0.5f;
    const float PI = 3.14159265f;

    // Cloud shape using bezier curves for smooth, fluffy appearance
    // Build path points for a nice cloud silhouette
    std::vector<ImVec2> cloudPath;

    // Bottom flat base
    float baseY = cy + s * 0.35f;
    float leftX = cx - s * 0.7f;
    float rightX = cx + s * 0.75f;

    // Start from bottom left, go clockwise
    cloudPath.push_back(ImVec2(leftX, baseY));

    // Left small bump
    float lb_cx = cx - s * 0.5f, lb_cy = cy + s * 0.1f, lb_r = s * 0.32f;
    for (int i = 0; i <= 8; i++) {
        float a = PI * 0.7f + PI * 0.8f * i / 8.0f;
        cloudPath.push_back(ImVec2(lb_cx + cosf(a) * lb_r, lb_cy + sinf(a) * lb_r));
    }

    // Main center bump (largest, tallest)
    float mc_cx = cx - s * 0.05f, mc_cy = cy - s * 0.15f, mc_r = s * 0.5f;
    for (int i = 0; i <= 10; i++) {
        float a = PI * 0.85f + PI * 0.8f * i / 10.0f;
        cloudPath.push_back(ImVec2(mc_cx + cosf(a) * mc_r, mc_cy + sinf(a) * mc_r));
    }

    // Right bump
    float rb_cx = cx + s * 0.45f, rb_cy = cy + s * 0.05f, rb_r = s * 0.38f;
    for (int i = 0; i <= 8; i++) {
        float a = PI * 0.9f + PI * 0.75f * i / 8.0f;
        cloudPath.push_back(ImVec2(rb_cx + cosf(a) * rb_r, rb_cy + sinf(a) * rb_r));
    }

    // Close to bottom right
    cloudPath.push_back(ImVec2(rightX, baseY));

    if (filled) {
        // Draw filled cloud
        dl->AddConvexPolyFilled(cloudPath.data(), (int)cloudPath.size(), color);
    } else {
        // Draw cloud outline
        for (size_t i = 0; i < cloudPath.size() - 1; i++) {
            dl->AddLine(cloudPath[i], cloudPath[i + 1], color, thickness);
        }
        // Bottom line
        dl->AddLine(ImVec2(leftX, baseY), ImVec2(rightX, baseY), color, thickness);
    }
}

// ============================================================
//  Procedural cover art fallback (when image not yet loaded)
// ============================================================

struct CoverColors {
    ImU32 bg1, bg2, accent, highlight;
    int pattern;
};

static CoverColors GenCoverColors(const std::string& id) {
    unsigned h = 0;
    for (char c : id) h = h * 2654435761u + c;

    float hue = (h % 360) / 360.0f;
    float sat = 0.5f + (((h >> 12) % 30) / 100.0f);
    float lit1 = 0.20f + (((h >> 20) % 15) / 100.0f);
    float lit2 = 0.10f + (((h >> 8) % 10) / 100.0f);

    auto hsl2rgb = [](float hh, float ss, float ll) -> ImU32 {
        float c = (1.0f - fabsf(2.0f * ll - 1.0f)) * ss;
        float x = c * (1.0f - fabsf(fmodf(hh * 6.0f, 2.0f) - 1.0f));
        float m = ll - c / 2.0f;
        float r, g, b;
        int sector = (int)(hh * 6.0f) % 6;
        switch (sector) {
            case 0: r=c;g=x;b=0; break; case 1: r=x;g=c;b=0; break;
            case 2: r=0;g=c;b=x; break; case 3: r=0;g=x;b=c; break;
            case 4: r=x;g=0;b=c; break; default:r=c;g=0;b=x; break;
        }
        return IM_COL32((int)((r+m)*255),(int)((g+m)*255),(int)((b+m)*255),255);
    };

    CoverColors cc;
    cc.bg1 = hsl2rgb(hue, sat, lit1);
    cc.bg2 = hsl2rgb(fmodf(hue + 0.08f, 1.0f), sat * 0.8f, lit2);
    cc.accent = hsl2rgb(fmodf(hue + 0.5f, 1.0f), 0.7f, 0.55f);
    cc.highlight = hsl2rgb(hue, 0.3f, 0.6f);
    cc.pattern = h % 6;
    return cc;
}

static void DrawProceduralCover(ImDrawList* dl, float x, float y, float w, float h,
                                 const std::string& id) {
    auto cc = GenCoverColors(id);
    dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), cc.bg1, cc.bg2, cc.bg1, cc.bg2);

    switch (cc.pattern) {
        case 0: { ImVec2 pts[4] = {{x+w*0.3f,y},{x+w*0.6f,y},{x+w*0.2f,y+h},{x-w*0.1f,y+h}};
                  dl->AddConvexPolyFilled(pts, 4, IM_COL32(255,255,255,12)); break; }
        case 1: for (int i=0;i<5;i++) { float by=y+h*(0.15f+i*0.18f);
                  dl->AddRectFilled(ImVec2(x,by),ImVec2(x+w,by+h*0.04f),IM_COL32(255,255,255,6+i*2)); } break;
        case 2: { float cx2=x+w*0.7f,cy2=y+h*0.3f;
                  for(int i=8;i>=0;i--) dl->AddCircleFilled(ImVec2(cx2,cy2),w*(0.3f+i*0.08f),IM_COL32(255,255,255,3),32); break; }
        case 3: for(int i=-4;i<8;i++) dl->AddLine(ImVec2(x+i*w*0.2f,y),ImVec2(x+i*w*0.2f+w*0.15f,y+h),IM_COL32(255,255,255,8)); break;
        case 4: for(int i=6;i>=0;i--) { float ry=y+h-h*(0.05f+i*0.07f);
                  dl->AddRectFilledMultiColor(ImVec2(x,ry),ImVec2(x+w,ry+h*0.02f),IM_COL32(255,255,255,0),IM_COL32(255,255,255,5),IM_COL32(255,255,255,5),IM_COL32(255,255,255,0)); } break;
        default: { float sz=w*0.6f; dl->AddRectFilledMultiColor(ImVec2(x,y),ImVec2(x+sz,y+sz),cc.accent&0x18FFFFFF,IM_COL32(0,0,0,0),IM_COL32(0,0,0,0),IM_COL32(0,0,0,0)); break; }
    }
    dl->AddRectFilledMultiColor(ImVec2(x,y),ImVec2(x+w,y+h*0.15f),IM_COL32(0,0,0,50),IM_COL32(0,0,0,50),IM_COL32(0,0,0,0),IM_COL32(0,0,0,0));
    dl->AddRectFilledMultiColor(ImVec2(x,y+h*0.5f),ImVec2(x+w,y+h),IM_COL32(0,0,0,0),IM_COL32(0,0,0,0),IM_COL32(0,0,0,160),IM_COL32(0,0,0,160));
    dl->AddLine(ImVec2(x,y),ImVec2(x+w,y),IM_COL32(255,255,255,15));
}

// ============================================================
//  GetBestTexture — try API header URL first, then CDN pattern
// ============================================================
static ImTextureID GetBestTexture(const std::string& appId) {
    // Try the direct header_image URL from the API first
    std::string hdrKey = "hdr_" + appId;
    ImTextureID tex = GetTextureByKey(hdrKey);
    if (tex) return tex;

    // If URL download failed, immediately fall back to CDN texture
    // This prevents waiting for a texture that will never arrive
    if (IsUrlTextureFailed(hdrKey)) {
        return GetGameTexture(appId);
    }

    // URL download still in progress, also try CDN texture
    // (CDN download runs in parallel and may complete first)
    tex = GetGameTexture(appId);
    if (tex) return tex;

    return (ImTextureID)0;
}

// ============================================================
//  DrawCoverArt — real texture with procedural fallback
// ============================================================

static void DrawCoverArt(ImDrawList* dl, float x, float y, float w, float h,
                          const std::string& appId, const char* title, float rounding = 4.0f) {
    ImTextureID tex = GetBestTexture(appId);

    if (tex) {
        dl->AddImageRounded(tex, ImVec2(x, y), ImVec2(x + w, y + h),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), rounding);
    } else {
        DrawProceduralCover(dl, x, y, w, h, appId);
    }

    if (title && title[0]) {
        dl->AddRectFilledMultiColor(ImVec2(x, y + h * 0.55f), ImVec2(x + w, y + h),
            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 200), IM_COL32(0, 0, 0, 200));

        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        float maxW = w - 12;
        dl->AddText(font, font->FontSize, ImVec2(x + 7, y + h - font->FontSize - 5),
                    IM_COL32(0, 0, 0, 200), title, nullptr, maxW);
        dl->AddText(font, font->FontSize, ImVec2(x + 6, y + h - font->FontSize - 6),
                    IM_COL32(255, 255, 255, 245), title, nullptr, maxW);
    }

    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(255, 255, 255, 15), rounding);
}

static void DrawShadow(ImDrawList* dl, float x, float y, float w, float h, float spread = 8.0f) {
    for (int i = (int)spread; i > 0; i -= 2) {
        float a = 8.0f * ((spread - i) / spread);
        dl->AddRectFilled(ImVec2(x - i, y - i), ImVec2(x + w + i, y + h + i),
                          IM_COL32(0, 0, 0, (int)a), 6.0f);
    }
}

// Helper: invisible button over a card area, returns hover+click state
static bool CardButton(const char* id, float x, float y, float w, float h,
                       bool& hovered, ImDrawList* dl) {
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::PushID(id);
    ImGui::InvisibleButton("##card", ImVec2(w, h));
    hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImGui::PopID();

    if (hovered) {
        // Subtle glow on hover
        dl->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1),
                    IM_COL32(102, 192, 244, 80), 5.0f, 0, 2.0f);
    }
    return clicked;
}

// ====================== LibraryContent ======================

LibraryContent::LibraryContent() {
    // 新闻数据将从游戏的 Steam API 动态加载
}

// 格式化时间戳为相对日期
static std::string FormatNewsDate(long long timestamp) {
    if (timestamp == 0) return "";

    time_t now = time(nullptr);
    long long diff = now - timestamp;

    if (diff < 86400) return u8"今天";
    if (diff < 86400 * 2) return u8"昨天";
    if (diff < 86400 * 7) {
        char buf[32];
        snprintf(buf, sizeof(buf), u8"%lld天前", diff / 86400);
        return buf;
    }
    if (diff < 86400 * 30) {
        char buf[32];
        snprintf(buf, sizeof(buf), u8"%lld周前", diff / (86400 * 7));
        return buf;
    }
    if (diff < 86400 * 365) {
        char buf[32];
        snprintf(buf, sizeof(buf), u8"%lld月前", diff / (86400 * 30));
        return buf;
    }

    // 超过一年，显示具体日期
    struct tm* t = localtime(&timestamp);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d/%d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    return buf;
}

void LibraryContent::UpdateNewsFromGames(const std::vector<GameInfo>& games) {
    // 请求前几个游戏的数据（限制数量避免过多请求）
    int maxRequest = std::min(10, (int)games.size());
    for (int i = 0; i < maxRequest; i++) {
        const auto& game = games[i];
        // 检查是否已请求过
        bool alreadyRequested = false;
        for (const auto& id : requestedAppIds_) {
            if (id == game.appId) {
                alreadyRequested = true;
                break;
            }
        }
        if (!alreadyRequested) {
            RequestStoreData(game.appId);
            requestedAppIds_.push_back(game.appId);
        }
    }

    // 收集每个游戏的最新一条新闻（不重复）
    std::vector<NewsItem> allNews;
    for (const auto& game : games) {
        const SteamStoreData* data = GetStoreData(game.appId);
        if (!data || !data->loaded) continue;
        if (data->news.empty()) continue;

        // 只取第一条（最新的）新闻
        const auto& newsItem = data->news[0];
        NewsItem item;
        item.title = newsItem.title;
        item.game = game.name;
        item.gameAppId = game.appId;
        item.date = FormatNewsDate(newsItem.date);
        item.thumbnailUrl = newsItem.thumbnailUrl;

        // 根据 appId 生成颜色
        unsigned h = 0;
        for (char c : game.appId) h = h * 2654435761u + c;
        float hue = (h % 360) / 360.0f;
        int r = (int)(128 + 64 * sin(hue * 6.28f));
        int g = (int)(128 + 64 * sin((hue + 0.33f) * 6.28f));
        int b = (int)(128 + 64 * sin((hue + 0.66f) * 6.28f));
        item.coverColor = IM_COL32(r, g, b, 255);

        allNews.push_back(std::move(item));
    }

    // 限制新闻数量（最多30条）
    if (allNews.size() > 30) {
        allNews.resize(30);
    }

    news_ = std::move(allNews);
}

void LibraryContent::RenderNewsSection(ImDrawList* dl, float x, float y, float width, float& outHeight, const std::vector<GameInfo>& games) {
    // 更新新闻数据
    UpdateNewsFromGames(games);

    float headerH = 30.0f;
    float cardH = 220.0f;
    float gap = 10.0f;

    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 230),
                    u8"\u6700\u65B0\u52A8\u6001"); // 最新动态
    else
        dl->AddText(ImVec2(x, y), IM_COL32(255, 255, 255, 230),
                    u8"\u6700\u65B0\u52A8\u6001");

    // Navigation arrows with click
    if (g_iconFont) {
        float arrowY = y + 2;
        ImGui::SetCursorScreenPos(ImVec2(x + width - 50, arrowY));
        ImGui::PushID("news_left");
        ImGui::InvisibleButton("##nl", ImVec2(22, 22));
        bool nlHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && newsScroll_ > 0) newsScroll_--;
        ImGui::PopID();
        DrawIcon(dl, icon::CHEVLEFT, ImVec2(x + width - 48, arrowY), IM_COL32(150, 165, 180, nlHov ? 255 : 180));

        ImGui::SetCursorScreenPos(ImVec2(x + width - 24, arrowY));
        ImGui::PushID("news_right");
        ImGui::InvisibleButton("##nr", ImVec2(22, 22));
        bool nrHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && newsScroll_ < (int)news_.size() - 4) newsScroll_++;
        ImGui::PopID();
        DrawIcon(dl, icon::CHEVRIGHT, ImVec2(x + width - 22, arrowY), IM_COL32(150, 165, 180, nrHov ? 255 : 180));
    }

    y += headerH;
    float cardW = (width - 30) / 4.0f;

    // 如果没有新闻，显示加载提示
    if (news_.empty()) {
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        const char* loadingText = u8"正在加载游戏动态...";
        if (games.empty()) {
            loadingText = u8"暂无游戏动态";
        }
        dl->AddText(font, font->FontSize, ImVec2(x + 20, y + cardH / 2 - 10),
                    IM_COL32(120, 140, 160, 180), loadingText);
        outHeight = headerH + cardH;
        return;
    }

    for (int i = 0; i < std::min(4, (int)news_.size()); i++) {
        float cx = x + i * (cardW + gap);
        int idx = (i + newsScroll_) % (int)news_.size();
        auto& item = news_[idx];

        // Clickable card
        bool cardHov = false;
        CardButton(item.title.c_str(), cx, y, cardW, cardH, cardHov, dl);

        DrawShadow(dl, cx, y, cardW, cardH, 6.0f);
        dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cardW, y + cardH),
                          IM_COL32(30, 40, 54, 255), 6.0f);

        // Cover — taller ratio
        float coverH = cardH * 0.6f;
        DrawCoverArt(dl, cx, y, cardW, coverH, item.gameAppId, nullptr, 6.0f);

        // Date badge
        ImVec2 dateSize = ImGui::CalcTextSize(item.date.c_str());
        dl->AddRectFilled(ImVec2(cx + 6, y + 6), ImVec2(cx + 6 + dateSize.x + 12, y + 22),
                          IM_COL32(0, 0, 0, 160), 3.0f);
        dl->AddText(ImVec2(cx + 12, y + 8), IM_COL32(255, 255, 255, 220), item.date.c_str());

        // Title - truncate if too long
        float titleY = y + coverH + 8;
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        float titleMaxW = cardW - 16;

        // Truncate title if too long
        std::string displayTitle = item.title;
        ImVec2 titleSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, displayTitle.c_str());
        if (titleSize.x > titleMaxW) {
            while (!displayTitle.empty() && titleSize.x > titleMaxW - 15) {
                size_t len = displayTitle.size();
                while (len > 0 && (displayTitle[len-1] & 0xC0) == 0x80) len--;
                if (len > 0) len--;
                displayTitle = displayTitle.substr(0, len);
                titleSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, displayTitle.c_str());
            }
            displayTitle += "...";
        }
        dl->AddText(font, font->FontSize, ImVec2(cx + 8, titleY),
                    IM_COL32(230, 240, 250, 240), displayTitle.c_str());

        // Game name
        float gameY = titleY + 24;
        if (g_iconFont) DrawIcon(dl, icon::GAMEPAD, ImVec2(cx + 8, gameY), IM_COL32(102, 192, 244, 180));
        if (g_mainFontSmall)
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                        ImVec2(cx + 26, gameY + 1), IM_COL32(102, 192, 244, 200), item.game.c_str());
        else
            dl->AddText(ImVec2(cx + 26, gameY + 1), IM_COL32(102, 192, 244, 200), item.game.c_str());

        dl->AddRectFilled(ImVec2(cx, y + cardH - 2), ImVec2(cx + cardW, y + cardH),
                          IM_COL32(102, 192, 244, 40));
    }

    outHeight = headerH + cardH;
}

void LibraryContent::RenderRecentGames(ImDrawList* dl, float x, float y, float width, float& outHeight, const std::vector<GameInfo>& games) {
    float headerH = 30.0f;

    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 230),
                    u8"\u6700\u8FD1\u6E38\u620F"); // 最近游戏
    else
        dl->AddText(ImVec2(x, y), IM_COL32(255, 255, 255, 230),
                    u8"\u6700\u8FD1\u6E38\u620F");

    if (g_iconFont) {
        DrawIcon(dl, icon::CHEVLEFT, ImVec2(x + width - 44, y + 2), IM_COL32(150, 165, 180, 180));
        DrawIcon(dl, icon::CHEVRIGHT, ImVec2(x + width - 20, y + 2), IM_COL32(150, 165, 180, 180));
    }

    y += headerH;
    // Use 16:9 ratio for nicer proportions (not too flat)
    float cardW = (width - 40) / 5.0f;
    float cardH = cardW * 0.5625f; // 16:9
    float gap = 10.0f;

    // 获取最近玩过的游戏（通过 SteamForge 启动的）
    std::vector<std::string> recentAppIds = InstallRecord::Get().GetRecentlyPlayed(5);

    // 构建最近游戏列表，结合 Steam 数据
    std::vector<GameInfo> recentGames;
    for (const auto& appId : recentAppIds) {
        // 在 games 列表中查找对应的游戏信息
        bool found = false;
        for (const auto& game : games) {
            if (game.appId == appId) {
                recentGames.push_back(game);
                found = true;
                break;
            }
        }
        // 如果游戏不在 Steam 库中，创建一个临时的 GameInfo
        if (!found) {
            GameInfo info;
            info.appId = appId;
            info.name = "Game " + appId;  // 临时名称，封面会通过 appId 加载
            recentGames.push_back(info);
        }
    }

    // 如果没有最近玩过的游戏，显示提示
    if (recentGames.empty()) {
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        dl->AddText(font, font->FontSize, ImVec2(x + 20, y + cardH / 2 - 10),
                    IM_COL32(120, 140, 160, 180), u8"暂无最近游玩记录");
        outHeight = headerH + cardH;
        return;
    }

    int showCount = std::min(5, (int)recentGames.size());
    for (int i = 0; i < showCount; i++) {
        float cx = x + i * (cardW + gap);
        auto& game = recentGames[i];

        bool cardHov = false;
        char btnId[32]; snprintf(btnId, sizeof(btnId), "recent_%d", i);
        CardButton(btnId, cx, y, cardW, cardH, cardHov, dl);

        DrawShadow(dl, cx, y, cardW, cardH, 6.0f);
        DrawCoverArt(dl, cx, y, cardW, cardH, game.appId, game.name.c_str());

        if (game.status == "playing") {
            dl->AddCircleFilled(ImVec2(cx + cardW - 10, y + 10), 5.0f, IM_COL32(144, 186, 60, 255));
            dl->AddCircle(ImVec2(cx + cardW - 10, y + 10), 7.0f, IM_COL32(144, 186, 60, 60), 0, 2.0f);
        }
    }

    outHeight = headerH + cardH;
}

void LibraryContent::RenderAllGames(float x, float y, float width, float& outHeight, const std::vector<GameInfo>& games) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    Log(LogLevel::Debug, "RenderAllGames: x=%.0f, y=%.0f, w=%.0f, games=%d", x, y, width, (int)games.size());

    char header[64];
    snprintf(header, sizeof(header), u8"\u5168\u90E8\u6E38\u620F (%d)", (int)games.size()); // 全部游戏
    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 230), header);
    else
        dl->AddText(ImVec2(x, y), IM_COL32(255, 255, 255, 230), header);

    if (g_mainFontSmall)
        dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                    ImVec2(x + width - 80, y + 4), IM_COL32(120, 140, 160, 160),
                    u8"\u6392\u5E8F: A-Z"); // 排序: A-Z

    float headerH = 32;
    y += headerH;

    // 16:9 proportions for game cards
    float cardW = 200.0f;
    float cardH = cardW * 0.5625f; // 16:9 = 112.5px tall
    float gap = 12.0f;
    int cols = std::max(1, (int)((width + gap) / (cardW + gap)));
    int rows = ((int)games.size() + cols - 1) / cols;

    for (int i = 0; i < (int)games.size(); i++) {
        int row = i / cols;
        int col = i % cols;
        float cx = x + col * (cardW + gap);
        float cy = y + row * (cardH + gap + 4);

        bool cardHov = false;
        char btnId[32]; snprintf(btnId, sizeof(btnId), "all_%d", i);
        CardButton(btnId, cx, cy, cardW, cardH, cardHov, dl);

        DrawShadow(dl, cx, cy, cardW, cardH, 4.0f);
        DrawCoverArt(dl, cx, cy, cardW, cardH, games[i].appId, games[i].name.c_str());
    }

    // Calculate actual height: last card bottom = (rows-1) * rowStride + cardH
    float rowStride = cardH + gap + 4;  // same as used in positioning
    if (rows > 0) {
        outHeight = headerH + (rows - 1) * rowStride + cardH;
    } else {
        outHeight = headerH;
    }
}

void LibraryContent::Render(float x, float y, float width, float height, const std::vector<GameInfo>& games, const GameInfo* selectedGame) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(27, 36, 48, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##LibContent", nullptr, flags)) {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::BeginChild("##libScroll", ImVec2(width, height), false);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();

        // If a game is selected, show game detail view
        if (selectedGame) {
            // Reset texture request flag and media selection when game changes
            if (lastSelectedAppId_ != selectedGame->appId) {
                lastSelectedAppId_ = selectedGame->appId;
                detailTexturesRequested_ = false;
                selectedMediaIdx_ = 0;
                if (isPlayingVideo_) {
                    StopVideo();
                    isPlayingVideo_ = false;
                    playingVideoIdx_ = -1;
                }
                autoSlideTimer_ = 0.0f;
            }
            RenderGameDetail(dl, origin.x, origin.y, width, height, selectedGame);
        } else {
            // Show default library home view
            float padding = 20.0f;
            float cx = origin.x + 24;
            float cy = origin.y + padding;
            float cw = width - 48;

            // Calculate total content height first
            float totalContentH = padding;  // top padding

            // News section height
            float newsH = 30.0f + 220.0f;  // headerH + cardH
            // Recent games height
            float cardW = (cw - 40) / 5.0f;
            float recentH = 30.0f + cardW * 0.5625f;  // headerH + cardH
            // All games height
            float allCardW = 200.0f;
            float allCardH = allCardW * 0.5625f;
            float gap = 12.0f;
            int cols = std::max(1, (int)((cw + gap) / (allCardW + gap)));
            int rows = ((int)games.size() + cols - 1) / cols;
            float rowStride = allCardH + gap + 4;
            float allGamesH = 32.0f + (rows > 0 ? (rows - 1) * rowStride + allCardH : 0);

            if (!games.empty()) {
                totalContentH += newsH + 16 + recentH + 16;  // sections + separators
            }
            totalContentH += allGamesH + padding;  // all games + bottom padding

            // Now render
            if (!games.empty()) {
                float outH = 0;
                RenderNewsSection(dl, cx, cy, cw, outH, games);
                cy += outH;

                cy += 8;
                dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + 1),
                    IM_COL32(80, 100, 120, 0), IM_COL32(80, 100, 120, 60),
                    IM_COL32(80, 100, 120, 60), IM_COL32(80, 100, 120, 0));
                cy += 8;

                RenderRecentGames(dl, cx, cy, cw, outH, games);
                cy += outH;

                cy += 8;
                dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + 1),
                    IM_COL32(80, 100, 120, 0), IM_COL32(80, 100, 120, 60),
                    IM_COL32(80, 100, 120, 60), IM_COL32(80, 100, 120, 0));
                cy += 8;
            }

            float outH = 0;
            RenderAllGames(cx, cy, cw, outH, games);

            // Set content size for scrolling
            ImGui::SetCursorPosY(totalContentH);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// Helper: format playtime hours
static std::string FormatPlaytime(float hours) {
    if (hours < 0.1f) return u8"从未游玩";
    if (hours < 1.0f) {
        int mins = (int)(hours * 60);
        char buf[32];
        snprintf(buf, sizeof(buf), u8"%d 分钟", mins);
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), u8"%.1f 小时", hours);
    return buf;
}

void LibraryContent::RenderGameDetail(ImDrawList* dl, float x, float y, float width, float height, const GameInfo* game) {
    if (!game) return;

    // Request store data for this game
    RequestStoreData(game->appId);
    const SteamStoreData* sd = GetStoreData(game->appId);

    // Request textures when data arrives
    if (sd && !detailTexturesRequested_) {
        detailTexturesRequested_ = true;
        if (!sd->headerImage.empty())
            RequestTextureFromUrl("hdr_" + game->appId, sd->headerImage);
        for (int i = 0; i < (int)sd->screenshotUrls.size() && i < 10; i++) {
            std::string key = "ss_" + game->appId + "_" + std::to_string(i);
            RequestTextureFromUrl(key, sd->screenshotUrls[i]);
        }
        // Request ALL achievement icons (not just first 12)
        for (int i = 0; i < (int)sd->achievements.size(); i++) {
            if (!sd->achievements[i].iconUrl.empty())
                RequestTextureFromUrl("ach_" + game->appId + "_" + std::to_string(i), sd->achievements[i].iconUrl);
        }
        // Request movie thumbnails
        for (int i = 0; i < (int)sd->movies.size() && i < 5; i++) {
            if (!sd->movies[i].thumbnailUrl.empty())
                RequestTextureFromUrl("mov_" + game->appId + "_" + std::to_string(i), sd->movies[i].thumbnailUrl);
        }
        fprintf(stderr, "[LibraryContent] Game %s: screenshots=%d, movies=%d\n",
                game->appId.c_str(), (int)sd->screenshotUrls.size(), (int)sd->movies.size());
    }

    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
    ImFont* mainFont = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : mainFont;

    // Responsive scaling based on window height (base height = 720px)
    float scale = std::max(0.8f, std::min(1.5f, height / 720.0f));
    const float pad = 24.0f * scale;
    const float cardRound = 12.0f;

    // ═══════════════════════════════════════════════════════════════════════
    //  GLASSMORPHISM HELPER - Draw frosted glass card
    // ═══════════════════════════════════════════════════════════════════════
    auto DrawGlassCard = [&](float cx, float cy, float cw, float ch, float alpha = 0.12f) {
        // Glass background with subtle gradient
        ImU32 bgTop = IM_COL32(255, 255, 255, (int)(alpha * 255));
        ImU32 bgBot = IM_COL32(255, 255, 255, (int)(alpha * 0.6f * 255));
        dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + ch), bgTop, bgTop, bgBot, bgBot);
        // Inner highlight (top edge glow)
        dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + 1),
            IM_COL32(255, 255, 255, 60), IM_COL32(255, 255, 255, 60),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
        // Border
        dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cw, cy + ch), IM_COL32(255, 255, 255, 25), cardRound, 0, 1.0f);
    };

    // ═══════════════════════════════════════════════════════════════════════
    //  HERO BANNER - Immersive full-width header with blur overlay
    // ═══════════════════════════════════════════════════════════════════════
    float bannerH = std::max(200.0f, std::min(400.0f, height * 0.4f));  // 40% of height, clamped
    ImTextureID tex = GetTextureByKey("hdr_" + game->appId);
    if (!tex) tex = GetGameTexture(game->appId);

    // Banner background image
    if (tex) {
        dl->AddImage(tex, ImVec2(x, y), ImVec2(x + width, y + bannerH),
                     ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
    } else {
        DrawProceduralCover(dl, x, y, width, bannerH, game->appId);
    }

    // Cinematic gradient overlays (multi-layer for depth)
    dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + width, y + 60),
        IM_COL32(15, 22, 32, 120), IM_COL32(15, 22, 32, 120),
        IM_COL32(15, 22, 32, 0), IM_COL32(15, 22, 32, 0));
    dl->AddRectFilledMultiColor(ImVec2(x, y + bannerH - 180), ImVec2(x + width, y + bannerH),
        IM_COL32(15, 22, 32, 0), IM_COL32(15, 22, 32, 0),
        IM_COL32(15, 22, 32, 255), IM_COL32(15, 22, 32, 255));
    // Side vignette
    dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + 120, y + bannerH),
        IM_COL32(15, 22, 32, 100), IM_COL32(15, 22, 32, 0),
        IM_COL32(15, 22, 32, 0), IM_COL32(15, 22, 32, 100));

    // Game title with glow effect
    const char* gameName = (sd && !sd->name.empty()) ? sd->name.c_str() : game->name.c_str();
    float titleFontSize = largeFont->FontSize * 1.8f;
    float titleY = y + bannerH - 75;
    // Glow layers
    for (int i = 3; i >= 0; i--) {
        float off = (float)i * 1.5f;
        dl->AddText(largeFont, titleFontSize, ImVec2(x + pad + off, titleY + off),
            IM_COL32(0, 0, 0, 60 - i * 15), gameName);
    }
    dl->AddText(largeFont, titleFontSize, ImVec2(x + pad, titleY), IM_COL32(255, 255, 255, 255), gameName);

    // Subtitle: Developer + Release date
    if (sd && (!sd->developer.empty() || !sd->releaseDate.empty())) {
        std::string subtitle;
        if (!sd->developer.empty()) subtitle = sd->developer;
        if (!sd->releaseDate.empty()) {
            if (!subtitle.empty()) subtitle += u8"  •  ";
            subtitle += sd->releaseDate;
        }
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(x + pad, titleY + titleFontSize + 4),
            IM_COL32(180, 195, 210, 200), subtitle.c_str());
    }

    float contentY = bannerH + 16;
    bool isInstalled = (game->status != "not_installed");

    // ═══════════════════════════════════════════════════════════════════════
    //  ACTION BAR - Modern glass-style button row
    // ═══════════════════════════════════════════════════════════════════════
    float actionBarY = y + contentY;
    float btnH = 48.0f * scale;
    float btnW = 160.0f * scale;

    // Main action button (Play or Install) with glow effect
    ImGui::SetCursorScreenPos(ImVec2(x + pad, actionBarY));
    ImGui::PushID("lib_action_btn");
    ImGui::InvisibleButton("##action", ImVec2(btnW, btnH));
    bool actionHov = ImGui::IsItemHovered();
    bool actionClk = ImGui::IsItemClicked();
    ImGui::PopID();

    if (isInstalled) {
        bool isRunning = GameProcessManager::Get().IsGameRunning(game->appId);

        if (isRunning) {
            // Red stop button with gradient
            if (actionHov) {
                dl->AddRectFilled(ImVec2(x + pad - 4, actionBarY - 4), ImVec2(x + pad + btnW + 4, actionBarY + btnH + 4),
                    IM_COL32(200, 80, 80, 40), 14.0f);
            }
            ImU32 btnTop = actionHov ? IM_COL32(220, 100, 100, 255) : IM_COL32(200, 80, 80, 255);
            ImU32 btnBot = actionHov ? IM_COL32(180, 70, 70, 255) : IM_COL32(160, 60, 60, 255);
            dl->AddRectFilledMultiColor(ImVec2(x + pad, actionBarY), ImVec2(x + pad + btnW, actionBarY + btnH),
                btnTop, btnTop, btnBot, btnBot);
            dl->AddRect(ImVec2(x + pad, actionBarY), ImVec2(x + pad + btnW, actionBarY + btnH),
                IM_COL32(230, 120, 120, 80), 8.0f, 0, 1.0f);

            // Stop icon (square)
            float iconX = x + pad + 28;
            float iconY = actionBarY + btnH / 2;
            float iconSz = 12.0f;
            dl->AddRectFilled(ImVec2(iconX - iconSz/2, iconY - iconSz/2),
                              ImVec2(iconX + iconSz/2, iconY + iconSz/2),
                              IM_COL32(255, 255, 255, 255), 2.0f);

            const char* stopTxt = u8"停止游戏";
            ImVec2 txtSz = mainFont->CalcTextSizeA(mainFont->FontSize * 1.1f, FLT_MAX, 0, stopTxt);
            dl->AddText(mainFont, mainFont->FontSize * 1.1f, ImVec2(iconX + 20, actionBarY + (btnH - txtSz.y) / 2),
                        IM_COL32(255, 255, 255, 255), stopTxt);

            if (actionClk) Navigate(NavAction::StopGame, game->appId);
        } else {
            // Glowing green button with gradient
            if (actionHov) {
                // Outer glow
                dl->AddRectFilled(ImVec2(x + pad - 4, actionBarY - 4), ImVec2(x + pad + btnW + 4, actionBarY + btnH + 4),
                    IM_COL32(140, 195, 65, 40), 14.0f);
            }
            ImU32 btnTop = actionHov ? IM_COL32(165, 215, 85, 255) : IM_COL32(140, 195, 65, 255);
            ImU32 btnBot = actionHov ? IM_COL32(125, 185, 55, 255) : IM_COL32(105, 165, 45, 255);
            dl->AddRectFilledMultiColor(ImVec2(x + pad, actionBarY), ImVec2(x + pad + btnW, actionBarY + btnH),
                btnTop, btnTop, btnBot, btnBot);
            dl->AddRect(ImVec2(x + pad, actionBarY), ImVec2(x + pad + btnW, actionBarY + btnH),
                IM_COL32(180, 220, 100, 80), 8.0f, 0, 1.0f);

            // Play icon (SVG style)
            float iconX = x + pad + 28;
            float iconY = actionBarY + btnH / 2;
            DrawIconPlay(dl, iconX, iconY, 28.0f, IM_COL32(40, 60, 20, 255), true);

            const char* playTxt = u8"开始游戏";
            ImVec2 txtSz = mainFont->CalcTextSizeA(mainFont->FontSize * 1.1f, FLT_MAX, 0, playTxt);
            dl->AddText(mainFont, mainFont->FontSize * 1.1f, ImVec2(iconX + 20, actionBarY + (btnH - txtSz.y) / 2),
                        IM_COL32(40, 60, 20, 255), playTxt);

            if (actionClk) {
                Log(LogLevel::Info, "Library: Play button clicked for appId=%d", game->appId);
                Navigate(NavAction::LaunchGame, game->appId);
            }
        }
    } else {
        // Deep blue install button with glow (Steam style)
        if (actionHov) {
            dl->AddRectFilled(ImVec2(x + pad - 4, actionBarY - 4), ImVec2(x + pad + btnW + 4, actionBarY + btnH + 4),
                IM_COL32(30, 90, 160, 50), 14.0f);
        }
        ImU32 btnTop = actionHov ? IM_COL32(50, 120, 190, 255) : IM_COL32(35, 100, 170, 255);
        ImU32 btnBot = actionHov ? IM_COL32(30, 90, 160, 255) : IM_COL32(20, 70, 140, 255);
        dl->AddRectFilledMultiColor(ImVec2(x + pad, actionBarY), ImVec2(x + pad + btnW, actionBarY + btnH),
            btnTop, btnTop, btnBot, btnBot);
        dl->AddRect(ImVec2(x + pad, actionBarY), ImVec2(x + pad + btnW, actionBarY + btnH),
            IM_COL32(80, 140, 200, 100), 8.0f, 0, 1.0f);

        // Download icon (SVG style) - centered with text
        const char* installTxt = u8"安装游戏";
        ImVec2 txtSz = mainFont->CalcTextSizeA(mainFont->FontSize * 1.1f, FLT_MAX, 0, installTxt);
        float iconSize = 24.0f;
        float totalW = iconSize + 8 + txtSz.x;  // icon + gap + text
        float startX = x + pad + (btnW - totalW) / 2;

        float iconX = startX + iconSize / 2;
        float iconY = actionBarY + btnH / 2;
        DrawIconDownload(dl, iconX, iconY, iconSize, IM_COL32(255, 255, 255, 255), 2.5f);

        dl->AddText(mainFont, mainFont->FontSize * 1.1f, ImVec2(startX + iconSize + 8, actionBarY + (btnH - txtSz.y) / 2),
                    IM_COL32(255, 255, 255, 255), installTxt);

        if (actionClk) Navigate(NavAction::InstallGame, game->appId, gameName, game->size);
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  INFO ITEMS - Steam-style status info next to button (responsive)
    // ═══════════════════════════════════════════════════════════════════════
    float infoItemX = x + pad + btnW + 35;
    float infoFontSize = mainFont->FontSize * 0.95f;  // Larger font for info items
    float infoLineH = infoFontSize + 5;
    float totalTextH = infoFontSize * 2 + 5;  // Two lines of text
    float infoItemY = actionBarY + (btnH - totalTextH) / 2;  // Vertically centered in button height

    // Responsive icon size based on button height
    float iconSize = btnH * 0.55f;
    if (iconSize < 24.0f) iconSize = 24.0f;
    if (iconSize > 36.0f) iconSize = 36.0f;

    // Fixed spacing between info items for consistent layout
    const float itemSpacing = 30.0f;

    // Helper lambda to draw an info item with icon centered vertically
    auto DrawInfoItemWithIcon = [&](float& itemX, float iconW, const char* label, const char* value, ImU32 valueColor,
                            std::function<void(float cx, float cy, float sz)> drawIcon) {
        // Icon centered vertically with text block
        float iconCx = itemX + iconW / 2;
        float iconCy = actionBarY + btnH / 2;
        drawIcon(iconCx, iconCy, iconSize);

        // Text to the right of icon
        float textX = itemX + iconW + 8;
        dl->AddText(mainFont, infoFontSize, ImVec2(textX, infoItemY),
            IM_COL32(140, 155, 170, 255), label);
        dl->AddText(mainFont, infoFontSize, ImVec2(textX, infoItemY + infoLineH),
            valueColor, value);

        // Calculate width for next item
        ImVec2 labelSz = mainFont->CalcTextSizeA(infoFontSize, FLT_MAX, 0, label);
        ImVec2 valueSz = mainFont->CalcTextSizeA(infoFontSize, FLT_MAX, 0, value);
        float textW = (labelSz.x > valueSz.x) ? labelSz.x : valueSz.x;
        itemX += iconW + 8 + textW + itemSpacing;  // Move to next item position
    };

    // Helper lambda to draw text-only info item (no icon)
    auto DrawInfoItemText = [&](float& itemX, const char* label, const char* value, ImU32 valueColor) {
        dl->AddText(mainFont, infoFontSize, ImVec2(itemX, infoItemY),
            IM_COL32(140, 155, 170, 255), label);
        dl->AddText(mainFont, infoFontSize, ImVec2(itemX, infoItemY + infoLineH),
            valueColor, value);

        ImVec2 labelSz = mainFont->CalcTextSizeA(infoFontSize, FLT_MAX, 0, label);
        ImVec2 valueSz = mainFont->CalcTextSizeA(infoFontSize, FLT_MAX, 0, value);
        float textW = (labelSz.x > valueSz.x) ? labelSz.x : valueSz.x;
        itemX += textW + itemSpacing;
    };

    // Check if game supports Steam Cloud (from categories)
    bool hasCloudSave = false;
    if (sd) {
        for (const auto& cat : sd->categories) {
            if (cat.find("Cloud") != std::string::npos || cat.find("云") != std::string::npos) {
                hasCloudSave = true;
                break;
            }
        }
    }

    // Format game size string - prefer Steam API data, fallback to GameInfo
    char sizeStr[32];
    long long diskBytes = 0;
    if (sd && sd->diskSpaceBytes > 0) {
        diskBytes = sd->diskSpaceBytes;
    }

    if (diskBytes > 0) {
        double sizeGB = (double)diskBytes / (1024.0 * 1024.0 * 1024.0);
        if (sizeGB >= 1.0) {
            snprintf(sizeStr, sizeof(sizeStr), "%.1f GB", sizeGB);
        } else {
            double sizeMB = (double)diskBytes / (1024.0 * 1024.0);
            snprintf(sizeStr, sizeof(sizeStr), "%.0f MB", sizeMB);
        }
    } else if (game->size >= 1.0f) {
        snprintf(sizeStr, sizeof(sizeStr), "%.1f GB", game->size);
    } else if (game->size > 0.0f) {
        snprintf(sizeStr, sizeof(sizeStr), "%.0f MB", game->size * 1024.0f);
    } else {
        snprintf(sizeStr, sizeof(sizeStr), "--");
    }

    if (isInstalled) {
        // ─── INSTALLED GAME INFO ───
        // 1. Game size (disk icon)
        DrawInfoItemWithIcon(infoItemX, iconSize, u8"磁盘空间", sizeStr, IM_COL32(220, 235, 250, 255),
            [&](float cx, float cy, float sz) {
                // Draw disk/storage icon
                float r = sz * 0.4f;
                dl->AddRect(ImVec2(cx - r, cy - r * 0.7f), ImVec2(cx + r, cy + r * 0.7f),
                    IM_COL32(140, 155, 170, 220), r * 0.2f, 0, 2.0f);
                dl->AddLine(ImVec2(cx - r * 0.5f, cy), ImVec2(cx + r * 0.5f, cy),
                    IM_COL32(140, 155, 170, 220), 2.0f);
                dl->AddCircleFilled(ImVec2(cx + r * 0.5f, cy + r * 0.3f), r * 0.15f, IM_COL32(140, 155, 170, 220));
            });

        // 2. Cloud sync status (only if game supports it)
        if (hasCloudSave) {
            DrawInfoItemWithIcon(infoItemX, iconSize, u8"云状态", u8"已同步", IM_COL32(120, 200, 120, 255),
                [&](float cx, float cy, float sz) {
                    DrawIconCloud(dl, cx, cy, sz, IM_COL32(120, 200, 120, 255), 2.0f, false);
                });
        }

        // 3. Last played date
        DrawInfoItemText(infoItemX, u8"最后运行日期", u8"今天", IM_COL32(220, 235, 250, 255));

        // 4. Playtime with clock icon
        DrawInfoItemWithIcon(infoItemX, iconSize, u8"游戏时间", FormatPlaytime(game->playtime).c_str(), IM_COL32(220, 235, 250, 255),
            [&](float cx, float cy, float sz) {
                float r = sz * 0.4f;
                dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(140, 155, 170, 220), 16, 2.0f);
                dl->AddLine(ImVec2(cx, cy - r * 0.55f), ImVec2(cx, cy), IM_COL32(140, 155, 170, 220), 2.0f);
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx + r * 0.4f, cy + r * 0.4f), IM_COL32(140, 155, 170, 220), 2.0f);
            });

    } else {
        // ─── NOT INSTALLED GAME INFO ───
        // 1. Game size (disk icon) - show required space
        DrawInfoItemWithIcon(infoItemX, iconSize, u8"所需空间", sizeStr, IM_COL32(220, 235, 250, 255),
            [&](float cx, float cy, float sz) {
                // Draw disk/storage icon
                float r = sz * 0.4f;
                dl->AddRect(ImVec2(cx - r, cy - r * 0.7f), ImVec2(cx + r, cy + r * 0.7f),
                    IM_COL32(140, 155, 170, 220), r * 0.2f, 0, 2.0f);
                dl->AddLine(ImVec2(cx - r * 0.5f, cy), ImVec2(cx + r * 0.5f, cy),
                    IM_COL32(140, 155, 170, 220), 2.0f);
                dl->AddCircleFilled(ImVec2(cx + r * 0.5f, cy + r * 0.3f), r * 0.15f, IM_COL32(140, 155, 170, 220));
            });

        // 2. Cloud sync status (only if game supports it)
        if (hasCloudSave) {
            DrawInfoItemWithIcon(infoItemX, iconSize, u8"云存档", u8"支持", IM_COL32(120, 200, 120, 255),
                [&](float cx, float cy, float sz) {
                    DrawIconCloud(dl, cx, cy, sz, IM_COL32(120, 200, 120, 255), 2.0f, false);
                });
        }

        // 3. Last played date
        const char* lastPlayedStr = game->playtime > 0 ? u8"--" : u8"从未运行";
        DrawInfoItemText(infoItemX, u8"最后运行日期", lastPlayedStr, IM_COL32(220, 235, 250, 255));

        // 4. Playtime with clock icon
        DrawInfoItemWithIcon(infoItemX, iconSize, u8"游戏时间", FormatPlaytime(game->playtime).c_str(), IM_COL32(220, 235, 250, 255),
            [&](float cx, float cy, float sz) {
                float r = sz * 0.4f;
                dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(140, 155, 170, 220), 16, 2.0f);
                dl->AddLine(ImVec2(cx, cy - r * 0.55f), ImVec2(cx, cy), IM_COL32(140, 155, 170, 220), 2.0f);
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx + r * 0.4f, cy + r * 0.4f), IM_COL32(140, 155, 170, 220), 2.0f);
            });
    }

    contentY += btnH + 20;

    // ═══════════════════════════════════════════════════════════════════════
    //  CONTENT AREA - Modern glass card layout
    // ═══════════════════════════════════════════════════════════════════════
    float infoY = y + contentY;
    float leftColW = width * 0.62f;
    float rightColW = width - leftColW - pad * 2;

    // ─────────────────────────────────────────────────────────────────────
    //  LEFT COLUMN: Media gallery (screenshots + videos) with click to switch
    // ─────────────────────────────────────────────────────────────────────
    float ssAreaX = x + pad;
    float ssAreaW = leftColW - pad;
    float ssH = ssAreaW * 0.5625f; // 16:9

    // Calculate total media count (screenshots + movies)
    int numScreenshots = sd ? (int)sd->screenshotUrls.size() : 0;
    int numMovies = sd ? (int)sd->movies.size() : 0;
    int totalMedia = numScreenshots + numMovies;

    // Auto-slideshow logic: only for screenshots when no videos, every 3 seconds
    float currentTime = (float)ImGui::GetTime();
    float deltaTime = currentTime - lastFrameTime_;
    lastFrameTime_ = currentTime;

    // Only auto-slide if: has screenshots, no videos, not hovering thumbnails
    bool shouldAutoSlide = (numScreenshots > 1) && (numMovies == 0);
    if (shouldAutoSlide) {
        autoSlideTimer_ += deltaTime;
        if (autoSlideTimer_ >= 3.0f) {
            autoSlideTimer_ = 0.0f;
            selectedMediaIdx_ = (selectedMediaIdx_ + 1) % numScreenshots;
        }
    }

    // Clamp selected index
    if (selectedMediaIdx_ >= totalMedia) selectedMediaIdx_ = 0;
    if (selectedMediaIdx_ < 0) selectedMediaIdx_ = 0;

    // Determine if current selection is a video
    bool isVideoSelected = (selectedMediaIdx_ >= numScreenshots) && (numMovies > 0);
    int videoIdx = isVideoSelected ? (selectedMediaIdx_ - numScreenshots) : -1;
    int ssIdx = isVideoSelected ? -1 : selectedMediaIdx_;

    // Glass frame around main display
    DrawGlassCard(ssAreaX - 4, infoY - 4, ssAreaW + 8, ssH + 8, 0.06f);

    // Check if video ended
    if (isPlayingVideo_ && !IsVideoActive()) {
        isPlayingVideo_ = false;
        playingVideoIdx_ = -1;
    }

    // Get video frame if playing
    ImTextureID videoFrameTex = (ImTextureID)0;
    if (isPlayingVideo_) {
        int vw, vh;
        videoFrameTex = GetVideoFrame(&vw, &vh);
    }

    // Main display area
    if (isPlayingVideo_ && videoFrameTex) {
        // Show video frame
        dl->AddImageRounded(videoFrameTex, ImVec2(ssAreaX, infoY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);

        // Video controls bar at bottom
        float barH = 36.0f;
        float barY = infoY + ssH - barH;
        dl->AddRectFilled(ImVec2(ssAreaX, barY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                          IM_COL32(0, 0, 0, 180), 0, ImDrawFlags_RoundCornersBottom);

        // Pause/Play button
        float btnX = ssAreaX + 12;
        float btnY = barY + 8;
        float btnSz = 20.0f;
        ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
        ImGui::PushID("lib_vidpause");
        ImGui::InvisibleButton("##vp", ImVec2(btnSz, btnSz));
        bool pauseHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            if (IsVideoPaused()) ResumeVideo();
            else PauseVideo();
        }
        ImGui::PopID();

        if (IsVideoPaused()) {
            // Draw play triangle
            ImVec2 tri[3] = {
                ImVec2(btnX + 4, btnY + 2), ImVec2(btnX + 4, btnY + btnSz - 2), ImVec2(btnX + btnSz - 2, btnY + btnSz * 0.5f)
            };
            dl->AddTriangleFilled(tri[0], tri[1], tri[2],
                                  pauseHov ? IM_COL32(102, 192, 244, 255) : IM_COL32(255, 255, 255, 220));
        } else {
            // Draw pause bars
            ImU32 pc = pauseHov ? IM_COL32(102, 192, 244, 255) : IM_COL32(255, 255, 255, 220);
            dl->AddRectFilled(ImVec2(btnX + 3, btnY + 2), ImVec2(btnX + 8, btnY + btnSz - 2), pc);
            dl->AddRectFilled(ImVec2(btnX + 12, btnY + 2), ImVec2(btnX + 17, btnY + btnSz - 2), pc);
        }

        // Progress bar
        float progX = btnX + btnSz + 12;
        float progW = ssAreaW - 80;
        float progY = barY + barH / 2 - 3;
        float progH = 6.0f;
        double duration = GetVideoDuration();
        double position = GetVideoPosition();
        float progress = (duration > 0) ? (float)(position / duration) : 0.0f;

        dl->AddRectFilled(ImVec2(progX, progY), ImVec2(progX + progW, progY + progH),
                          IM_COL32(60, 70, 85, 200), 3.0f);
        dl->AddRectFilled(ImVec2(progX, progY), ImVec2(progX + progW * progress, progY + progH),
                          IM_COL32(102, 192, 244, 255), 3.0f);

        // Seek on click
        ImGui::SetCursorScreenPos(ImVec2(progX, progY - 4));
        ImGui::PushID("lib_vidseek");
        ImGui::InvisibleButton("##seek", ImVec2(progW, progH + 8));
        if (ImGui::IsItemClicked() && duration > 0) {
            float mx = ImGui::GetMousePos().x;
            float seekPct = (mx - progX) / progW;
            if (seekPct < 0) seekPct = 0;
            if (seekPct > 1) seekPct = 1;
            SeekVideo(seekPct * duration);
        }
        ImGui::PopID();

        // Stop button (X)
        float stopX = ssAreaX + ssAreaW - 36;
        float stopY = barY + 8;
        ImGui::SetCursorScreenPos(ImVec2(stopX, stopY));
        ImGui::PushID("lib_vidstop");
        ImGui::InvisibleButton("##stop", ImVec2(20, 20));
        bool stopHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            StopVideo();
            isPlayingVideo_ = false;
            playingVideoIdx_ = -1;
        }
        ImGui::PopID();
        ImU32 stopCol = stopHov ? IM_COL32(255, 100, 100, 255) : IM_COL32(255, 255, 255, 200);
        dl->AddLine(ImVec2(stopX + 4, stopY + 4), ImVec2(stopX + 16, stopY + 16), stopCol, 2.0f);
        dl->AddLine(ImVec2(stopX + 16, stopY + 4), ImVec2(stopX + 4, stopY + 16), stopCol, 2.0f);

    } else if (isVideoSelected && videoIdx >= 0 && videoIdx < numMovies) {
        // Show video thumbnail with play button overlay
        ImTextureID movTex = GetTextureByKey("mov_" + game->appId + "_" + std::to_string(videoIdx));
        if (movTex) {
            dl->AddImageRounded(movTex, ImVec2(ssAreaX, infoY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);
        } else {
            dl->AddRectFilled(ImVec2(ssAreaX, infoY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                              IM_COL32(15, 20, 30, 255), 8.0f);
        }

        // Dark overlay for video
        dl->AddRectFilled(ImVec2(ssAreaX, infoY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                          IM_COL32(0, 0, 0, 100), 8.0f);

        // Large play button in center
        float playCx = ssAreaX + ssAreaW / 2;
        float playCy = infoY + ssH / 2;
        float playR = 40.0f;

        // Play button circle
        dl->AddCircleFilled(ImVec2(playCx, playCy), playR, IM_COL32(255, 255, 255, 200));
        dl->AddCircle(ImVec2(playCx, playCy), playR, IM_COL32(102, 192, 244, 255), 32, 3.0f);

        // Play triangle
        DrawIconPlay(dl, playCx + 4, playCy, 36.0f, IM_COL32(30, 40, 55, 255), true);

        // Video title
        if (!sd->movies[videoIdx].name.empty()) {
            const char* vidName = sd->movies[videoIdx].name.c_str();
            ImVec2 nameSz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, vidName);
            dl->AddRectFilled(ImVec2(ssAreaX, infoY + ssH - 35), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                              IM_COL32(0, 0, 0, 180), 0, ImDrawFlags_RoundCornersBottom);
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(ssAreaX + 12, infoY + ssH - 28),
                        IM_COL32(255, 255, 255, 255), vidName);
        }

        // Click to play video in-app
        ImGui::SetCursorScreenPos(ImVec2(ssAreaX, infoY));
        ImGui::PushID("play_video");
        ImGui::InvisibleButton("##playvid", ImVec2(ssAreaW, ssH));
        if (ImGui::IsItemClicked() && sd) {
            const auto& mov = sd->movies[videoIdx];
            std::string videoUrl;
            if (!mov.mp4Url.empty()) videoUrl = mov.mp4Url;
            else if (!mov.hlsUrl.empty()) videoUrl = mov.hlsUrl;
            else if (!mov.dashUrl.empty()) videoUrl = mov.dashUrl;

            if (!videoUrl.empty()) {
                if (isPlayingVideo_) StopVideo();
                if (PlayVideo(videoUrl)) {
                    isPlayingVideo_ = true;
                    playingVideoIdx_ = videoIdx;
                }
            }
        }
        ImGui::PopID();

    } else {
        // Show screenshot
        ImTextureID ssTex = (ImTextureID)0;
        if (sd && ssIdx >= 0 && ssIdx < numScreenshots) {
            ssTex = GetTextureByKey("ss_" + game->appId + "_" + std::to_string(ssIdx));
        }

        if (ssTex) {
            dl->AddImageRounded(ssTex, ImVec2(ssAreaX, infoY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);
        } else if (tex) {
            // Fallback to header image while loading
            dl->AddImageRounded(tex, ImVec2(ssAreaX, infoY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 180), 8.0f);
            // Show loading indicator
            const char* loadTxt = u8"加载截图中...";
            ImVec2 loadSz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, loadTxt);
            dl->AddRectFilled(ImVec2(ssAreaX + (ssAreaW - loadSz.x) / 2 - 10, infoY + ssH - 40),
                              ImVec2(ssAreaX + (ssAreaW + loadSz.x) / 2 + 10, infoY + ssH - 15),
                              IM_COL32(0, 0, 0, 180), 6.0f);
            dl->AddText(smallFont, smallFont->FontSize,
                        ImVec2(ssAreaX + (ssAreaW - loadSz.x) / 2, infoY + ssH - 35),
                        IM_COL32(255, 255, 255, 200), loadTxt);
        } else {
            dl->AddRectFilled(ImVec2(ssAreaX, infoY), ImVec2(ssAreaX + ssAreaW, infoY + ssH),
                              IM_COL32(25, 32, 42, 255), 8.0f);
            const char* loadTxt = u8"加载中...";
            ImVec2 loadSz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, loadTxt);
            dl->AddText(smallFont, smallFont->FontSize,
                        ImVec2(ssAreaX + (ssAreaW - loadSz.x) / 2, infoY + (ssH - loadSz.y) / 2),
                        IM_COL32(100, 115, 130, 150), loadTxt);
        }
    }

    // Left/Right navigation arrows on main display (only if more than 1 media item)
    if (totalMedia > 1) {
        float arrowSize = 36.0f;
        float arrowPad = 16.0f;
        float arrowY = infoY + ssH / 2;

        // Left arrow
        float leftArrowX = ssAreaX + arrowPad;
        ImGui::SetCursorScreenPos(ImVec2(leftArrowX - arrowSize / 2, arrowY - arrowSize / 2));
        ImGui::PushID("media_left");
        ImGui::InvisibleButton("##left", ImVec2(arrowSize, arrowSize));
        bool leftHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            selectedMediaIdx_ = (selectedMediaIdx_ - 1 + totalMedia) % totalMedia;
            autoSlideTimer_ = 0.0f;
        }
        ImGui::PopID();

        // Draw left arrow circle + chevron
        ImU32 leftBg = leftHov ? IM_COL32(0, 0, 0, 180) : IM_COL32(0, 0, 0, 100);
        dl->AddCircleFilled(ImVec2(leftArrowX, arrowY), arrowSize / 2, leftBg);
        // Chevron left <
        float chevW = 10.0f, chevH = 16.0f;
        ImVec2 p1(leftArrowX + chevW / 3, arrowY - chevH / 2);
        ImVec2 p2(leftArrowX - chevW / 2, arrowY);
        ImVec2 p3(leftArrowX + chevW / 3, arrowY + chevH / 2);
        dl->AddLine(p1, p2, IM_COL32(255, 255, 255, leftHov ? 255 : 180), 2.5f);
        dl->AddLine(p2, p3, IM_COL32(255, 255, 255, leftHov ? 255 : 180), 2.5f);

        // Right arrow
        float rightArrowX = ssAreaX + ssAreaW - arrowPad;
        ImGui::SetCursorScreenPos(ImVec2(rightArrowX - arrowSize / 2, arrowY - arrowSize / 2));
        ImGui::PushID("media_right");
        ImGui::InvisibleButton("##right", ImVec2(arrowSize, arrowSize));
        bool rightHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            selectedMediaIdx_ = (selectedMediaIdx_ + 1) % totalMedia;
            autoSlideTimer_ = 0.0f;
        }
        ImGui::PopID();

        // Draw right arrow circle + chevron
        ImU32 rightBg = rightHov ? IM_COL32(0, 0, 0, 180) : IM_COL32(0, 0, 0, 100);
        dl->AddCircleFilled(ImVec2(rightArrowX, arrowY), arrowSize / 2, rightBg);
        // Chevron right >
        ImVec2 r1(rightArrowX - chevW / 3, arrowY - chevH / 2);
        ImVec2 r2(rightArrowX + chevW / 2, arrowY);
        ImVec2 r3(rightArrowX - chevW / 3, arrowY + chevH / 2);
        dl->AddLine(r1, r2, IM_COL32(255, 255, 255, rightHov ? 255 : 180), 2.5f);
        dl->AddLine(r2, r3, IM_COL32(255, 255, 255, rightHov ? 255 : 180), 2.5f);
    }

    // Thumbnails row (screenshots first, then videos)
    float thumbY = infoY + ssH + 12;
    float thumbH = 56.0f;
    float thumbW = thumbH * 1.78f; // 16:9
    float thumbGap = 8.0f;
    int maxThumbs = (int)((ssAreaW + thumbGap) / (thumbW + thumbGap));
    if (maxThumbs > 10) maxThumbs = 10;

    // Reserve space for videos - show at least 2 video thumbnails if available
    int reservedForVideos = std::min(numMovies, 3);
    int maxScreenshotThumbs = maxThumbs - reservedForVideos;
    if (maxScreenshotThumbs < 3) maxScreenshotThumbs = 3;  // Show at least 3 screenshots

    int thumbIdx = 0;
    float tx = ssAreaX;

    // Screenshot thumbnails (limited to leave room for videos)
    for (int i = 0; i < numScreenshots && thumbIdx < maxScreenshotThumbs; i++, thumbIdx++) {
        std::string key = "ss_" + game->appId + "_" + std::to_string(i);
        ImTextureID thumbTex = GetTextureByKey(key);
        bool isSelected = (selectedMediaIdx_ == i);

        // Clickable thumbnail
        ImGui::SetCursorScreenPos(ImVec2(tx, thumbY));
        ImGui::PushID(("ss_thumb_" + std::to_string(i)).c_str());
        ImGui::InvisibleButton("##thumb", ImVec2(thumbW, thumbH));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            selectedMediaIdx_ = i;
            autoSlideTimer_ = 0.0f;  // Reset auto-slide timer on manual selection
        }
        // Reset timer when hovering thumbnails
        if (hovered) {
            autoSlideTimer_ = 0.0f;
        }
        ImGui::PopID();

        // Draw thumbnail
        if (thumbTex) {
            ImU32 tint = isSelected ? IM_COL32(255, 255, 255, 255) : (hovered ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 255, 255, 160));
            dl->AddImageRounded(thumbTex, ImVec2(tx, thumbY), ImVec2(tx + thumbW, thumbY + thumbH),
                                 ImVec2(0, 0), ImVec2(1, 1), tint, 4.0f);
        } else {
            dl->AddRectFilled(ImVec2(tx, thumbY), ImVec2(tx + thumbW, thumbY + thumbH),
                              IM_COL32(30, 38, 50, 200), 4.0f);
        }

        // Selection highlight
        if (isSelected) {
            dl->AddRect(ImVec2(tx - 2, thumbY - 2), ImVec2(tx + thumbW + 2, thumbY + thumbH + 2),
                        IM_COL32(102, 192, 244, 255), 6.0f, 0, 2.5f);
        } else if (hovered) {
            dl->AddRect(ImVec2(tx - 1, thumbY - 1), ImVec2(tx + thumbW + 1, thumbY + thumbH + 1),
                        IM_COL32(102, 192, 244, 120), 5.0f, 0, 1.5f);
        }

        tx += thumbW + thumbGap;
    }

    // Video thumbnails (with play icon overlay)
    for (int i = 0; i < numMovies && thumbIdx < maxThumbs; i++, thumbIdx++) {
        int mediaIdx = numScreenshots + i;
        std::string key = "mov_" + game->appId + "_" + std::to_string(i);
        ImTextureID thumbTex = GetTextureByKey(key);
        bool isSelected = (selectedMediaIdx_ == mediaIdx);

        // Clickable thumbnail
        ImGui::SetCursorScreenPos(ImVec2(tx, thumbY));
        ImGui::PushID(("mov_thumb_" + std::to_string(i)).c_str());
        ImGui::InvisibleButton("##movthumb", ImVec2(thumbW, thumbH));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            selectedMediaIdx_ = mediaIdx;
            autoSlideTimer_ = 0.0f;  // Reset auto-slide timer
        }
        ImGui::PopID();

        // Draw thumbnail
        if (thumbTex) {
            ImU32 tint = isSelected ? IM_COL32(255, 255, 255, 255) : (hovered ? IM_COL32(255, 255, 255, 220) : IM_COL32(255, 255, 255, 160));
            dl->AddImageRounded(thumbTex, ImVec2(tx, thumbY), ImVec2(tx + thumbW, thumbY + thumbH),
                                 ImVec2(0, 0), ImVec2(1, 1), tint, 4.0f);
        } else {
            dl->AddRectFilled(ImVec2(tx, thumbY), ImVec2(tx + thumbW, thumbY + thumbH),
                              IM_COL32(25, 35, 50, 220), 4.0f);
        }

        // Dark overlay for video
        dl->AddRectFilled(ImVec2(tx, thumbY), ImVec2(tx + thumbW, thumbY + thumbH),
                          IM_COL32(0, 0, 0, 80), 4.0f);

        // Play icon on video thumbnail
        float pcx = tx + thumbW / 2;
        float pcy = thumbY + thumbH / 2;
        dl->AddCircleFilled(ImVec2(pcx, pcy), 12.0f, IM_COL32(255, 255, 255, 200));
        DrawIconPlay(dl, pcx + 2, pcy, 14.0f, IM_COL32(30, 40, 55, 255), true);

        // Selection highlight
        if (isSelected) {
            dl->AddRect(ImVec2(tx - 2, thumbY - 2), ImVec2(tx + thumbW + 2, thumbY + thumbH + 2),
                        IM_COL32(102, 192, 244, 255), 6.0f, 0, 2.5f);
        } else if (hovered) {
            dl->AddRect(ImVec2(tx - 1, thumbY - 1), ImVec2(tx + thumbW + 1, thumbY + thumbH + 1),
                        IM_COL32(102, 192, 244, 120), 5.0f, 0, 1.5f);
        }

        tx += thumbW + thumbGap;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  RIGHT COLUMN: Achievements glass card
    // ─────────────────────────────────────────────────────────────────────
    float statsX = x + leftColW + pad;
    float statsW = rightColW;
    float statsCardH = ssH + thumbH + 16;

    // Glass card background for achievements (single card, no nested frames)
    DrawGlassCard(statsX, infoY, statsW, statsCardH, 0.08f);

    float statY = infoY + 16;
    float statPad = 16.0f;

    if (sd && sd->totalAchievements > 0) {
        // Achievement section header
        DrawIconTrophy(dl, statsX + statPad + 12, statY + 10, 24.0f, IM_COL32(255, 215, 100, 255), 2.0f);
        dl->AddText(mainFont, mainFont->FontSize * 1.1f, ImVec2(statsX + statPad + 30, statY),
                    IM_COL32(255, 255, 255, 255), u8"成就");
        char achCountBuf[32];
        snprintf(achCountBuf, sizeof(achCountBuf), u8"共 %d 项", sd->totalAchievements);
        ImVec2 achTitleSz = mainFont->CalcTextSizeA(mainFont->FontSize * 1.1f, FLT_MAX, 0, u8"成就");
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(statsX + statPad + 30 + achTitleSz.x + 10, statY + 4),
                    IM_COL32(140, 155, 170, 200), achCountBuf);
        statY += mainFont->FontSize * 1.1f + 12;

        // Calculate grid layout to fill available space - show more achievements
        float availableH = statsCardH - (statY - infoY) - 45;  // Leave space for "view all" button
        float achIconSize = 52.0f;  // Slightly smaller icons to fit more
        float achIconGap = 8.0f;
        int cols = (int)((statsW - statPad * 2 + achIconGap) / (achIconSize + achIconGap));
        if (cols < 4) cols = 4;
        if (cols > 6) cols = 6;
        int rows = (int)((availableH + achIconGap) / (achIconSize + achIconGap + 6));
        if (rows < 3) rows = 3;
        if (rows > 5) rows = 5;
        int maxAchIcons = std::min(cols * rows, (int)sd->achievements.size());

        // Center the grid horizontally
        float gridW = cols * achIconSize + (cols - 1) * achIconGap;
        float achStartX = statsX + (statsW - gridW) / 2;

        for (int i = 0; i < maxAchIcons; i++) {
            const auto& ach = sd->achievements[i];
            int row = i / cols;
            int col = i % cols;
            float achIconX = achStartX + col * (achIconSize + achIconGap);
            float achIconY = statY + row * (achIconSize + achIconGap + 6);

            ImTextureID achTex = GetTextureByKey("ach_" + game->appId + "_" + std::to_string(i));

            // Simple rounded rect background (no nested glass card)
            dl->AddRectFilled(ImVec2(achIconX, achIconY),
                ImVec2(achIconX + achIconSize, achIconY + achIconSize),
                IM_COL32(30, 38, 50, 180), 6.0f);

            if (achTex) {
                dl->AddImageRounded(achTex, ImVec2(achIconX, achIconY),
                    ImVec2(achIconX + achIconSize, achIconY + achIconSize),
                    ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
            } else {
                // Trophy icon placeholder
                float cx = achIconX + achIconSize / 2;
                float cy = achIconY + achIconSize / 2;
                DrawIconTrophy(dl, cx, cy, achIconSize * 0.45f, IM_COL32(180, 160, 100, 150), 1.5f);
            }

            // Rarity indicator bar
            float rarityW = achIconSize * (ach.globalPercent / 100.0f);
            ImU32 rarityColor = ach.globalPercent < 10 ? IM_COL32(255, 215, 0, 220) :
                               ach.globalPercent < 30 ? IM_COL32(192, 192, 192, 220) :
                                                        IM_COL32(205, 127, 50, 220);
            dl->AddRectFilled(ImVec2(achIconX, achIconY + achIconSize + 2),
                ImVec2(achIconX + achIconSize, achIconY + achIconSize + 5),
                IM_COL32(20, 25, 35, 200), 2.0f);
            dl->AddRectFilled(ImVec2(achIconX, achIconY + achIconSize + 2),
                ImVec2(achIconX + rarityW, achIconY + achIconSize + 5), rarityColor, 2.0f);
        }

        // "View all" button at bottom right (clickable) - larger text
        if ((int)sd->achievements.size() > maxAchIcons) {
            const char* btnText = u8"查看全部 >";
            float btnFontSize = mainFont->FontSize * 0.95f;
            ImVec2 btnTextSz = mainFont->CalcTextSizeA(btnFontSize, FLT_MAX, 0, btnText);
            float btnW = btnTextSz.x + 16;
            float btnH = 28.0f;
            float btnX = statsX + statsW - statPad - btnW;  // Right aligned
            float btnY = infoY + statsCardH - 36;

            ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
            ImGui::PushID("view_all_ach");
            ImGui::InvisibleButton("##viewall", ImVec2(btnW, btnH));
            bool hovered = ImGui::IsItemHovered();
            bool clicked = ImGui::IsItemClicked();
            ImGui::PopID();

            ImU32 btnColor = hovered ? IM_COL32(102, 192, 244, 255) : IM_COL32(102, 192, 244, 180);
            dl->AddText(mainFont, btnFontSize, ImVec2(btnX, btnY + 4), btnColor, btnText);

            if (clicked) {
                showAchievementsPopup_ = true;
                achievementsPopupAppId_ = game->appId;
                achievementsScrollY_ = 0.0f;
            }
        }
    } else {
        // No achievements - show placeholder
        float centerY = infoY + statsCardH / 2;
        DrawIconTrophy(dl, statsX + statsW / 2, centerY - 20, 48.0f, IM_COL32(80, 90, 105, 150), 2.0f);
        const char* noAchTxt = u8"暂无成就数据";
        ImVec2 noAchSz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, noAchTxt);
        dl->AddText(smallFont, smallFont->FontSize,
            ImVec2(statsX + (statsW - noAchSz.x) / 2, centerY + 20),
            IM_COL32(100, 115, 130, 150), noAchTxt);
    }

    contentY += ssH + thumbH + 50;

    // ═══════════════════════════════════════════════════════════════════════
    //  NEWS & UPDATES SECTION - Latest news and announcements
    // ═══════════════════════════════════════════════════════════════════════
    if (sd && !sd->news.empty()) {
        float newsY = y + contentY;
        float newsCardW = width - pad * 2;

        // Section header with icon - larger font
        DrawIconNews(dl, x + pad + 12, newsY + 12, 32.0f, IM_COL32(102, 192, 244, 255), 2.0f);
        dl->AddText(mainFont, mainFont->FontSize * 1.3f, ImVec2(x + pad + 48, newsY),
                    IM_COL32(255, 255, 255, 255), u8"最新动态");
        newsY += mainFont->FontSize * 1.3f + 20;

        // Single glass card container for all news - larger items
        float singleCardW = (newsCardW - 20) / 2;
        float newsItemH = 130.0f;  // Increased for content preview
        int maxNews = std::min(4, (int)sd->news.size());
        int newsRows = (maxNews + 1) / 2;
        float totalNewsH = newsRows * newsItemH + (newsRows - 1) * 14 + 28;

        DrawGlassCard(x + pad, newsY, newsCardW, totalNewsH, 0.06f);

        for (int i = 0; i < maxNews; i++) {
            const auto& news = sd->news[i];
            float nx = x + pad + 14 + (i % 2) * (singleCardW + 12);
            float ny = newsY + 14 + (i / 2) * (newsItemH + 14);

            // News item background (subtle, no nested glass)
            dl->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + singleCardW - 10, ny + newsItemH),
                IM_COL32(255, 255, 255, 10), 8.0f);

            // Wider image - 16:10 aspect ratio
            float imgW = 140.0f;
            float imgH = 84.0f;
            ImTextureID newsTex = GetTextureByKey("news_thumb_" + game->appId + "_" + std::to_string(i));
            bool useNewsTex = false;

            // Check if the news thumbnail is valid (not too small like emoji)
            if (newsTex) {
                int texW = 0, texH = 0;
                if (GetTextureSize("news_thumb_" + game->appId + "_" + std::to_string(i), texW, texH)) {
                    // Only use if image is reasonably sized (not emoji/icon)
                    if (texW >= 100 && texH >= 60) {
                        useNewsTex = true;
                    }
                } else {
                    // Can't get size, assume it's okay
                    useNewsTex = true;
                }
            }

            // Fallback to game screenshot if news thumbnail is invalid
            ImTextureID fallbackTex = (ImTextureID)0;
            if (!useNewsTex) {
                // Use game screenshot as fallback
                if (!sd->screenshotUrls.empty()) {
                    fallbackTex = GetTextureByKey("ss_" + game->appId + "_" + std::to_string(i % (int)sd->screenshotUrls.size()));
                }
                if (!fallbackTex) {
                    fallbackTex = GetTextureByKey("hdr_" + game->appId);
                }
                if (!fallbackTex) {
                    fallbackTex = GetGameTexture(game->appId);
                }
            }

            ImTextureID displayTex = useNewsTex ? newsTex : fallbackTex;

            float imgX = nx + 12;
            float imgY = ny + (newsItemH - imgH) / 2;

            if (displayTex) {
                // Draw with cover fit (crop to fill)
                dl->AddImageRounded(displayTex, ImVec2(imgX, imgY),
                    ImVec2(imgX + imgW, imgY + imgH),
                    ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f), IM_COL32(255, 255, 255, 255), 8.0f);
            } else {
                // News icon placeholder
                dl->AddRectFilled(ImVec2(imgX, imgY), ImVec2(imgX + imgW, imgY + imgH),
                    IM_COL32(40, 50, 65, 200), 8.0f);
                DrawIconNews(dl, imgX + imgW / 2, imgY + imgH / 2, imgH * 0.4f,
                    IM_COL32(80, 100, 120, 150), 1.5f);
            }

            // News title - larger font, truncate if too long
            float titleX = imgX + imgW + 14;
            float titleW = singleCardW - imgW - 50;
            float titleFontSize = mainFont->FontSize * 1.05f;

            // Truncate title if too long
            std::string displayTitle = news.title;
            ImVec2 titleSize = mainFont->CalcTextSizeA(titleFontSize, FLT_MAX, 0, displayTitle.c_str());
            if (titleSize.x > titleW) {
                while (!displayTitle.empty() && titleSize.x > titleW - 20) {
                    size_t len = displayTitle.size();
                    while (len > 0 && (displayTitle[len-1] & 0xC0) == 0x80) len--;
                    if (len > 0) len--;
                    displayTitle = displayTitle.substr(0, len);
                    titleSize = mainFont->CalcTextSizeA(titleFontSize, FLT_MAX, 0, displayTitle.c_str());
                }
                displayTitle += "...";
            }
            dl->AddText(mainFont, titleFontSize, ImVec2(titleX, ny + 10),
                        IM_COL32(230, 240, 255, 255), displayTitle.c_str());

            // Content preview - below title with line break, supports wrapping
            if (!news.contents.empty()) {
                float contentFontSize = mainFont->FontSize * 0.82f;
                float contentY = ny + 10 + titleFontSize + 6;  // Below title with gap
                float contentMaxW = titleW;
                float contentMaxH = newsItemH - (contentY - ny) - 28;  // Leave room for feed label

                // Clean content for preview
                std::string preview = news.contents;
                // Remove HTML tags
                size_t pos;
                while ((pos = preview.find('<')) != std::string::npos) {
                    size_t end = preview.find('>', pos);
                    if (end != std::string::npos) preview.erase(pos, end - pos + 1);
                    else break;
                }
                // Replace newlines with spaces for cleaner display
                while ((pos = preview.find('\n')) != std::string::npos) preview[pos] = ' ';
                while ((pos = preview.find('\r')) != std::string::npos) preview[pos] = ' ';
                // Remove multiple spaces
                while ((pos = preview.find("  ")) != std::string::npos) preview.erase(pos, 1);
                // Trim leading/trailing spaces
                while (!preview.empty() && preview[0] == ' ') preview.erase(0, 1);

                // Truncate to reasonable length for multi-line display
                if (preview.length() > 200) {
                    preview = preview.substr(0, 197) + "...";
                }

                // Draw with wrap_width for multi-line support
                dl->AddText(mainFont, contentFontSize, ImVec2(titleX, contentY),
                            IM_COL32(160, 175, 195, 200), preview.c_str(), nullptr, contentMaxW);
            }

            // Feed label at bottom - slightly larger
            if (!news.feedLabel.empty()) {
                float labelFontSize = mainFont->FontSize * 0.85f;
                dl->AddText(mainFont, labelFontSize, ImVec2(titleX, ny + newsItemH - 28),
                            IM_COL32(102, 192, 244, 200), news.feedLabel.c_str());
            }
        }

        contentY += totalNewsH + mainFont->FontSize * 1.3f + 36;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  ACHIEVEMENTS POPUP - Full list of all achievements (Grid layout with glass cards)
    // ═══════════════════════════════════════════════════════════════════════
    if (showAchievementsPopup_ && achievementsPopupAppId_ == game->appId && sd && sd->totalAchievements > 0) {
        // Dim background with blur effect simulation
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        dl->AddRectFilled(ImVec2(0, 0), screenSize, IM_COL32(0, 0, 0, 160));

        // Large popup - almost fullscreen
        float popupW = screenSize.x * 0.92f;
        float popupH = screenSize.y * 0.88f;
        if (popupW > 1600.0f) popupW = 1600.0f;
        if (popupH > 950.0f) popupH = 950.0f;
        float popupX = (screenSize.x - popupW) / 2;
        float popupY = (screenSize.y - popupH) / 2;

        // Popup background with glass effect - multiple layers for depth
        // Outer glow
        dl->AddRectFilled(ImVec2(popupX - 2, popupY - 2), ImVec2(popupX + popupW + 2, popupY + popupH + 2),
            IM_COL32(102, 192, 244, 30), 16.0f);
        // Main background with transparency
        dl->AddRectFilled(ImVec2(popupX, popupY), ImVec2(popupX + popupW, popupY + popupH),
            IM_COL32(15, 22, 35, 240), 14.0f);
        // Inner highlight at top
        dl->AddRectFilledMultiColor(ImVec2(popupX, popupY), ImVec2(popupX + popupW, popupY + 80),
            IM_COL32(40, 60, 90, 100), IM_COL32(40, 60, 90, 100),
            IM_COL32(15, 22, 35, 0), IM_COL32(15, 22, 35, 0));
        // Border
        dl->AddRect(ImVec2(popupX, popupY), ImVec2(popupX + popupW, popupY + popupH),
            IM_COL32(80, 120, 160, 120), 14.0f, 0, 1.5f);

        // Header area
        float headerH = 70.0f;
        dl->AddRectFilled(ImVec2(popupX, popupY), ImVec2(popupX + popupW, popupY + headerH),
            IM_COL32(25, 35, 50, 200), 14.0f, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(popupX + 20, popupY + headerH), ImVec2(popupX + popupW - 20, popupY + headerH),
            IM_COL32(102, 192, 244, 60));

        // Trophy icon
        DrawIconTrophy(dl, popupX + 40, popupY + headerH / 2, 36.0f, IM_COL32(255, 215, 100, 255), 2.5f);

        // Title
        if (g_mainFontLarge) {
            dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize, ImVec2(popupX + 70, popupY + (headerH - g_mainFontLarge->FontSize) / 2),
                IM_COL32(255, 255, 255, 255), u8"游戏成就");
        } else {
            dl->AddText(mainFont, mainFont->FontSize * 1.4f, ImVec2(popupX + 70, popupY + (headerH - mainFont->FontSize * 1.4f) / 2),
                IM_COL32(255, 255, 255, 255), u8"游戏成就");
        }

        // Achievement count badge
        char totalBuf[64];
        snprintf(totalBuf, sizeof(totalBuf), u8"共 %d 项成就", sd->totalAchievements);
        float titleEndX = popupX + 70 + (g_mainFontLarge ? g_mainFontLarge->CalcTextSizeA(g_mainFontLarge->FontSize, FLT_MAX, 0, u8"游戏成就").x : 150);
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(titleEndX + 20, popupY + (headerH - smallFont->FontSize) / 2 + 2),
            IM_COL32(140, 170, 200, 200), totalBuf);

        // Close button (X)
        float closeBtnX = popupX + popupW - 55;
        float closeBtnY = popupY + (headerH - 36) / 2;
        ImGui::SetCursorScreenPos(ImVec2(closeBtnX, closeBtnY));
        ImGui::PushID("close_ach_popup");
        ImGui::InvisibleButton("##close", ImVec2(36, 36));
        bool closeHov = ImGui::IsItemHovered();
        bool closeClk = ImGui::IsItemClicked();
        ImGui::PopID();

        // Close button visual
        float cx = closeBtnX + 18, cy = closeBtnY + 18;
        if (closeHov) {
            dl->AddCircleFilled(ImVec2(cx, cy), 18, IM_COL32(200, 60, 60, 150));
        }
        ImU32 closeColor = closeHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 170, 180, 200);
        dl->AddLine(ImVec2(cx - 8, cy - 8), ImVec2(cx + 8, cy + 8), closeColor, 2.5f);
        dl->AddLine(ImVec2(cx + 8, cy - 8), ImVec2(cx - 8, cy + 8), closeColor, 2.5f);

        if (closeClk) {
            showAchievementsPopup_ = false;
        }

        // Grid content area
        float contentY = popupY + headerH + 20;
        float contentH = popupH - headerH - 40;
        float contentW = popupW - 40;
        float contentX = popupX + 20;

        // Card dimensions - wider cards for horizontal layout
        float cardW = 280.0f;
        float cardH = 110.0f;
        float cardGapX = 16.0f;
        float cardGapY = 14.0f;
        float iconSize = 72.0f;

        // Calculate columns that fit
        int cols = (int)((contentW + cardGapX) / (cardW + cardGapX));
        if (cols < 1) cols = 1;
        if (cols > 5) cols = 5;

        // Recalculate card width to fill space evenly
        cardW = (contentW - (cols - 1) * cardGapX) / cols;

        // Scrollable grid
        ImGui::SetCursorScreenPos(ImVec2(contentX, contentY));
        ImGui::BeginChild("##ach_grid_scroll", ImVec2(contentW, contentH), false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImDrawList* gridDl = ImGui::GetWindowDrawList();
        int numAch = (int)sd->achievements.size();
        int rows = (numAch + cols - 1) / cols;

        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols; col++) {
                int idx = row * cols + col;
                if (idx >= numAch) break;

                const auto& ach = sd->achievements[idx];

                float cardX = contentX + col * (cardW + cardGapX);
                float cardY = row * (cardH + cardGapY);

                ImVec2 cardMin = ImVec2(cardX, contentY + cardY - ImGui::GetScrollY());
                ImVec2 cardMax = ImVec2(cardX + cardW, contentY + cardY + cardH - ImGui::GetScrollY());

                // Skip if outside visible area
                if (cardMax.y < contentY - 20 || cardMin.y > contentY + contentH + 20) {
                    continue;
                }

                // Glass card background with gradient
                // Base layer
                gridDl->AddRectFilled(cardMin, cardMax, IM_COL32(30, 42, 60, 200), 10.0f);
                // Top highlight gradient
                gridDl->AddRectFilledMultiColor(
                    cardMin, ImVec2(cardMax.x, cardMin.y + cardH * 0.4f),
                    IM_COL32(60, 85, 120, 80), IM_COL32(60, 85, 120, 80),
                    IM_COL32(30, 42, 60, 0), IM_COL32(30, 42, 60, 0));
                // Border with subtle glow
                gridDl->AddRect(cardMin, cardMax, IM_COL32(80, 110, 150, 100), 10.0f, 0, 1.0f);

                // Achievement icon
                float achIconX = cardMin.x + 14;
                float achIconY = cardMin.y + (cardH - iconSize) / 2;
                ImTextureID achTex = GetTextureByKey("ach_" + game->appId + "_" + std::to_string(idx));

                if (achTex) {
                    // Icon with subtle shadow
                    gridDl->AddRectFilled(ImVec2(achIconX + 2, achIconY + 2),
                        ImVec2(achIconX + iconSize + 2, achIconY + iconSize + 2),
                        IM_COL32(0, 0, 0, 60), 8.0f);
                    gridDl->AddImageRounded(achTex, ImVec2(achIconX, achIconY),
                        ImVec2(achIconX + iconSize, achIconY + iconSize),
                        ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);
                } else {
                    // Placeholder
                    gridDl->AddRectFilled(ImVec2(achIconX, achIconY),
                        ImVec2(achIconX + iconSize, achIconY + iconSize),
                        IM_COL32(40, 55, 75, 200), 8.0f);
                    DrawIconTrophy(gridDl, achIconX + iconSize / 2, achIconY + iconSize / 2,
                        iconSize * 0.45f, IM_COL32(160, 140, 80, 150), 1.5f);
                }

                // Text area
                float textX = achIconX + iconSize + 14;
                float textW = cardW - iconSize - 42;

                // Achievement name (truncate if too long)
                std::string displayName = ach.displayName;
                ImVec2 nameSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, 0, displayName.c_str());
                if (nameSz.x > textW) {
                    // Truncate with ellipsis
                    while (displayName.length() > 3 && mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, 0, (displayName + "...").c_str()).x > textW) {
                        displayName.pop_back();
                    }
                    displayName += "...";
                }
                gridDl->AddText(mainFont, mainFont->FontSize, ImVec2(textX, cardMin.y + 14),
                    IM_COL32(240, 245, 255, 255), displayName.c_str());

                // Achievement description (2 lines max)
                if (!ach.description.empty()) {
                    gridDl->AddText(smallFont, smallFont->FontSize, ImVec2(textX, cardMin.y + 14 + mainFont->FontSize + 4),
                        IM_COL32(150, 170, 190, 200), ach.description.c_str(), nullptr, textW);
                }

                // Rarity indicator at bottom right
                float rarityY = cardMax.y - 24;

                // Percentage with color based on rarity
                char percentBuf[32];
                snprintf(percentBuf, sizeof(percentBuf), "%.1f%%", ach.globalPercent);
                ImU32 percentColor = ach.globalPercent < 5 ? IM_COL32(255, 215, 0, 255) :    // Gold - ultra rare
                                    ach.globalPercent < 15 ? IM_COL32(192, 192, 192, 255) : // Silver - rare
                                    ach.globalPercent < 40 ? IM_COL32(205, 127, 50, 255) :  // Bronze - uncommon
                                                             IM_COL32(120, 140, 160, 200);  // Common

                ImVec2 pctSz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, percentBuf);
                float pctX = cardMax.x - pctSz.x - 14;
                gridDl->AddText(smallFont, smallFont->FontSize, ImVec2(pctX, rarityY),
                    percentColor, percentBuf);

                // Mini progress bar
                float barW = 50.0f;
                float barH = 3.0f;
                float barX = pctX - barW - 8;
                float barY = rarityY + smallFont->FontSize / 2 - barH / 2;
                gridDl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                    IM_COL32(20, 30, 45, 200), 2.0f);
                gridDl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * (ach.globalPercent / 100.0f), barY + barH),
                    percentColor, 2.0f);

                // Rarity label
                const char* rarityLabel = ach.globalPercent < 5 ? u8"极稀有" :
                                         ach.globalPercent < 15 ? u8"稀有" :
                                         ach.globalPercent < 40 ? u8"少见" : u8"常见";
                ImVec2 labelSz = smallFont->CalcTextSizeA(smallFont->FontSize * 0.9f, FLT_MAX, 0, rarityLabel);
                gridDl->AddText(smallFont, smallFont->FontSize * 0.9f, ImVec2(barX - labelSz.x - 8, rarityY + 1),
                    IM_COL32(100, 120, 140, 180), rarityLabel);
            }
        }

        // Add spacing at bottom for scroll
        float totalGridH = rows * (cardH + cardGapY);
        ImGui::Dummy(ImVec2(contentW, totalGridH));

        ImGui::EndChild();

        // Click outside to close
        ImGui::SetCursorScreenPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##popup_bg", screenSize);
        if (ImGui::IsItemClicked() && !ImGui::IsMouseHoveringRect(ImVec2(popupX, popupY), ImVec2(popupX + popupW, popupY + popupH))) {
            showAchievementsPopup_ = false;
        }
    }

    // Set scroll height
    ImGui::SetCursorPosY(contentY + 50);
}

// ====================== StorePage ======================

const StorePage::StoreGame* StorePage::FindGameByAppId(const std::string& appId) const {
    for (auto& g : featured_)      if (g.appId == appId) return &g;
    for (auto& g : specials_)      if (g.appId == appId) return &g;
    for (auto& g : newReleases_)   if (g.appId == appId) return &g;
    for (auto& g : trending_)      if (g.appId == appId) return &g;
    for (auto& g : comingSoon_)    if (g.appId == appId) return &g;
    for (auto& g : searchResults_) if (g.appId == appId) return &g;
    for (auto& g : browseGames_)   if (g.appId == appId) return &g;
    return nullptr;
}

void StorePage::SelectGame(const std::string& appId) {
    fprintf(stderr, "[DETAIL] SelectGame called with appId: %s\n", appId.c_str());
    if (playingVideo_) { StopVideo(); playingVideo_ = false; playingMovieIdx_ = -1; }
    selectedAppId_ = appId;
    showDetail_ = true;
    selectedSSIdx_ = 0;
    detailTexturesRequested_ = false;
    RequestGameTexture(appId);
    RequestStoreData(appId);
}

StorePage::StorePage() {
    // Request Steam store listings on construction
    RequestStoreListings();

    // 初始化模拟统计数据
    onlineUsers_ = 1234 + rand() % 500;
    totalAccounts_ = 8888;
    idleAccounts_ = 3456 + rand() % 200;
}

void StorePage::UpdateFromSteamListings() {
    if (storeListingsLoaded_) return;

    const SteamStoreListings* listings = GetStoreListings();
    if (!listings) return;

    storeListingsLoaded_ = true;

    auto convert = [](const std::vector<SteamListingGame>& src) {
        std::vector<StoreGame> out;
        for (auto& g : src) {
            StoreGame sg;
            sg.name = g.name;
            sg.appId = g.appId;
            sg.price = g.price;
            sg.originalPrice = g.originalPrice;
            sg.discount = g.discount;
            sg.tags = g.tags;
            out.push_back(std::move(sg));

            // Request header image from API URL (more reliable than CDN pattern)
            if (!g.headerImage.empty() && !g.appId.empty()) {
                RequestTextureFromUrl("hdr_" + g.appId, g.headerImage);
            }
            // Also request CDN pattern as fallback
            if (!g.appId.empty()) {
                RequestGameTexture(g.appId);
            }
        }
        return out;
    };

    if (!listings->featured.empty())
        featured_ = convert(listings->featured);
    if (!listings->specials.empty())
        specials_ = convert(listings->specials);
    if (!listings->newReleases.empty())
        newReleases_ = convert(listings->newReleases);
    if (!listings->topSellers.empty())
        trending_ = convert(listings->topSellers);
    if (!listings->comingSoon.empty())
        comingSoon_ = convert(listings->comingSoon);

    // Clamp featured index
    if (!featured_.empty() && featuredIdx_ >= (int)featured_.size())
        featuredIdx_ = 0;

    // Request detailed store data for all visible games (tags, reviews, etc.)
    auto requestDetails = [](const std::vector<StoreGame>& games, int maxCount) {
        for (int i = 0; i < std::min(maxCount, (int)games.size()); i++) {
            if (!games[i].appId.empty())
                RequestStoreData(games[i].appId);
        }
    };
    requestDetails(featured_, 10);
    requestDetails(specials_, 9);
    requestDetails(newReleases_, 8);
    requestDetails(trending_, 8);
    requestDetails(comingSoon_, 4);

    fprintf(stderr, "[UI] Store listings loaded: featured=%d specials=%d new=%d trending=%d coming=%d\n",
            (int)featured_.size(), (int)specials_.size(),
            (int)newReleases_.size(), (int)trending_.size(), (int)comingSoon_.size());
}

void StorePage::RenderFeaturedBanner(ImDrawList* dl, float x, float y, float width) {
    float bannerH = 380.0f;
    float sideW = 220.0f;
    float mainW = width - sideW - 16;
    float arrowSize = 44.0f;

    featuredTimer_ += ImGui::GetIO().DeltaTime;
    if (featuredTimer_ > 5.0f) {
        featuredTimer_ = 0;
        featuredIdx_ = (featuredIdx_ + 1) % (int)featured_.size();
    }

    auto& feat = featured_[featuredIdx_ % featured_.size()];

    // Modern shadow with blur effect
    for (int i = 20; i > 0; i -= 2) {
        float a = 15.0f * ((20.0f - i) / 20.0f);
        dl->AddRectFilled(ImVec2(x - i, y - i), ImVec2(x + mainW + i, y + bannerH + i),
                          IM_COL32(0, 0, 0, (int)a), 12.0f);
    }

    // Main banner image with rounded corners
    ImTextureID tex = GetBestTexture(feat.appId);
    if (tex) {
        dl->AddImageRounded(tex, ImVec2(x, y), ImVec2(x + mainW, y + bannerH),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 12.0f);
    } else {
        DrawProceduralCover(dl, x, y, mainW, bannerH, feat.appId);
    }

    // NOTE: Banner click detection moved AFTER arrow buttons to avoid blocking them

    // Modern label with glass effect
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 180, y + 32), IM_COL32(0, 0, 0, 150), 12.0f, ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomRight);
    if (g_mainFontSmall)
        dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                    ImVec2(x + 14, y + 8), IM_COL32(255, 255, 255, 220),
                    u8"\u7CBE\u9009\u4E0E\u63A8\u8350"); // 精选与推荐

    // Modern gradient overlay at bottom
    dl->AddRectFilledMultiColor(ImVec2(x, y + bannerH - 140), ImVec2(x + mainW, y + bannerH),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(15, 20, 30, 250), IM_COL32(15, 20, 30, 250));

    // Use real data from SteamStoreData if available
    const SteamStoreData* featSd = GetStoreData(feat.appId);
    const char* featName = (featSd && !featSd->name.empty()) ? featSd->name.c_str() : feat.name.c_str();
    std::string featTags;
    if (featSd && !featSd->genres.empty()) {
        for (int gi = 0; gi < std::min(4, (int)featSd->genres.size()); gi++) {
            if (!featTags.empty()) featTags += u8"\u3001"; // 、
            featTags += featSd->genres[gi];
        }
    } else {
        featTags = feat.tags;
    }
    const char* featPrice = (featSd && !featSd->priceFormatted.empty()) ? featSd->priceFormatted.c_str() : feat.price.c_str();

    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize * 1.3f,
                    ImVec2(x + 24, y + bannerH - 95), IM_COL32(255, 255, 255, 255), featName);

    // Tags + review label
    float tagY = y + bannerH - 55;
    dl->AddText(ImVec2(x + 26, tagY), IM_COL32(180, 200, 220, 200), featTags.c_str());
    if (featSd && !featSd->reviewDesc.empty()) {
        ImVec2 tagSz = ImGui::CalcTextSize(featTags.c_str());
        float revX = x + 26 + tagSz.x + 16;
        ImU32 revCol = featSd->reviewScore >= 80 ? IM_COL32(102, 192, 244, 220) :
                       featSd->reviewScore >= 60 ? IM_COL32(180, 200, 120, 220) :
                                                    IM_COL32(200, 160, 60, 220);
        dl->AddText(ImVec2(revX, tagY), revCol, featSd->reviewDesc.c_str());
    }

    // Modern price badge
    ImVec2 ps = ImGui::CalcTextSize(featPrice);
    float priceX = x + 24;
    float priceY = y + bannerH - 32;
    dl->AddRectFilled(ImVec2(priceX, priceY), ImVec2(priceX + ps.x + 28, priceY + 24),
                      IM_COL32(76, 107, 34, 255), 6.0f);
    dl->AddText(ImVec2(priceX + 14, priceY + 4), IM_COL32(190, 230, 20, 255), featPrice);

    // Modern border
    dl->AddRect(ImVec2(x, y), ImVec2(x + mainW, y + bannerH), IM_COL32(255, 255, 255, 20), 12.0f, 0, 1.5f);

    // Modern navigation arrows on main banner
    float arrowY = y + (bannerH - arrowSize) / 2;

    // Left arrow
    float leftArrowX = x + 16;
    ImGui::SetCursorScreenPos(ImVec2(leftArrowX, arrowY));
    ImGui::PushID("feat_left");
    ImGui::InvisibleButton("##fl", ImVec2(arrowSize, arrowSize));
    bool leftHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        featuredIdx_ = (featuredIdx_ - 1 + (int)featured_.size()) % (int)featured_.size();
        featuredTimer_ = 0;
    }
    ImGui::PopID();

    // Modern circular arrow button - left
    dl->AddCircleFilled(ImVec2(leftArrowX + arrowSize/2, arrowY + arrowSize/2), arrowSize/2,
                        leftHov ? IM_COL32(255, 255, 255, 60) : IM_COL32(0, 0, 0, 120));
    dl->AddCircle(ImVec2(leftArrowX + arrowSize/2, arrowY + arrowSize/2), arrowSize/2,
                  IM_COL32(255, 255, 255, leftHov ? 180 : 80), 0, 2.0f);
    // Draw < arrow
    float ac = leftArrowX + arrowSize/2;
    float ay = arrowY + arrowSize/2;
    dl->AddLine(ImVec2(ac + 4, ay - 10), ImVec2(ac - 6, ay), IM_COL32(255, 255, 255, leftHov ? 255 : 180), 2.5f);
    dl->AddLine(ImVec2(ac - 6, ay), ImVec2(ac + 4, ay + 10), IM_COL32(255, 255, 255, leftHov ? 255 : 180), 2.5f);

    // Right arrow
    float rightArrowX = x + mainW - arrowSize - 16;
    ImGui::SetCursorScreenPos(ImVec2(rightArrowX, arrowY));
    ImGui::PushID("feat_right");
    ImGui::InvisibleButton("##fr", ImVec2(arrowSize, arrowSize));
    bool rightHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        featuredIdx_ = (featuredIdx_ + 1) % (int)featured_.size();
        featuredTimer_ = 0;
    }
    ImGui::PopID();

    // Modern circular arrow button - right
    dl->AddCircleFilled(ImVec2(rightArrowX + arrowSize/2, arrowY + arrowSize/2), arrowSize/2,
                        rightHov ? IM_COL32(255, 255, 255, 60) : IM_COL32(0, 0, 0, 120));
    dl->AddCircle(ImVec2(rightArrowX + arrowSize/2, arrowY + arrowSize/2), arrowSize/2,
                  IM_COL32(255, 255, 255, rightHov ? 180 : 80), 0, 2.0f);
    // Draw > arrow
    ac = rightArrowX + arrowSize/2;
    ay = arrowY + arrowSize/2;
    dl->AddLine(ImVec2(ac - 4, ay - 10), ImVec2(ac + 6, ay), IM_COL32(255, 255, 255, rightHov ? 255 : 180), 2.5f);
    dl->AddLine(ImVec2(ac + 6, ay), ImVec2(ac - 4, ay + 10), IM_COL32(255, 255, 255, rightHov ? 255 : 180), 2.5f);

    // Dot indicators at bottom center
    int numDots = std::min(6, (int)featured_.size());
    float dotSpacing = 16.0f;
    float dotsW = numDots * dotSpacing;
    float dotsX = x + (mainW - dotsW) / 2;
    float dotsY = y + bannerH - 16;
    for (int i = 0; i < numDots; i++) {
        float dotX = dotsX + i * dotSpacing + dotSpacing/2;
        bool isActive = (i == featuredIdx_ % numDots);

        // Clickable dot indicator
        ImGui::SetCursorScreenPos(ImVec2(dotX - 8, dotsY - 8));
        char dotId[16]; snprintf(dotId, sizeof(dotId), "dot_%d", i);
        ImGui::PushID(dotId);
        ImGui::InvisibleButton("##dot", ImVec2(16, 16));
        if (ImGui::IsItemClicked()) {
            featuredIdx_ = i;
            featuredTimer_ = 0;
        }
        ImGui::PopID();

        if (isActive) {
            dl->AddCircleFilled(ImVec2(dotX, dotsY), 5.0f, IM_COL32(102, 192, 244, 255));
        } else {
            dl->AddCircleFilled(ImVec2(dotX, dotsY), 4.0f, IM_COL32(255, 255, 255, 100));
        }
    }

    // Banner click detection (AFTER arrows and dots to not block them)
    // Only trigger if click is not on arrow buttons or dot indicators
    ImVec2 mousePos = ImGui::GetMousePos();
    bool onLeftArrow = (mousePos.x >= leftArrowX && mousePos.x <= leftArrowX + arrowSize &&
                        mousePos.y >= arrowY && mousePos.y <= arrowY + arrowSize);
    bool onRightArrow = (mousePos.x >= rightArrowX && mousePos.x <= rightArrowX + arrowSize &&
                         mousePos.y >= arrowY && mousePos.y <= arrowY + arrowSize);
    bool onDots = (mousePos.x >= dotsX - 8 && mousePos.x <= dotsX + dotsW + 8 &&
                   mousePos.y >= dotsY - 10 && mousePos.y <= dotsY + 10);

    if (!onLeftArrow && !onRightArrow && !onDots) {
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::PushID("feat_banner_click");
        ImGui::InvisibleButton("##banner", ImVec2(mainW, bannerH));
        bool bannerHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            SelectGame(feat.appId);
        }
        if (bannerHov) {
            dl->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + mainW + 1, y + bannerH + 1),
                        IM_COL32(102, 192, 244, 80), 12.0f, 0, 2.0f);
        }
        ImGui::PopID();
    }

    // Side thumbnails with modern style
    float thumbH = (bannerH - 24) / 4.0f;
    float thumbX = x + mainW + 16;
    for (int i = 0; i < (int)featured_.size() && i < 4; i++) {
        float ty2 = y + i * (thumbH + 8);
        bool isActive = (i == featuredIdx_);

        // Clickable thumbnail
        bool thumbHov = false;
        char tid[16]; snprintf(tid, sizeof(tid), "ft_%d", i);
        ImGui::SetCursorScreenPos(ImVec2(thumbX, ty2));
        ImGui::PushID(tid);
        ImGui::InvisibleButton("##ft", ImVec2(sideW, thumbH));
        thumbHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) { featuredIdx_ = i; featuredTimer_ = 0; }
        ImGui::PopID();

        // Modern shadow
        for (int s = 8; s > 0; s -= 2) {
            float a = 10.0f * ((8.0f - s) / 8.0f);
            dl->AddRectFilled(ImVec2(thumbX - s, ty2 - s), ImVec2(thumbX + sideW + s, ty2 + thumbH + s),
                              IM_COL32(0, 0, 0, (int)a), 8.0f);
        }

        ImTextureID ttex = GetBestTexture(featured_[i].appId);
        if (ttex) {
            dl->AddImageRounded(ttex, ImVec2(thumbX, ty2), ImVec2(thumbX + sideW, ty2 + thumbH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, isActive ? 255 : 200), 8.0f);
        } else {
            DrawProceduralCover(dl, thumbX, ty2, sideW, thumbH, featured_[i].appId);
        }

        // Modern gradient overlay
        dl->AddRectFilledMultiColor(ImVec2(thumbX, ty2 + thumbH - 28), ImVec2(thumbX + sideW, ty2 + thumbH),
            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 220), IM_COL32(0, 0, 0, 220));
        if (g_mainFontSmall)
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                        ImVec2(thumbX + 8, ty2 + thumbH - 20), IM_COL32(255, 255, 255, 240), featured_[i].name.c_str());

        // Modern selection/hover border
        if (isActive) {
            dl->AddRect(ImVec2(thumbX - 2, ty2 - 2), ImVec2(thumbX + sideW + 2, ty2 + thumbH + 2),
                        IM_COL32(102, 192, 244, 255), 8.0f, 0, 3.0f);
        } else if (thumbHov) {
            dl->AddRect(ImVec2(thumbX - 1, ty2 - 1), ImVec2(thumbX + sideW + 1, ty2 + thumbH + 1),
                        IM_COL32(102, 192, 244, 150), 8.0f, 0, 2.0f);
        } else {
            dl->AddRect(ImVec2(thumbX, ty2), ImVec2(thumbX + sideW, ty2 + thumbH), IM_COL32(255, 255, 255, 20), 8.0f);
        }
    }
}

void StorePage::RenderSpecialOffers(ImDrawList* dl, float x, float y, float width) {
    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 240),
                    u8"\u7279\u60E0\u4FC3\u9500"); // 特惠促销

    // "Browse More" clickable
    ImGui::SetCursorScreenPos(ImVec2(x + width - 100, y));
    ImGui::PushID("browse_more");
    ImGui::InvisibleButton("##bm", ImVec2(100, 20));
    bool bmHov = ImGui::IsItemHovered();
    ImGui::PopID();
    dl->AddText(ImVec2(x + width - 100, y + 4),
                IM_COL32(102, 192, 244, bmHov ? 255 : 200),
                u8"\u67E5\u770B\u66F4\u591A >"); // 查看更多 >
    y += 35;

    float cardW = (width - 24) / 3.0f;  // Slightly wider cards
    float cardH = 150.0f;  // Increased from 120 to 150 for larger cards
    float gap = 14.0f;  // Increased gap

    for (int i = 0; i < std::min(9, (int)specials_.size()); i++) {
        int row = i / 3, col = i % 3;
        float cx = x + col * (cardW + gap);
        float cy = y + row * (cardH + gap);
        auto& s = specials_[i];

        // Request store data if not already loaded
        const SteamStoreData* sSd = GetStoreData(s.appId);
        if (!sSd && !IsStoreDataLoading(s.appId)) {
            RequestStoreData(s.appId);
        }

        bool cardHov = false;
        char cid[16]; snprintf(cid, sizeof(cid), "sp_%d", i);
        if (CardButton(cid, cx, cy, cardW, cardH, cardHov, dl))
            SelectGame(s.appId);

        DrawShadow(dl, cx, cy, cardW, cardH, 5.0f);
        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                          IM_COL32(22, 32, 45, 255), 8.0f);

        // Cover image - use 16:9 aspect ratio for better quality display
        float coverW = cardH * 1.4f;  // Adjusted for better proportions
        ImTextureID stex = GetBestTexture(s.appId);
        if (stex) {
            dl->AddImageRounded(stex, ImVec2(cx, cy), ImVec2(cx + coverW, cy + cardH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);
        } else {
            DrawProceduralCover(dl, cx, cy, coverW, cardH, s.appId);
        }

        float textX = cx + coverW + 14;
        float maxTextW = cardW - coverW - 20;

        // Use real name from store data if available
        const char* gameName = (sSd && !sSd->name.empty()) ? sSd->name.c_str() : s.name.c_str();

        // Name (clip to card width) - larger font for bigger cards
        ImGui::PushClipRect(ImVec2(textX, cy), ImVec2(cx + cardW - 4, cy + cardH), true);
        if (g_mainFont)
            dl->AddText(g_mainFont, g_mainFont->FontSize,
                        ImVec2(textX, cy + 10), IM_COL32(255, 255, 255, 240), gameName);
        else
            dl->AddText(ImVec2(textX, cy + 10), IM_COL32(255, 255, 255, 240), gameName);

        // Tags from real data
        if (g_mainFontSmall) {
            std::string sTags;
            if (sSd && !sSd->genres.empty()) {
                for (int gi = 0; gi < std::min(3, (int)sSd->genres.size()); gi++) {
                    if (!sTags.empty()) sTags += u8"\u3001";
                    sTags += sSd->genres[gi];
                }
            }
            if (!sTags.empty())
                dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                            ImVec2(textX, cy + 34), IM_COL32(140, 160, 180, 180), sTags.c_str());
        }

        // Review info - moved down for larger cards
        if (sSd && !sSd->reviewDesc.empty() && g_mainFontSmall) {
            ImU32 revCol = sSd->reviewScore >= 80 ? IM_COL32(102, 192, 244, 220) :
                           sSd->reviewScore >= 60 ? IM_COL32(180, 200, 120, 220) :
                                                     IM_COL32(200, 160, 60, 220);
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                        ImVec2(textX, cy + 56), revCol, sSd->reviewDesc.c_str());
        }
        ImGui::PopClipRect();

        // Price / discount - adjusted for larger cards
        if (!s.discount.empty()) {
            float priceY2 = cy + cardH - 36;
            ImVec2 discSize = ImGui::CalcTextSize(s.discount.c_str());
            float discW = discSize.x + 16;
            dl->AddRectFilled(ImVec2(textX, priceY2), ImVec2(textX + discW, priceY2 + 28),
                              IM_COL32(76, 107, 34, 255), 5.0f);
            dl->AddText(ImVec2(textX + 8, priceY2 + 6), IM_COL32(190, 230, 20, 255), s.discount.c_str());

            // Original price (strikethrough) + final price
            const char* origP = (!s.originalPrice.empty()) ? s.originalPrice.c_str() :
                                (sSd && !sSd->originalPrice.empty()) ? sSd->originalPrice.c_str() : nullptr;
            const char* finalP = (sSd && !sSd->priceFormatted.empty()) ? sSd->priceFormatted.c_str() : s.price.c_str();
            float afterDiscX = textX + discW + 10;
            if (origP) {
                ImVec2 ops = ImGui::CalcTextSize(origP);
                if (g_mainFontSmall)
                    dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                                ImVec2(afterDiscX, priceY2 + 4), IM_COL32(140, 160, 180, 160), origP);
                dl->AddLine(ImVec2(afterDiscX, priceY2 + 12), ImVec2(afterDiscX + ops.x, priceY2 + 12),
                            IM_COL32(140, 160, 180, 160), 1.0f);
                afterDiscX += ops.x + 8;
            }
            dl->AddText(ImVec2(afterDiscX, priceY2 + 6), IM_COL32(190, 230, 20, 255), finalP);
        } else {
            float priceY2 = cy + cardH - 32;
            const char* realPrice = (sSd && !sSd->priceFormatted.empty()) ? sSd->priceFormatted.c_str() : s.price.c_str();
            dl->AddText(ImVec2(textX, priceY2), IM_COL32(190, 230, 20, 255), realPrice);
        }

        dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH), IM_COL32(255, 255, 255, 10), 8.0f);
    }
}

void StorePage::RenderCategories(ImDrawList* dl, float x, float y, float width) {
    // Category with representative game appId for background image
    struct CatEntry {
        const char* label;
        const char* searchTerm;
        const char* appId;
    };
    static CatEntry cats[] = {
        // Page 1
        { u8"\u514D\u8D39\u6E38\u620F",   "free to play",   "570" },      // Dota 2
        { u8"\u62A2\u5148\u4F53\u9A8C",   "early access",   "892970" },   // Valheim
        { u8"\u52A8\u4F5C",               "action",         "1174180" },  // Red Dead Redemption 2
        { u8"\u5192\u9669",               "adventure",      "1245620" },  // Elden Ring
        // Page 2
        { u8"\u7B56\u7565",               "strategy",       "1086940" },  // Baldur's Gate 3
        { u8"RPG",                        "RPG",            "1091500" },  // Cyberpunk 2077
        { u8"\u6A21\u62DF",               "simulation",     "255710" },   // Cities: Skylines
        { u8"\u5C04\u51FB",               "shooter",        "1172470" },  // Apex Legends
        // Page 3
        { u8"\u6050\u6016",               "horror",         "381210" },   // Dead by Daylight
        { u8"\u751F\u5B58",               "survival",       "346110" },   // ARK
        { u8"\u5F00\u653E\u4E16\u754C",   "open world",     "271590" },   // GTA V
        { u8"\u591A\u4EBA\u6E38\u620F",   "multiplayer",    "252490" },   // Rust
        // Page 4
        { u8"\u5355\u4EBA\u6E38\u620F",   "singleplayer",   "1151640" },  // Horizon Zero Dawn
        { u8"\u72EC\u7ACB\u6E38\u620F",   "indie",          "413150" },   // Stardew Valley
        { u8"\u8D5B\u8F66",               "racing",         "1222670" },  // Forza Horizon 5
        { u8"\u4F53\u80B2",               "sports",         "1811260" },  // EA SPORTS FC 24
    };
    int numCats = sizeof(cats) / sizeof(cats[0]);
    int catsPerPage = 4;
    int totalPages = (numCats + catsPerPage - 1) / catsPerPage;

    // Request textures for all category images
    for (auto& cat : cats) {
        if (cat.appId && cat.appId[0]) {
            ImTextureID tex = GetBestTexture(cat.appId);
            if (!tex) {
                RequestGameTexture(cat.appId);
            }
        }
    }

    // Title and navigation arrows
    float arrowSize = 36.0f;
    float titleY = y;

    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, titleY), IM_COL32(255, 255, 255, 240),
                    u8"\u6309\u7C7B\u522B\u6D4F\u89C8"); // 按类别浏览

    // Page indicator text (e.g., "1 / 4")
    char pageText[32];
    snprintf(pageText, sizeof(pageText), "%d / %d", categoryPage_ + 1, totalPages);
    ImVec2 pageTextSize = ImGui::CalcTextSize(pageText);
    float pageTextX = x + width - 90 - pageTextSize.x;
    dl->AddText(ImVec2(pageTextX, titleY + 4), IM_COL32(140, 160, 180, 200), pageText);

    // Right arrow
    float rightArrowX = x + width - arrowSize;
    float arrowY = titleY - 2;
    ImGui::SetCursorScreenPos(ImVec2(rightArrowX, arrowY));
    ImGui::PushID("cat_right");
    ImGui::InvisibleButton("##cr", ImVec2(arrowSize, arrowSize));
    bool rightHov = ImGui::IsItemHovered();
    bool canGoRight = (categoryPage_ < totalPages - 1);
    if (ImGui::IsItemClicked() && canGoRight) {
        categoryPage_++;
    }
    ImGui::PopID();

    // Draw right arrow button
    ImU32 rightBg = canGoRight ? (rightHov ? IM_COL32(60, 80, 100, 255) : IM_COL32(40, 55, 75, 255))
                               : IM_COL32(30, 40, 55, 150);
    ImU32 rightArrowCol = canGoRight ? (rightHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 210, 220, 220))
                                     : IM_COL32(100, 110, 120, 100);
    dl->AddRectFilled(ImVec2(rightArrowX, arrowY), ImVec2(rightArrowX + arrowSize, arrowY + arrowSize),
                      rightBg, arrowSize * 0.5f);
    if (canGoRight && rightHov) {
        dl->AddRect(ImVec2(rightArrowX, arrowY), ImVec2(rightArrowX + arrowSize, arrowY + arrowSize),
                    IM_COL32(102, 192, 244, 150), arrowSize * 0.5f);
    }
    // Draw > arrow
    float rcx = rightArrowX + arrowSize * 0.5f;
    float rcy = arrowY + arrowSize * 0.5f;
    dl->AddLine(ImVec2(rcx - 5, rcy - 8), ImVec2(rcx + 5, rcy), rightArrowCol, 2.5f);
    dl->AddLine(ImVec2(rcx + 5, rcy), ImVec2(rcx - 5, rcy + 8), rightArrowCol, 2.5f);

    // Left arrow
    float leftArrowX = rightArrowX - arrowSize - 8;
    ImGui::SetCursorScreenPos(ImVec2(leftArrowX, arrowY));
    ImGui::PushID("cat_left");
    ImGui::InvisibleButton("##cl", ImVec2(arrowSize, arrowSize));
    bool leftHov = ImGui::IsItemHovered();
    bool canGoLeft = (categoryPage_ > 0);
    if (ImGui::IsItemClicked() && canGoLeft) {
        categoryPage_--;
    }
    ImGui::PopID();

    // Draw left arrow button
    ImU32 leftBg = canGoLeft ? (leftHov ? IM_COL32(60, 80, 100, 255) : IM_COL32(40, 55, 75, 255))
                             : IM_COL32(30, 40, 55, 150);
    ImU32 leftArrowCol = canGoLeft ? (leftHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 210, 220, 220))
                                   : IM_COL32(100, 110, 120, 100);
    dl->AddRectFilled(ImVec2(leftArrowX, arrowY), ImVec2(leftArrowX + arrowSize, arrowY + arrowSize),
                      leftBg, arrowSize * 0.5f);
    if (canGoLeft && leftHov) {
        dl->AddRect(ImVec2(leftArrowX, arrowY), ImVec2(leftArrowX + arrowSize, arrowY + arrowSize),
                    IM_COL32(102, 192, 244, 150), arrowSize * 0.5f);
    }
    // Draw < arrow
    float lcx = leftArrowX + arrowSize * 0.5f;
    float lcy = arrowY + arrowSize * 0.5f;
    dl->AddLine(ImVec2(lcx + 5, lcy - 8), ImVec2(lcx - 5, lcy), leftArrowCol, 2.5f);
    dl->AddLine(ImVec2(lcx - 5, lcy), ImVec2(lcx + 5, lcy + 8), leftArrowCol, 2.5f);

    y += 40;

    // Card layout: 4 cards in a single row
    float cardW = (width - 30) / 4.0f;
    float cardH = 140.0f;  // Increased from 110 to 140
    float gap = 10.0f;

    int startIdx = categoryPage_ * catsPerPage;
    int endIdx = std::min(startIdx + catsPerPage, numCats);

    for (int i = startIdx; i < endIdx; i++) {
        int col = i - startIdx;
        float cx = x + col * (cardW + gap);
        float cy = y;
        auto& cat = cats[i];

        // Clickable card
        ImGui::SetCursorScreenPos(ImVec2(cx, cy));
        ImGui::PushID(cat.label);
        ImGui::InvisibleButton("##cat", ImVec2(cardW, cardH));
        bool catHov = ImGui::IsItemHovered();
        bool catClk = ImGui::IsItemClicked();
        ImGui::PopID();

        // Shadow
        DrawShadow(dl, cx, cy, cardW, cardH, 4.0f);

        // Background image
        ImTextureID tex = GetBestTexture(cat.appId);
        if (tex) {
            dl->AddImageRounded(tex, ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                                 ImVec2(0, 0), ImVec2(1, 1),
                                 IM_COL32(255, 255, 255, catHov ? 255 : 200), 8.0f);
            dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                IM_COL32(0, 0, 0, catHov ? 80 : 120), IM_COL32(0, 0, 0, catHov ? 80 : 120),
                IM_COL32(0, 0, 0, catHov ? 140 : 180), IM_COL32(0, 0, 0, catHov ? 140 : 180));
        } else {
            unsigned hash = 0;
            for (const char* p = cat.label; *p; p++) hash = hash * 31 + (unsigned char)*p;
            ImU32 c1 = IM_COL32(25 + (hash % 35), 45 + ((hash >> 8) % 45), 75 + ((hash >> 16) % 45), 255);
            ImU32 c2 = IM_COL32(15 + (hash % 25), 30 + ((hash >> 8) % 35), 55 + ((hash >> 16) % 35), 255);
            dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH), c1, c1, c2, c2);
        }

        // Hover border
        if (catHov) {
            dl->AddRect(ImVec2(cx - 1, cy - 1), ImVec2(cx + cardW + 1, cy + cardH + 1),
                        IM_COL32(102, 192, 244, 220), 8.0f, 0, 2.5f);
        } else {
            dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                        IM_COL32(255, 255, 255, 15), 8.0f);
        }

        // Category label
        ImFont* labelFont = g_mainFontLarge ? g_mainFontLarge : ImGui::GetFont();
        ImVec2 labelSize = labelFont->CalcTextSizeA(labelFont->FontSize, FLT_MAX, 0, cat.label);
        float labelX = cx + (cardW - labelSize.x) * 0.5f;
        float labelY = cy + (cardH - labelSize.y) * 0.5f;

        dl->AddText(labelFont, labelFont->FontSize, ImVec2(labelX + 2, labelY + 2), IM_COL32(0, 0, 0, 180), cat.label);
        dl->AddText(labelFont, labelFont->FontSize, ImVec2(labelX + 1, labelY + 1), IM_COL32(0, 0, 0, 220), cat.label);
        dl->AddText(labelFont, labelFont->FontSize, ImVec2(labelX, labelY),
                    catHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(240, 245, 255, 245), cat.label);

        if (catClk) {
            // Enter category detail view
            showCategoryDetail_ = true;
            categoryDetailLabel_ = cat.label;
            categoryDetailTerm_ = cat.searchTerm;
            categoryDetailGames_.clear();
            categoryDetailLoaded_ = false;
            categoryDetailScrollY_ = 0;
            // Trigger search to load games for this category
            DoSearch(cat.searchTerm);
        }
    }

    // Dot indicators below cards
    float dotsY = y + cardH + 12;
    float dotSpacing = 14.0f;
    float dotsW = totalPages * dotSpacing;
    float dotsX = x + (width - dotsW) * 0.5f;
    for (int i = 0; i < totalPages; i++) {
        float dotX = dotsX + i * dotSpacing + dotSpacing * 0.5f;
        bool isActive = (i == categoryPage_);

        // Clickable dot
        ImGui::SetCursorScreenPos(ImVec2(dotX - 6, dotsY - 6));
        char dotId[16]; snprintf(dotId, sizeof(dotId), "catdot_%d", i);
        ImGui::PushID(dotId);
        ImGui::InvisibleButton("##dot", ImVec2(12, 12));
        if (ImGui::IsItemClicked()) {
            categoryPage_ = i;
        }
        ImGui::PopID();

        if (isActive) {
            dl->AddCircleFilled(ImVec2(dotX, dotsY), 5.0f, IM_COL32(102, 192, 244, 255));
        } else {
            dl->AddCircleFilled(ImVec2(dotX, dotsY), 4.0f, IM_COL32(255, 255, 255, 100));
        }
    }
}

void StorePage::RenderNewReleases(ImDrawList* dl, float x, float y, float width) {
    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 240),
                    u8"\u65B0\u54C1\u4E0E\u70ED\u95E8"); // 新品与热门

    // "See More" clickable
    ImGui::SetCursorScreenPos(ImVec2(x + width - 80, y));
    ImGui::PushID("see_more");
    ImGui::InvisibleButton("##sm", ImVec2(80, 20));
    bool smHov = ImGui::IsItemHovered();
    ImGui::PopID();
    dl->AddText(ImVec2(x + width - 80, y + 4),
                IM_COL32(102, 192, 244, smHov ? 255 : 200),
                u8"\u67E5\u770B\u66F4\u591A >"); // 查看更多 >
    y += 32;

    float cardW = (width - 30) / 4.0f;
    float coverH = cardW * 0.5625f; // 16:9
    float infoH = 65.0f;
    float cardH = coverH + infoH;
    float gap = 10.0f;

    for (int i = 0; i < std::min(8, (int)newReleases_.size()); i++) {
        int row = i / 4, col = i % 4;
        float cx = x + col * (cardW + gap);
        float cy2 = y + row * (cardH + gap);
        auto& g = newReleases_[i];

        bool cardHov = false;
        char cid[16]; snprintf(cid, sizeof(cid), "nr_%d", i);
        if (CardButton(cid, cx, cy2, cardW, cardH, cardHov, dl))
            SelectGame(g.appId);

        DrawShadow(dl, cx, cy2, cardW, cardH, 5.0f);
        dl->AddRectFilled(ImVec2(cx, cy2), ImVec2(cx + cardW, cy2 + cardH), IM_COL32(22, 32, 45, 255), 6.0f);

        ImTextureID ntex = GetBestTexture(g.appId);
        if (ntex)
            dl->AddImageRounded(ntex, ImVec2(cx, cy2), ImVec2(cx + cardW, cy2 + coverH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
        else
            DrawProceduralCover(dl, cx, cy2, cardW, coverH, g.appId);

        float infoY = cy2 + coverH + 8;
        const SteamStoreData* gSd = GetStoreData(g.appId);

        ImGui::PushClipRect(ImVec2(cx, cy2 + coverH), ImVec2(cx + cardW, cy2 + cardH), true);
        dl->AddText(ImVec2(cx + 8, infoY), IM_COL32(255, 255, 255, 230), g.name.c_str());

        // Real tags from Steam data
        if (g_mainFontSmall) {
            std::string gTags;
            if (gSd && !gSd->genres.empty()) {
                for (int gi = 0; gi < std::min(2, (int)gSd->genres.size()); gi++) {
                    if (!gTags.empty()) gTags += u8"\u3001";
                    gTags += gSd->genres[gi];
                }
            }
            if (!gTags.empty())
                dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                            ImVec2(cx + 8, infoY + 18), IM_COL32(140, 160, 180, 160), gTags.c_str());
        }
        ImGui::PopClipRect();

        if (!g.discount.empty()) {
            ImVec2 discSize = ImGui::CalcTextSize(g.discount.c_str());
            float discW = discSize.x + 10;
            dl->AddRectFilled(ImVec2(cx + 8, infoY + 38), ImVec2(cx + 8 + discW, infoY + 56),
                              IM_COL32(76, 107, 34, 255), 3.0f);
            dl->AddText(ImVec2(cx + 13, infoY + 40), IM_COL32(190, 230, 20, 255), g.discount.c_str());
            dl->AddText(ImVec2(cx + 18 + discW, infoY + 40), IM_COL32(190, 230, 20, 255), g.price.c_str());
        } else {
            const char* rp = (gSd && !gSd->priceFormatted.empty()) ? gSd->priceFormatted.c_str() : g.price.c_str();
            dl->AddText(ImVec2(cx + 8, infoY + 40), IM_COL32(190, 230, 20, 255), rp);
        }
        dl->AddRect(ImVec2(cx, cy2), ImVec2(cx + cardW, cy2 + cardH), IM_COL32(255, 255, 255, 10), 6.0f);
    }
}

void StorePage::RenderTrending(ImDrawList* dl, float x, float y, float width) {
    if (trending_.empty()) return;

    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 240),
                    u8"\u70ED\u9500\u6392\u884C"); // 热销排行
    y += 32;

    float cardW = (width - 30) / 4.0f;
    float coverH = cardW * 0.5625f;
    float infoH = 65.0f;
    float cardH = coverH + infoH;
    float gap = 10.0f;

    for (int i = 0; i < std::min(8, (int)trending_.size()); i++) {
        int row = i / 4, col = i % 4;
        float cx = x + col * (cardW + gap);
        float cy2 = y + row * (cardH + gap);
        auto& g = trending_[i];

        bool cardHov = false;
        char cid[16]; snprintf(cid, sizeof(cid), "tr_%d", i);
        if (CardButton(cid, cx, cy2, cardW, cardH, cardHov, dl))
            SelectGame(g.appId);

        DrawShadow(dl, cx, cy2, cardW, cardH, 5.0f);
        dl->AddRectFilled(ImVec2(cx, cy2), ImVec2(cx + cardW, cy2 + cardH), IM_COL32(22, 32, 45, 255), 6.0f);

        ImTextureID ntex = GetBestTexture(g.appId);
        if (ntex)
            dl->AddImageRounded(ntex, ImVec2(cx, cy2), ImVec2(cx + cardW, cy2 + coverH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
        else
            DrawProceduralCover(dl, cx, cy2, cardW, coverH, g.appId);

        float infoY = cy2 + coverH + 8;
        const SteamStoreData* tSd = GetStoreData(g.appId);

        ImGui::PushClipRect(ImVec2(cx, cy2 + coverH), ImVec2(cx + cardW, cy2 + cardH), true);
        dl->AddText(ImVec2(cx + 8, infoY), IM_COL32(255, 255, 255, 230), g.name.c_str());

        // Real tags
        if (g_mainFontSmall && tSd && !tSd->genres.empty()) {
            std::string tTags;
            for (int gi = 0; gi < std::min(2, (int)tSd->genres.size()); gi++) {
                if (!tTags.empty()) tTags += u8"\u3001";
                tTags += tSd->genres[gi];
            }
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                        ImVec2(cx + 8, infoY + 18), IM_COL32(140, 160, 180, 160), tTags.c_str());
        }
        ImGui::PopClipRect();

        if (!g.discount.empty()) {
            ImVec2 discSize = ImGui::CalcTextSize(g.discount.c_str());
            float discW = discSize.x + 10;
            dl->AddRectFilled(ImVec2(cx + 8, infoY + 38), ImVec2(cx + 8 + discW, infoY + 56),
                              IM_COL32(76, 107, 34, 255), 3.0f);
            dl->AddText(ImVec2(cx + 13, infoY + 40), IM_COL32(190, 230, 20, 255), g.discount.c_str());
            dl->AddText(ImVec2(cx + 18 + discW, infoY + 40), IM_COL32(190, 230, 20, 255), g.price.c_str());
        } else {
            const char* rp = (tSd && !tSd->priceFormatted.empty()) ? tSd->priceFormatted.c_str() : g.price.c_str();
            dl->AddText(ImVec2(cx + 8, infoY + 40), IM_COL32(190, 230, 20, 255), rp);
        }
        dl->AddRect(ImVec2(cx, cy2), ImVec2(cx + cardW, cy2 + cardH), IM_COL32(255, 255, 255, 10), 6.0f);
    }
}

void StorePage::RenderComingSoon(ImDrawList* dl, float x, float y, float width) {
    if (comingSoon_.empty()) return;

    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 240),
                    u8"\u5373\u5C06\u63A8\u51FA"); // 即将推出
    y += 32;

    float cardW = (width - 30) / 4.0f;
    float coverH = cardW * 0.5625f;
    float infoH = 65.0f;
    float cardH = coverH + infoH;
    float gap = 10.0f;

    for (int i = 0; i < std::min(4, (int)comingSoon_.size()); i++) {
        float cx = x + i * (cardW + gap);
        auto& g = comingSoon_[i];

        bool cardHov = false;
        char cid[16]; snprintf(cid, sizeof(cid), "cs_%d", i);
        if (CardButton(cid, cx, y, cardW, cardH, cardHov, dl))
            SelectGame(g.appId);

        DrawShadow(dl, cx, y, cardW, cardH, 5.0f);
        dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + cardW, y + cardH), IM_COL32(22, 32, 45, 255), 6.0f);

        ImTextureID ntex = GetBestTexture(g.appId);
        if (ntex)
            dl->AddImageRounded(ntex, ImVec2(cx, y), ImVec2(cx + cardW, y + coverH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
        else
            DrawProceduralCover(dl, cx, y, cardW, coverH, g.appId);

        float infoY = y + coverH + 8;
        const SteamStoreData* cSd = GetStoreData(g.appId);

        ImGui::PushClipRect(ImVec2(cx, y + coverH), ImVec2(cx + cardW, y + cardH), true);
        dl->AddText(ImVec2(cx + 8, infoY), IM_COL32(255, 255, 255, 230), g.name.c_str());

        // Release date from real data
        if (g_mainFontSmall && cSd && !cSd->releaseDate.empty()) {
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                        ImVec2(cx + 8, infoY + 18), IM_COL32(140, 160, 180, 160), cSd->releaseDate.c_str());
        }
        ImGui::PopClipRect();

        const char* csPrice = (cSd && !cSd->priceFormatted.empty()) ? cSd->priceFormatted.c_str() : g.price.c_str();
        dl->AddText(ImVec2(cx + 8, infoY + 40), IM_COL32(140, 160, 180, 180), csPrice);
        dl->AddRect(ImVec2(cx, y), ImVec2(cx + cardW, y + cardH), IM_COL32(255, 255, 255, 10), 6.0f);
    }
}

void StorePage::DoSearch(const std::string& term) {
    if (term.empty()) {
        showSearchResults_ = false;
        ClearSearchResults();
        return;
    }
    activeSearchQuery_ = term;
    showSearchResults_ = true;
    searchResultsLoaded_ = false;
    searchResults_.clear();
    searchTotal_ = 0;
    RequestStoreSearch(term);
}

void StorePage::RenderSearchBar(ImDrawList* dl, float x, float y, float width) {
    float barH = 40.0f;
    float barW = width; // full width
    float barX = x;

    // ── Back button (left side, only when in search results) ──
    float inputStartX = barX;
    if (showSearchResults_) {
        float backW = 80.0f;
        ImVec2 backP0(barX, y);
        ImVec2 backP1(barX + backW, y + barH);

        ImGui::SetCursorScreenPos(backP0);
        ImGui::PushID("##backSearch");
        ImGui::InvisibleButton("##bk", ImVec2(backW, barH));
        bool backHov = ImGui::IsItemHovered();
        bool backClk = ImGui::IsItemClicked();
        ImGui::PopID();

        dl->AddRectFilled(backP0, backP1,
            backHov ? IM_COL32(55, 75, 100, 255) : IM_COL32(38, 52, 72, 255), 6.0f);
        // Arrow
        float arrowCx = barX + 18, arrowCy = y + barH * 0.5f;
        dl->AddTriangleFilled(
            ImVec2(arrowCx, arrowCy),
            ImVec2(arrowCx + 8, arrowCy - 6),
            ImVec2(arrowCx + 8, arrowCy + 6),
            backHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 200, 220, 220));
        if (g_mainFontSmall)
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                ImVec2(arrowCx + 14, y + (barH - g_mainFontSmall->FontSize) * 0.5f),
                backHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 200, 220, 220),
                u8"\u8FD4\u56DE"); // 返回

        if (backClk) {
            showSearchResults_ = false;
            searchBuf_[0] = '\0';
            ClearSearchResults();
        }
        inputStartX = barX + backW + 10;
    }

    // ── Main search input area ──
    float btnW = 90.0f;
    float inputW = barX + barW - inputStartX - btnW - 8;
    float inputX = inputStartX;

    // Input background - dark inset with subtle glow when focused
    ImVec2 inpP0(inputX, y);
    ImVec2 inpP1(inputX + inputW, y + barH);

    // Detect if input is focused for glow effect
    // Use left padding to leave room for magnifying glass icon
    ImGui::SetCursorScreenPos(ImVec2(inputX, y));
    ImGui::PushItemWidth(inputW);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(16, 22, 34, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(20, 28, 40, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(18, 25, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 238, 248, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    // Left padding 28px for magnifying glass, right padding 28px for clear button
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(28, (barH - ImGui::GetFontSize()) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

    bool entered = ImGui::InputText("##searchInput", searchBuf_, sizeof(searchBuf_),
                                     ImGuiInputTextFlags_EnterReturnsTrue);
    bool focused = ImGui::IsItemActive();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
    ImGui::PopItemWidth();

    // Draw custom border with glow
    ImU32 borderCol = focused ? IM_COL32(102, 192, 244, 180) : IM_COL32(50, 70, 95, 180);
    dl->AddRect(inpP0, inpP1, borderCol, 6.0f, 0, focused ? 1.5f : 1.0f);
    if (focused) {
        // Subtle outer glow
        dl->AddRect(ImVec2(inpP0.x - 1, inpP0.y - 1), ImVec2(inpP1.x + 1, inpP1.y + 1),
                    IM_COL32(102, 192, 244, 40), 7.0f, 0, 1.0f);
    }

    // Magnifying glass icon (drawn on top of input background, left side)
    {
        float iconX = inputX + 9, iconY = y + barH * 0.5f;
        float r = 5.0f;
        dl->AddCircle(ImVec2(iconX + r, iconY - 1), r,
                      focused ? IM_COL32(102, 192, 244, 200) : IM_COL32(120, 145, 175, 160), 12, 1.5f);
        dl->AddLine(ImVec2(iconX + r + 3.5f, iconY + 3.5f), ImVec2(iconX + r + 7, iconY + 7),
                    focused ? IM_COL32(102, 192, 244, 200) : IM_COL32(120, 145, 175, 160), 1.5f);
    }

    // Placeholder text
    if (searchBuf_[0] == '\0' && !focused) {
        dl->AddText(ImVec2(inputX + 28, y + (barH - 16) * 0.5f),
                    IM_COL32(90, 110, 140, 140),
                    u8"\u641C\u7D22\u5546\u5E97"); // 搜索商店
    }

    // Immediate search on Enter
    if (entered && searchBuf_[0] != '\0') {
        DoSearch(searchBuf_);
        strncpy(searchPrev_, searchBuf_, sizeof(searchPrev_));
        searchDebounce_ = 0;
    }

    // Debounced live search: trigger search 0.4s after user stops typing
    if (searchBuf_[0] != '\0' && strcmp(searchBuf_, searchPrev_) != 0) {
        searchDebounce_ = 0.4f; // reset timer on text change
        strncpy(searchPrev_, searchBuf_, sizeof(searchPrev_));
    }
    if (searchDebounce_ > 0) {
        searchDebounce_ -= ImGui::GetIO().DeltaTime;
        if (searchDebounce_ <= 0) {
            searchDebounce_ = 0;
            if (searchBuf_[0] != '\0') {
                DoSearch(searchBuf_);
            }
        }
    }

    // ── Search button (right side) ──
    float btnX = inputX + inputW + 8;
    ImVec2 btnP0(btnX, y);
    ImVec2 btnP1(btnX + btnW, y + barH);

    ImGui::SetCursorScreenPos(btnP0);
    ImGui::PushID("##searchBtn");
    ImGui::InvisibleButton("##sb", ImVec2(btnW, barH));
    bool btnHov = ImGui::IsItemHovered();
    bool btnClk = ImGui::IsItemClicked();
    ImGui::PopID();

    // Gradient button
    ImU32 btnTopL = btnHov ? IM_COL32(75, 140, 200, 255) : IM_COL32(50, 112, 170, 255);
    ImU32 btnTopR = btnHov ? IM_COL32(65, 125, 185, 255) : IM_COL32(42, 95, 150, 255);
    ImU32 btnBotL = btnHov ? IM_COL32(55, 115, 175, 255) : IM_COL32(35, 82, 130, 255);
    ImU32 btnBotR = btnHov ? IM_COL32(48, 105, 162, 255) : IM_COL32(30, 72, 118, 255);
    dl->AddRectFilledMultiColor(btnP0, btnP1, btnTopL, btnTopR, btnBotR, btnBotL);
    dl->AddRect(btnP0, btnP1, IM_COL32(100, 170, 230, btnHov ? 120 : 60), 6.0f);

    // Button text centered
    const char* btnText = u8"\u641C  \u7D22"; // 搜  索
    ImVec2 btnTs = ImGui::CalcTextSize(btnText);
    dl->AddText(ImVec2(btnX + (btnW - btnTs.x) * 0.5f, y + (barH - btnTs.y) * 0.5f),
                IM_COL32(255, 255, 255, btnHov ? 255 : 220), btnText);

    if (btnClk && searchBuf_[0] != '\0') {
        DoSearch(searchBuf_);
    }

    // ── Clear (X) button inside input when text present ──
    if (searchBuf_[0] != '\0') {
        float clearX = inputX + inputW - 22;
        float clearY = y + (barH - 16) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(clearX, clearY));
        ImGui::PushID("##clearSearch");
        ImGui::InvisibleButton("##clr", ImVec2(16, 16));
        bool clrHov = ImGui::IsItemHovered();
        ImGui::PopID();

        ImU32 xCol = clrHov ? IM_COL32(255, 255, 255, 220) : IM_COL32(140, 160, 180, 160);
        float cx2 = clearX + 8, cy2 = clearY + 8;
        dl->AddLine(ImVec2(cx2 - 4, cy2 - 4), ImVec2(cx2 + 4, cy2 + 4), xCol, 1.5f);
        dl->AddLine(ImVec2(cx2 + 4, cy2 - 4), ImVec2(cx2 - 4, cy2 + 4), xCol, 1.5f);

        if (ImGui::IsItemClicked()) {
            searchBuf_[0] = '\0';
            if (showSearchResults_) {
                showSearchResults_ = false;
                ClearSearchResults();
            }
        }
    }
}

void StorePage::RenderSearchResults(ImDrawList* dl, float x, float y, float width, float height) {
    // Poll for results
    if (!searchResultsLoaded_) {
        const SteamSearchResults* sr = GetSearchResults();
        if (sr) {
            searchResultsLoaded_ = true;
            searchTotal_ = sr->total;
            searchResults_.clear();
            for (auto& g : sr->items) {
                StoreGame sg;
                sg.name = g.name;
                sg.appId = g.appId;
                sg.price = g.price;
                sg.originalPrice = g.originalPrice;
                sg.discount = g.discount;
                searchResults_.push_back(std::move(sg));
            }
        }
    }

    // Title
    {
        char titleBuf[256];
        if (searchResultsLoaded_) {
            snprintf(titleBuf, sizeof(titleBuf), u8"\u201C%s\u201D \u7684\u641C\u7D22\u7ED3\u679C \u2014 %d \u6B3E\u6E38\u620F",
                     activeSearchQuery_.c_str(), searchTotal_); // "xxx" 的搜索结果 — N 款游戏
        } else {
            snprintf(titleBuf, sizeof(titleBuf), u8"\u6B63\u5728\u641C\u7D22 \u201C%s\u201D ...",
                     activeSearchQuery_.c_str()); // 正在搜索 "xxx" ...
        }
        if (g_mainFontLarge)
            dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                        ImVec2(x, y), IM_COL32(255, 255, 255, 240), titleBuf);
    }
    y += 36;

    // Loading indicator
    if (!searchResultsLoaded_ || IsSearchLoading()) {
        float cx2 = x + width * 0.5f;
        float t = fmodf((float)ImGui::GetTime(), 1.0f);
        for (int i = 0; i < 3; i++) {
            float alpha = (t > i * 0.3f) ? 255.0f : 80.0f;
            dl->AddCircleFilled(ImVec2(cx2 - 20 + i * 20, y + 30), 4.0f,
                                IM_COL32(102, 192, 244, (int)alpha));
        }
        ImGui::Dummy(ImVec2(width, 80));
        return;
    }

    if (searchResults_.empty()) {
        dl->AddText(ImVec2(x, y + 10), IM_COL32(150, 170, 190, 200),
                    u8"\u6CA1\u6709\u627E\u5230\u76F8\u5173\u6E38\u620F"); // 没有找到相关游戏
        ImGui::Dummy(ImVec2(width, 60));
        return;
    }

    // Render results as Steam-style list rows
    float rowH = 70.0f;
    float gap = 2.0f;
    float imgW = 120.0f;  // cover image width
    float imgH = rowH - 4;
    float priceAreaW = 180.0f; // right side price area

    int count = (int)searchResults_.size();
    for (int i = 0; i < count; i++) {
        float ry = y + i * (rowH + gap);
        auto& g = searchResults_[i];

        // Row clickable area
        bool rowHov = false;
        char cid[32]; snprintf(cid, sizeof(cid), "sr_%d", i);
        if (CardButton(cid, x, ry, width, rowH, rowHov, dl))
            SelectGame(g.appId);

        // Row background (alternating + hover)
        ImU32 rowBg = rowHov ? IM_COL32(42, 58, 80, 255)
                    : (i % 2 == 0) ? IM_COL32(22, 32, 48, 255)
                                   : IM_COL32(26, 37, 53, 255);
        dl->AddRectFilled(ImVec2(x, ry), ImVec2(x + width, ry + rowH), rowBg, 3.0f);

        // Cover image (left)
        float imgX = x + 2, imgY = ry + 2;
        ImTextureID tex = GetBestTexture(g.appId);
        if (tex) {
            dl->AddImageRounded(tex, ImVec2(imgX, imgY), ImVec2(imgX + imgW, imgY + imgH),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 3.0f);
        } else {
            DrawProceduralCover(dl, imgX, imgY, imgW, imgH, g.appId);
        }

        // Game info (middle)
        float infoX = imgX + imgW + 14;
        float infoW = width - imgW - priceAreaW - 30;

        // Name
        ImGui::PushClipRect(ImVec2(infoX, ry), ImVec2(infoX + infoW, ry + rowH), true);
        dl->AddText(ImVec2(infoX, ry + 8), IM_COL32(255, 255, 255, 240), g.name.c_str());

        // Tags from SteamStoreData if available
        const SteamStoreData* sd = GetStoreData(g.appId);
        if (sd && !sd->genres.empty()) {
            std::string tagsStr;
            for (int gi = 0; gi < std::min(5, (int)sd->genres.size()); gi++) {
                if (!tagsStr.empty()) tagsStr += u8"\u3001"; // 、
                tagsStr += sd->genres[gi];
            }
            if (g_mainFontSmall)
                dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                    ImVec2(infoX, ry + 30),
                    IM_COL32(120, 145, 175, 180), tagsStr.c_str());
        } else if (!g.tags.empty()) {
            if (g_mainFontSmall)
                dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                    ImVec2(infoX, ry + 30),
                    IM_COL32(120, 145, 175, 180), g.tags.c_str());
        }

        // Release date
        if (sd && !sd->releaseDate.empty()) {
            if (g_mainFontSmall)
                dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                    ImVec2(infoX, ry + 48),
                    IM_COL32(100, 120, 145, 140), sd->releaseDate.c_str());
        }
        ImGui::PopClipRect();

        // Price area (right side)
        float priceX = x + width - priceAreaW;
        ImGui::PushClipRect(ImVec2(priceX, ry), ImVec2(x + width, ry + rowH), true);

        if (!g.discount.empty()) {
            // Discount badge
            ImVec2 ds = ImGui::CalcTextSize(g.discount.c_str());
            float discX = priceX + 4;
            float discY = ry + (rowH - 28) * 0.5f;
            dl->AddRectFilled(ImVec2(discX, discY), ImVec2(discX + ds.x + 12, discY + 28),
                              IM_COL32(76, 107, 34, 255), 3.0f);
            dl->AddText(ImVec2(discX + 6, discY + 6), IM_COL32(190, 230, 20, 255), g.discount.c_str());

            // Price block (original + final)
            float prBlockX = discX + ds.x + 20;
            // Original price (strikethrough)
            if (!g.price.empty()) {
                // Use originalPrice from search/listing data, fallback to SteamStoreData
                std::string origP = g.originalPrice;
                if (origP.empty() && sd && !sd->originalPrice.empty()) origP = sd->originalPrice;

                if (!origP.empty()) {
                    ImVec2 ops = ImGui::CalcTextSize(origP.c_str());
                    if (g_mainFontSmall)
                        dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                            ImVec2(prBlockX, discY + 2),
                            IM_COL32(140, 160, 180, 150), origP.c_str());
                    // Strikethrough line
                    dl->AddLine(ImVec2(prBlockX, discY + 8), ImVec2(prBlockX + ops.x, discY + 8),
                                IM_COL32(140, 160, 180, 150), 1.0f);
                }
                // Final price
                dl->AddText(ImVec2(prBlockX, discY + 14),
                            IM_COL32(190, 230, 20, 255), g.price.c_str());
            }
        } else {
            // No discount - just price
            ImVec2 ps = ImGui::CalcTextSize(g.price.c_str());
            dl->AddText(ImVec2(x + width - ps.x - 16, ry + (rowH - ps.y) * 0.5f),
                        IM_COL32(200, 215, 230, 220), g.price.c_str());
        }
        ImGui::PopClipRect();

        // Hover highlight border
        if (rowHov) {
            dl->AddRect(ImVec2(x, ry), ImVec2(x + width, ry + rowH),
                        IM_COL32(102, 192, 244, 80), 3.0f, 0, 1.5f);
        }
    }

    // Height is set by caller via SetCursorPosY
}

// ====================== Category Detail Page ======================

void StorePage::RenderCategoryDetail(ImDrawList* dl, float x, float y, float width, float height) {
    float cx = x + 24;
    float cy = y + 16;
    float cw = width - 48;
    float contentY = 16;

    // Back button
    float backBtnW = 100.0f, backBtnH = 36.0f;
    ImGui::SetCursorScreenPos(ImVec2(cx, cy));
    ImGui::PushID("##catDetailBack");
    ImGui::InvisibleButton("##back", ImVec2(backBtnW, backBtnH));
    bool backHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        showCategoryDetail_ = false;
        categoryDetailGames_.clear();
    }
    ImGui::PopID();

    // Draw back button
    dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + backBtnW, cy + backBtnH),
                      backHov ? IM_COL32(50, 70, 95, 255) : IM_COL32(35, 50, 70, 255), 6.0f);
    dl->AddRect(ImVec2(cx, cy), ImVec2(cx + backBtnW, cy + backBtnH),
                backHov ? IM_COL32(102, 192, 244, 150) : IM_COL32(70, 95, 130, 120), 6.0f);

    // Back arrow and text
    float arrowX = cx + 14;
    float arrowY = cy + backBtnH * 0.5f;
    dl->AddLine(ImVec2(arrowX + 6, arrowY - 6), ImVec2(arrowX, arrowY),
                backHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 210, 220, 220), 2.0f);
    dl->AddLine(ImVec2(arrowX, arrowY), ImVec2(arrowX + 6, arrowY + 6),
                backHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 210, 220, 220), 2.0f);
    dl->AddText(ImVec2(cx + 28, cy + 10),
                backHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 210, 220, 220),
                u8"\u8FD4\u56DE"); // 返回

    cy += backBtnH + 20;
    contentY += backBtnH + 20;

    // Category title with icon
    if (g_mainFontLarge) {
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize * 1.2f,
                    ImVec2(cx, cy), IM_COL32(255, 255, 255, 255),
                    categoryDetailLabel_.c_str());
    } else {
        dl->AddText(ImVec2(cx, cy), IM_COL32(255, 255, 255, 255), categoryDetailLabel_.c_str());
    }

    // Game count
    if (!searchResults_.empty()) {
        char countBuf[64];
        snprintf(countBuf, sizeof(countBuf), u8"  %d \u6B3E\u6E38\u620F", (int)searchResults_.size()); // N 款游戏
        ImVec2 titleSize = g_mainFontLarge ?
            g_mainFontLarge->CalcTextSizeA(g_mainFontLarge->FontSize * 1.2f, FLT_MAX, 0, categoryDetailLabel_.c_str())
            : ImGui::CalcTextSize(categoryDetailLabel_.c_str());
        if (g_mainFontSmall)
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                ImVec2(cx + titleSize.x + 12, cy + 8),
                IM_COL32(102, 192, 244, 200), countBuf);
    }

    cy += 45;
    contentY += 45;

    // Divider
    dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + 1),
        IM_COL32(80, 100, 120, 0), IM_COL32(80, 100, 120, 80),
        IM_COL32(80, 100, 120, 80), IM_COL32(80, 100, 120, 0));
    cy += 20;
    contentY += 20;

    // Loading state
    if (!searchResultsLoaded_) {
        float loadX = cx + cw * 0.5f;
        float loadY = cy + 60;

        if (g_mainFont)
            dl->AddText(g_mainFont, g_mainFont->FontSize,
                ImVec2(loadX - 60, loadY - 30), IM_COL32(140, 160, 180, 200),
                u8"\u6B63\u5728\u52A0\u8F7D..."); // 正在加载...

        float t = fmodf((float)ImGui::GetTime(), 1.0f);
        for (int i = 0; i < 3; i++) {
            float alpha = (t > i * 0.3f) ? 255.0f : 80.0f;
            dl->AddCircleFilled(ImVec2(loadX - 20 + i * 20, loadY + 10), 4.0f,
                                IM_COL32(102, 192, 244, (int)alpha));
        }
        contentY += 150;
        ImGui::SetCursorPosY(contentY);
        return;
    }

    // No results
    if (searchResults_.empty()) {
        if (g_mainFont)
            dl->AddText(g_mainFont, g_mainFont->FontSize,
                ImVec2(cx + cw * 0.5f - 80, cy + 60), IM_COL32(140, 160, 180, 200),
                u8"\u6682\u65E0\u7ED3\u679C"); // 暂无结果
        contentY += 150;
        ImGui::SetCursorPosY(contentY);
        return;
    }

    // Render game cards in grid (larger cards for category detail)
    int cols = 4;
    float gap = 14.0f;
    float cardW = (cw - (cols - 1) * gap) / (float)cols;
    float coverH = cardW * 0.467f; // header.jpg aspect ~460:215
    float infoH = 70.0f;  // More space for info
    float cardH = coverH + infoH;

    int count = (int)searchResults_.size();
    for (int i = 0; i < count; i++) {
        int row = i / cols, col = i % cols;
        float cx2 = cx + col * (cardW + gap);
        float cy2 = cy + row * (cardH + gap);
        auto& g = searchResults_[i];

        // Request store data for detailed info
        const SteamStoreData* sd = GetStoreData(g.appId);
        if (!sd && !IsStoreDataLoading(g.appId)) {
            RequestStoreData(g.appId);
        }

        bool cardHov = false;
        char cid[32]; snprintf(cid, sizeof(cid), "catg_%d", i);
        if (CardButton(cid, cx2, cy2, cardW, cardH, cardHov, dl))
            SelectGame(g.appId);

        // Shadow
        DrawShadow(dl, cx2, cy2, cardW, cardH, 5.0f);

        // Card background
        dl->AddRectFilled(ImVec2(cx2, cy2), ImVec2(cx2 + cardW, cy2 + cardH),
                          IM_COL32(22, 32, 45, 255), 8.0f);

        // Cover image
        ImTextureID tex = GetBestTexture(g.appId);
        if (tex) {
            dl->AddImageRounded(tex, ImVec2(cx2, cy2), ImVec2(cx2 + cardW, cy2 + coverH),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);
        } else {
            DrawProceduralCover(dl, cx2, cy2, cardW, coverH, g.appId);
        }

        // Info section
        float infoY = cy2 + coverH + 6;
        ImGui::PushClipRect(ImVec2(cx2, cy2 + coverH), ImVec2(cx2 + cardW - 4, cy2 + cardH), true);

        // Game name
        const char* gameName = (sd && !sd->name.empty()) ? sd->name.c_str() : g.name.c_str();
        if (g_mainFont)
            dl->AddText(g_mainFont, g_mainFont->FontSize,
                ImVec2(cx2 + 8, infoY), IM_COL32(255, 255, 255, 240), gameName);
        else
            dl->AddText(ImVec2(cx2 + 8, infoY), IM_COL32(255, 255, 255, 240), gameName);

        // Tags/genres
        if (sd && !sd->genres.empty() && g_mainFontSmall) {
            std::string tags;
            for (int gi = 0; gi < std::min(2, (int)sd->genres.size()); gi++) {
                if (!tags.empty()) tags += u8"\u3001";
                tags += sd->genres[gi];
            }
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                ImVec2(cx2 + 8, infoY + 22), IM_COL32(140, 160, 180, 180), tags.c_str());
        }

        // Price
        float priceY = cy2 + cardH - 24;
        const char* priceStr = (sd && !sd->priceFormatted.empty()) ? sd->priceFormatted.c_str() : g.price.c_str();

        if (!g.discount.empty()) {
            // Discount badge
            ImVec2 discSize = ImGui::CalcTextSize(g.discount.c_str());
            float discW = discSize.x + 12;
            dl->AddRectFilled(ImVec2(cx2 + 8, priceY - 2), ImVec2(cx2 + 8 + discW, priceY + 18),
                              IM_COL32(76, 107, 34, 255), 4.0f);
            dl->AddText(ImVec2(cx2 + 14, priceY + 1), IM_COL32(190, 230, 20, 255), g.discount.c_str());
            dl->AddText(ImVec2(cx2 + 14 + discW + 6, priceY + 1), IM_COL32(190, 230, 20, 255), priceStr);
        } else {
            dl->AddText(ImVec2(cx2 + 8, priceY + 1), IM_COL32(190, 230, 20, 255), priceStr);
        }

        ImGui::PopClipRect();

        // Hover effect
        if (cardHov) {
            dl->AddRect(ImVec2(cx2 - 1, cy2 - 1), ImVec2(cx2 + cardW + 1, cy2 + cardH + 1),
                        IM_COL32(102, 192, 244, 180), 8.0f, 0, 2.0f);
        } else {
            dl->AddRect(ImVec2(cx2, cy2), ImVec2(cx2 + cardW, cy2 + cardH),
                        IM_COL32(255, 255, 255, 10), 8.0f);
        }
    }

    int rows = (count + cols - 1) / cols;
    contentY += rows * (cardH + gap) + 40;

    ImGui::SetCursorPosY(contentY);
}

void StorePage::RenderBrowseAll(ImDrawList* dl, float x, float& cy, float width) {
    // Auto-request first page
    if (!browseRequested_) {
        browseRequested_ = true;
        RequestBrowseGames(0, 100);
    }

    // Poll for new data
    const SteamBrowsePage* bp = GetBrowsePage();
    if (bp && (int)bp->games.size() > browseLoadedCount_) {
        // New games arrived, convert them
        for (int i = browseLoadedCount_; i < (int)bp->games.size(); i++) {
            StoreGame sg;
            sg.name = bp->games[i].name;
            sg.appId = bp->games[i].appId;
            sg.price = bp->games[i].price;
            sg.discount = bp->games[i].discount;
            browseGames_.push_back(std::move(sg));
        }
        browseLoadedCount_ = (int)bp->games.size();
        browseLastAppId_ = bp->lastAppId;
        browseHasMore_ = bp->hasMore;
    }

    // Section title
    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, cy), IM_COL32(255, 255, 255, 240),
                    u8"\u6D4F\u89C8\u5168\u90E8\u6E38\u620F"); // 浏览全部游戏

    // Game count badge
    if (!browseGames_.empty()) {
        char countBuf[64];
        snprintf(countBuf, sizeof(countBuf), u8"  \u5DF2\u52A0\u8F7D %d \u6B3E", (int)browseGames_.size()); // 已加载 N 款
        ImVec2 titleSize = g_mainFontLarge ?
            g_mainFontLarge->CalcTextSizeA(g_mainFontLarge->FontSize, FLT_MAX, 0,
                u8"\u6D4F\u89C8\u5168\u90E8\u6E38\u620F") : ImGui::CalcTextSize(u8"\u6D4F\u89C8\u5168\u90E8\u6E38\u620F");
        if (g_mainFontSmall)
            dl->AddText(g_mainFontSmall, g_mainFontSmall->FontSize,
                ImVec2(x + titleSize.x + 8, cy + 6),
                IM_COL32(102, 192, 244, 180), countBuf);
    }
    cy += 36;

    if (browseGames_.empty() && IsBrowseLoading()) {
        // Loading animation
        float loadX = x + width * 0.5f;
        float t = fmodf((float)ImGui::GetTime(), 1.0f);
        for (int i = 0; i < 3; i++) {
            float alpha = (t > i * 0.3f) ? 255.0f : 80.0f;
            dl->AddCircleFilled(ImVec2(loadX - 20 + i * 20, cy + 20), 4.0f,
                                IM_COL32(102, 192, 244, (int)alpha));
        }
        cy += 60;
        return;
    }

    // Render game cards in grid
    int cols = 5;
    float gap = 10.0f;
    float cardW = (width - (cols - 1) * gap) / (float)cols;
    float coverH = cardW * 0.467f; // header.jpg aspect ~460:215
    float infoH = 40.0f;
    float cardH = coverH + infoH;

    int count = (int)browseGames_.size();
    for (int i = 0; i < count; i++) {
        int row = i / cols, col = i % cols;
        float cx2 = x + col * (cardW + gap);
        float cy2 = cy + row * (cardH + gap);
        auto& g = browseGames_[i];

        bool cardHov = false;
        char cid[32]; snprintf(cid, sizeof(cid), "br_%d", i);
        if (CardButton(cid, cx2, cy2, cardW, cardH, cardHov, dl))
            SelectGame(g.appId);

        DrawShadow(dl, cx2, cy2, cardW, cardH, 4.0f);
        dl->AddRectFilled(ImVec2(cx2, cy2), ImVec2(cx2 + cardW, cy2 + cardH),
                          IM_COL32(22, 32, 45, 255), 5.0f);

        ImTextureID tex = GetBestTexture(g.appId);
        if (tex) {
            dl->AddImageRounded(tex, ImVec2(cx2, cy2), ImVec2(cx2 + cardW, cy2 + coverH),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 5.0f);
        } else {
            DrawProceduralCover(dl, cx2, cy2, cardW, coverH, g.appId);
        }

        // Name (clipped to card)
        ImGui::PushClipRect(ImVec2(cx2, cy2 + coverH), ImVec2(cx2 + cardW, cy2 + cardH), true);
        dl->AddText(ImVec2(cx2 + 6, cy2 + coverH + 4), IM_COL32(255, 255, 255, 220), g.name.c_str());

        if (!g.price.empty())
            dl->AddText(ImVec2(cx2 + 6, cy2 + coverH + 22), IM_COL32(140, 160, 180, 180), g.price.c_str());
        ImGui::PopClipRect();

        if (cardHov)
            dl->AddRect(ImVec2(cx2, cy2), ImVec2(cx2 + cardW, cy2 + cardH),
                        IM_COL32(102, 192, 244, 120), 5.0f, 0, 1.5f);
        else
            dl->AddRect(ImVec2(cx2, cy2), ImVec2(cx2 + cardW, cy2 + cardH),
                        IM_COL32(255, 255, 255, 8), 5.0f);
    }

    int rows = (count + cols - 1) / cols;
    cy += rows * (cardH + gap) + 10;

    // "Load More" button
    if (browseHasMore_) {
        float btnW = 240.0f, btnH = 42.0f;
        float btnX = x + (width - btnW) * 0.5f;

        ImVec2 btnP0(btnX, cy);
        ImVec2 btnP1(btnX + btnW, cy + btnH);

        ImGui::SetCursorScreenPos(btnP0);
        ImGui::PushID("##loadMoreBrowse");
        ImGui::InvisibleButton("##lm", ImVec2(btnW, btnH));
        bool lmHov = ImGui::IsItemHovered();
        bool lmClk = ImGui::IsItemClicked();
        ImGui::PopID();

        bool loading = IsBrowseLoading();

        // Button style
        ImU32 btnBg = loading ? IM_COL32(30, 42, 58, 255)
                     : lmHov ? IM_COL32(50, 112, 170, 255)
                             : IM_COL32(38, 55, 78, 255);
        dl->AddRectFilled(btnP0, btnP1, btnBg, 6.0f);
        dl->AddRect(btnP0, btnP1,
                    lmHov ? IM_COL32(102, 192, 244, 160) : IM_COL32(70, 95, 130, 160), 6.0f);

        const char* btnText = loading
            ? u8"\u52A0\u8F7D\u4E2D..."                              // 加载中...
            : u8"\u52A0\u8F7D\u66F4\u591A\u6E38\u620F  \u25BC";     // 加载更多游戏  ▼
        ImVec2 ts = ImGui::CalcTextSize(btnText);
        dl->AddText(ImVec2(btnX + (btnW - ts.x) * 0.5f, cy + (btnH - ts.y) * 0.5f),
                    loading ? IM_COL32(140, 160, 180, 160) : IM_COL32(220, 235, 250, 240),
                    btnText);

        if (lmClk && !loading) {
            RequestBrowseGames(browseLastAppId_, 100);
        }

        cy += btnH + 20;
    }
}

void StorePage::Render(float x, float y, float width, float height) {
    // Poll for Steam listings data
    UpdateFromSteamListings();

    // 侧边栏宽度
    float sidebarW = 220.0f;

    // 渲染侧边栏
    RenderSidebar(x, y, sidebarW, height);

    // 渲染主内容区（根据分类显示不同内容）
    RenderMainContent(x + sidebarW, y, width - sidebarW, height);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SVG 风格图标 - 畅玩图标（火箭）
// ═══════════════════════════════════════════════════════════════════════════
static void DrawIconFreePlay(ImDrawList* dl, float cx, float cy, float size, ImU32 color, float thickness = 2.0f) {
    float s = size * 0.5f;

    // 火箭主体 - 流线型设计
    // 火箭头部（三角形）
    ImVec2 tip(cx + s * 0.5f, cy - s * 0.1f);
    ImVec2 topLeft(cx - s * 0.1f, cy - s * 0.5f);
    ImVec2 botLeft(cx - s * 0.1f, cy + s * 0.3f);
    dl->AddTriangleFilled(tip, topLeft, botLeft, color);

    // 火箭身体（圆角矩形）
    dl->AddRectFilled(ImVec2(cx - s * 0.4f, cy - s * 0.4f),
                      ImVec2(cx + s * 0.1f, cy + s * 0.2f),
                      color, s * 0.15f);

    // 火箭窗口（小圆）
    dl->AddCircleFilled(ImVec2(cx - s * 0.15f, cy - s * 0.15f), s * 0.12f,
                        IM_COL32(20, 24, 32, 255));
    dl->AddCircle(ImVec2(cx - s * 0.15f, cy - s * 0.15f), s * 0.12f, color, 12, 1.5f);

    // 火箭尾翼
    dl->AddTriangleFilled(
        ImVec2(cx - s * 0.4f, cy + s * 0.1f),
        ImVec2(cx - s * 0.6f, cy + s * 0.5f),
        ImVec2(cx - s * 0.3f, cy + s * 0.3f),
        color);

    // 火焰效果（渐变三角形）
    ImU32 flameOuter = IM_COL32(255, 150, 50, 200);
    ImU32 flameInner = IM_COL32(255, 220, 100, 255);
    dl->AddTriangleFilled(
        ImVec2(cx - s * 0.35f, cy + s * 0.25f),
        ImVec2(cx - s * 0.5f, cy + s * 0.7f),
        ImVec2(cx - s * 0.2f, cy + s * 0.25f),
        flameOuter);
    dl->AddTriangleFilled(
        ImVec2(cx - s * 0.32f, cy + s * 0.28f),
        ImVec2(cx - s * 0.4f, cy + s * 0.55f),
        ImVec2(cx - s * 0.23f, cy + s * 0.28f),
        flameInner);

    // 速度线（表示快速）
    dl->AddLine(ImVec2(cx + s * 0.3f, cy - s * 0.4f),
                ImVec2(cx + s * 0.6f, cy - s * 0.55f),
                color, 1.5f);
    dl->AddLine(ImVec2(cx + s * 0.35f, cy - s * 0.15f),
                ImVec2(cx + s * 0.65f, cy - s * 0.25f),
                color, 1.5f);
    dl->AddLine(ImVec2(cx + s * 0.3f, cy + s * 0.1f),
                ImVec2(cx + s * 0.55f, cy + s * 0.05f),
                color, 1.5f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SVG 风格图标 - 签到图标（日历打勾）
// ═══════════════════════════════════════════════════════════════════════════
static void DrawIconCheckIn(ImDrawList* dl, float cx, float cy, float size, ImU32 color) {
    float s = size * 0.4f;

    // 日历主体
    dl->AddRect(ImVec2(cx - s, cy - s * 0.7f), ImVec2(cx + s, cy + s), color, 3.0f, 0, 2.0f);

    // 日历顶部横条
    dl->AddRectFilled(ImVec2(cx - s, cy - s * 0.7f), ImVec2(cx + s, cy - s * 0.3f), color, 3.0f, ImDrawFlags_RoundCornersTop);

    // 日历挂钩
    dl->AddLine(ImVec2(cx - s * 0.5f, cy - s), ImVec2(cx - s * 0.5f, cy - s * 0.5f), color, 2.5f);
    dl->AddLine(ImVec2(cx + s * 0.5f, cy - s), ImVec2(cx + s * 0.5f, cy - s * 0.5f), color, 2.5f);

    // 打勾
    dl->AddLine(ImVec2(cx - s * 0.4f, cy + s * 0.2f), ImVec2(cx - s * 0.1f, cy + s * 0.5f), IM_COL32(87, 203, 100, 255), 2.5f);
    dl->AddLine(ImVec2(cx - s * 0.1f, cy + s * 0.5f), ImVec2(cx + s * 0.5f, cy - s * 0.1f), IM_COL32(87, 203, 100, 255), 2.5f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SVG 风格图标 - 抽奖图标（礼盒/转盘）
// ═══════════════════════════════════════════════════════════════════════════
static void DrawIconLottery(ImDrawList* dl, float cx, float cy, float size, ImU32 color) {
    float s = size * 0.4f;

    // 礼盒主体
    dl->AddRectFilled(ImVec2(cx - s, cy - s * 0.2f), ImVec2(cx + s, cy + s), color, 4.0f, ImDrawFlags_RoundCornersBottom);

    // 礼盒盖子
    dl->AddRectFilled(ImVec2(cx - s * 1.1f, cy - s * 0.5f), ImVec2(cx + s * 1.1f, cy - s * 0.15f), color, 3.0f);

    // 中间竖条（丝带）
    dl->AddRectFilled(ImVec2(cx - s * 0.15f, cy - s * 0.5f), ImVec2(cx + s * 0.15f, cy + s), IM_COL32(255, 220, 100, 255));

    // 蝴蝶结
    dl->AddCircleFilled(ImVec2(cx - s * 0.4f, cy - s * 0.7f), s * 0.25f, IM_COL32(255, 220, 100, 255));
    dl->AddCircleFilled(ImVec2(cx + s * 0.4f, cy - s * 0.7f), s * 0.25f, IM_COL32(255, 220, 100, 255));
    dl->AddCircleFilled(ImVec2(cx, cy - s * 0.7f), s * 0.18f, IM_COL32(255, 180, 50, 255));
}

// ═══════════════════════════════════════════════════════════════════════════
//  SVG 风格图标 - 任务图标（清单）
// ═══════════════════════════════════════════════════════════════════════════
static void DrawIconTask(ImDrawList* dl, float cx, float cy, float size, ImU32 color) {
    float s = size * 0.4f;

    // 剪贴板主体
    dl->AddRectFilled(ImVec2(cx - s * 0.8f, cy - s * 0.6f), ImVec2(cx + s * 0.8f, cy + s), color, 4.0f);

    // 剪贴板夹子
    dl->AddRectFilled(ImVec2(cx - s * 0.35f, cy - s * 0.9f), ImVec2(cx + s * 0.35f, cy - s * 0.5f), color, 3.0f);
    dl->AddRectFilled(ImVec2(cx - s * 0.25f, cy - s * 1.0f), ImVec2(cx + s * 0.25f, cy - s * 0.75f), IM_COL32(60, 70, 90, 255), 2.0f);

    // 任务行（三行）
    float lineY1 = cy - s * 0.25f;
    float lineY2 = cy + s * 0.15f;
    float lineY3 = cy + s * 0.55f;

    // 复选框和线条
    dl->AddRect(ImVec2(cx - s * 0.55f, lineY1 - s * 0.12f), ImVec2(cx - s * 0.35f, lineY1 + s * 0.12f), IM_COL32(87, 203, 100, 255), 2.0f, 0, 1.5f);
    dl->AddLine(ImVec2(cx - s * 0.52f, lineY1), ImVec2(cx - s * 0.45f, lineY1 + s * 0.08f), IM_COL32(87, 203, 100, 255), 1.5f);
    dl->AddLine(ImVec2(cx - s * 0.45f, lineY1 + s * 0.08f), ImVec2(cx - s * 0.38f, lineY1 - s * 0.08f), IM_COL32(87, 203, 100, 255), 1.5f);
    dl->AddRectFilled(ImVec2(cx - s * 0.2f, lineY1 - s * 0.04f), ImVec2(cx + s * 0.55f, lineY1 + s * 0.04f), IM_COL32(60, 70, 90, 255), 2.0f);

    dl->AddRect(ImVec2(cx - s * 0.55f, lineY2 - s * 0.12f), ImVec2(cx - s * 0.35f, lineY2 + s * 0.12f), IM_COL32(87, 203, 100, 255), 2.0f, 0, 1.5f);
    dl->AddLine(ImVec2(cx - s * 0.52f, lineY2), ImVec2(cx - s * 0.45f, lineY2 + s * 0.08f), IM_COL32(87, 203, 100, 255), 1.5f);
    dl->AddLine(ImVec2(cx - s * 0.45f, lineY2 + s * 0.08f), ImVec2(cx - s * 0.38f, lineY2 - s * 0.08f), IM_COL32(87, 203, 100, 255), 1.5f);
    dl->AddRectFilled(ImVec2(cx - s * 0.2f, lineY2 - s * 0.04f), ImVec2(cx + s * 0.45f, lineY2 + s * 0.04f), IM_COL32(60, 70, 90, 255), 2.0f);

    dl->AddRect(ImVec2(cx - s * 0.55f, lineY3 - s * 0.12f), ImVec2(cx - s * 0.35f, lineY3 + s * 0.12f), IM_COL32(140, 155, 175, 180), 2.0f, 0, 1.5f);
    dl->AddRectFilled(ImVec2(cx - s * 0.2f, lineY3 - s * 0.04f), ImVec2(cx + s * 0.35f, lineY3 + s * 0.04f), IM_COL32(60, 70, 90, 255), 2.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SVG 风格图标 - 商城图标（购物袋）
// ═══════════════════════════════════════════════════════════════════════════
static void DrawIconShop(ImDrawList* dl, float cx, float cy, float size, ImU32 color) {
    float s = size * 0.4f;

    // 购物袋主体（梯形）
    ImVec2 bagPts[4] = {
        ImVec2(cx - s * 0.7f, cy - s * 0.3f),
        ImVec2(cx + s * 0.7f, cy - s * 0.3f),
        ImVec2(cx + s * 0.9f, cy + s),
        ImVec2(cx - s * 0.9f, cy + s)
    };
    dl->AddConvexPolyFilled(bagPts, 4, color);

    // 购物袋提手
    dl->AddBezierQuadratic(
        ImVec2(cx - s * 0.4f, cy - s * 0.3f),
        ImVec2(cx - s * 0.4f, cy - s * 0.9f),
        ImVec2(cx, cy - s * 0.9f),
        IM_COL32(60, 70, 90, 255), 3.0f);
    dl->AddBezierQuadratic(
        ImVec2(cx, cy - s * 0.9f),
        ImVec2(cx + s * 0.4f, cy - s * 0.9f),
        ImVec2(cx + s * 0.4f, cy - s * 0.3f),
        IM_COL32(60, 70, 90, 255), 3.0f);

    // 星星装饰
    float starCx = cx;
    float starCy = cy + s * 0.35f;
    float starR = s * 0.3f;
    for (int i = 0; i < 5; i++) {
        float angle1 = -3.14159f / 2 + i * 3.14159f * 2 / 5;
        float angle2 = angle1 + 3.14159f / 5;
        ImVec2 outer(starCx + cosf(angle1) * starR, starCy + sinf(angle1) * starR);
        ImVec2 inner(starCx + cosf(angle2) * starR * 0.4f, starCy + sinf(angle2) * starR * 0.4f);
        dl->AddLine(ImVec2(starCx, starCy), outer, IM_COL32(255, 220, 100, 255), 2.0f);
    }
    dl->AddCircleFilled(ImVec2(starCx, starCy), s * 0.12f, IM_COL32(255, 220, 100, 255));
}

void StorePage::RenderSidebar(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 24, 32, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 16));

    if (ImGui::Begin("##StoreSidebar", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 获取窗口实际位置
        ImVec2 winPos = ImGui::GetWindowPos();
        float wx = winPos.x;
        float wy = winPos.y;

        float cardX = wx + 12;
        float cardY = wy + 16;
        float cardW = width - 24;

        // 字体
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
        ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;

        // ═══════════════════════════════════════════════════════════════
        //  功能入口大卡片（签到、抽奖、任务、商城）
        // ═══════════════════════════════════════════════════════════════
        float funcCardH = 180.0f;

        // 卡片背景 - 玻璃效果
        dl->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + funcCardH),
                          IM_COL32(30, 38, 52, 220), 12.0f);
        // 顶部高光
        dl->AddRectFilledMultiColor(
            ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + 40),
            IM_COL32(255, 255, 255, 8), IM_COL32(255, 255, 255, 8),
            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
        // 边框
        dl->AddRect(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + funcCardH),
                    IM_COL32(255, 255, 255, 25), 12.0f, 0, 1.0f);

        // 2x2 网格布局
        float cellW = (cardW - 20) / 2;
        float cellH = (funcCardH - 20) / 2;
        float startX = cardX + 10;
        float startY = cardY + 10;

        struct FuncItem {
            const char* label;
            ImU32 iconColor;
            ImU32 bgColor;
            void (*drawIcon)(ImDrawList*, float, float, float, ImU32);
        };
        FuncItem funcs[] = {
            {u8"签到", IM_COL32(102, 192, 244, 255), IM_COL32(102, 192, 244, 30), DrawIconCheckIn},
            {u8"抽奖", IM_COL32(255, 180, 50, 255), IM_COL32(255, 180, 50, 30), DrawIconLottery},
            {u8"任务", IM_COL32(87, 203, 100, 255), IM_COL32(87, 203, 100, 30), DrawIconTask},
            {u8"商城", IM_COL32(220, 130, 255, 255), IM_COL32(220, 130, 255, 30), DrawIconShop},
        };

        for (int i = 0; i < 4; i++) {
            int row = i / 2;
            int col = i % 2;
            float cellX = startX + col * cellW;
            float cellY = startY + row * cellH;

            ImGui::SetCursorScreenPos(ImVec2(cellX, cellY));
            ImGui::PushID(i + 2000);
            ImGui::InvisibleButton("##funcbtn", ImVec2(cellW - 5, cellH - 5));
            bool hovered = ImGui::IsItemHovered();
            bool clicked = ImGui::IsItemClicked();
            ImGui::PopID();

            // 处理点击事件
            if (clicked && i == 0) {  // 签到按钮
                if (IsLoggedIn()) {
                    if (auto* app = Application::GetInstance()) {
                        app->OpenCheckInPanel();
                    }
                } else {
                    ShowToast(u8"请先登录", ToastType::Warning);
                }
            }
            if (clicked && i == 1) {  // 抽奖按钮
                if (IsLoggedIn()) {
                    if (auto* app = Application::GetInstance()) {
                        app->OpenLotteryPanel();
                    }
                } else {
                    ShowToast(u8"请先登录", ToastType::Warning);
                }
            }
            if (clicked && i == 2) {  // 任务按钮
                if (IsLoggedIn()) {
                    if (auto* app = Application::GetInstance()) {
                        app->OpenTaskPanel();
                    }
                } else {
                    ShowToast(u8"请先登录", ToastType::Warning);
                }
            }
            if (clicked && i == 3) {  // 商城按钮
                if (IsLoggedIn()) {
                    if (auto* app = Application::GetInstance()) {
                        app->OpenShopPanel();
                    }
                } else {
                    ShowToast(u8"请先登录", ToastType::Warning);
                }
            }

            // 单元格背景
            ImU32 cellBg = hovered ? IM_COL32(45, 55, 72, 255) : IM_COL32(35, 45, 60, 200);
            dl->AddRectFilled(ImVec2(cellX, cellY), ImVec2(cellX + cellW - 5, cellY + cellH - 5),
                              cellBg, 8.0f);

            // 悬停边框
            if (hovered) {
                dl->AddRect(ImVec2(cellX, cellY), ImVec2(cellX + cellW - 5, cellY + cellH - 5),
                            funcs[i].iconColor, 8.0f, 0, 1.5f);
            }

            // 图标
            float iconCx = cellX + (cellW - 5) / 2;
            float iconCy = cellY + (cellH - 5) / 2 - 8;
            funcs[i].drawIcon(dl, iconCx, iconCy, 48.0f, funcs[i].iconColor);

            // 标签
            ImVec2 labelSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, funcs[i].label);
            float labelX = cellX + ((cellW - 5) - labelSize.x) / 2;
            float labelY = cellY + cellH - 22;
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(labelX, labelY),
                        IM_COL32(200, 210, 225, 255), funcs[i].label);
        }

        float cardH = funcCardH;

        // ═══════════════════════════════════════════════════════════════
        //  分类卡片（带游戏图片背景）
        // ═══════════════════════════════════════════════════════════════
        float catY = cardY + cardH + 16;
        float catCardH = 90.0f;
        float catCardGap = 10.0f;

        struct CategoryCard {
            const char* label;
            const char* appId;  // 用于获取游戏图片
            StoreCategory cat;
            ImU32 overlayColor;
            ImU32 accentColor;
        };
        CategoryCard cards[] = {
            {u8"畅玩游戏", "2358720", StoreCategory::FreePlay,   // Black Myth Wukong
             IM_COL32(0, 0, 0, 160), IM_COL32(255, 180, 50, 255)},
            {u8"正版游戏", "1245620", StoreCategory::Official,   // Elden Ring
             IM_COL32(0, 0, 0, 160), IM_COL32(102, 192, 244, 255)},
            {u8"离线游戏", "1091500", StoreCategory::Offline,    // Cyberpunk 2077
             IM_COL32(0, 0, 0, 160), IM_COL32(87, 203, 100, 255)},
            {u8"盗版破解", "1174180", StoreCategory::Pirated,    // Red Dead Redemption 2
             IM_COL32(0, 0, 0, 160), IM_COL32(255, 100, 100, 255)},
        };

        // 请求图片
        for (int i = 0; i < 4; i++) {
            RequestGameTexture(cards[i].appId);
        }

        for (int i = 0; i < 4; i++) {
            float cy = catY + i * (catCardH + catCardGap);
            bool isActive = (currentCategory_ == cards[i].cat);

            ImGui::SetCursorScreenPos(ImVec2(cardX, cy));
            ImGui::PushID(i + 1000);
            ImGui::InvisibleButton("##catcard", ImVec2(cardW, catCardH));
            bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                currentCategory_ = cards[i].cat;
            }
            ImGui::PopID();

            // 卡片背景 - 先画底色
            dl->AddRectFilled(ImVec2(cardX, cy), ImVec2(cardX + cardW, cy + catCardH),
                              IM_COL32(30, 35, 45, 255), 8.0f);

            // 游戏图片背景
            ImTextureID tex = GetGameTexture(cards[i].appId);
            if (tex) {
                // 裁剪区域
                ImVec2 clipMin(cardX, cy);
                ImVec2 clipMax(cardX + cardW, cy + catCardH);
                dl->PushClipRect(clipMin, clipMax, true);

                // 图片填充整个卡片（cover模式）
                float imgW = cardW + 40;  // 稍微放大以覆盖
                float imgH = catCardH + 20;
                float imgX = cardX - 10;
                float imgY = cy - 5;
                dl->AddImageRounded(tex, ImVec2(imgX, imgY), ImVec2(imgX + imgW, imgY + imgH),
                                    ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 200), 8.0f);

                dl->PopClipRect();
            }

            // 暗色遮罩层
            ImU32 overlay = cards[i].overlayColor;
            if (hovered) overlay = IM_COL32(0, 0, 0, 120);
            if (isActive) overlay = IM_COL32(0, 0, 0, 100);
            dl->AddRectFilled(ImVec2(cardX, cy), ImVec2(cardX + cardW, cy + catCardH), overlay, 8.0f);

            // 选中边框
            if (isActive) {
                dl->AddRect(ImVec2(cardX, cy), ImVec2(cardX + cardW, cy + catCardH),
                            cards[i].accentColor, 8.0f, 0, 2.5f);
            }

            // 标题
            ImU32 titleColor = IM_COL32(255, 255, 255, 255);
            dl->AddText(largeFont, largeFont->FontSize, ImVec2(cardX + 14, cy + catCardH / 2 - largeFont->FontSize / 2),
                        titleColor, cards[i].label);

            // 悬停边框
            if (hovered && !isActive) {
                dl->AddRect(ImVec2(cardX, cy), ImVec2(cardX + cardW, cy + catCardH),
                            IM_COL32(255, 255, 255, 80), 8.0f, 0, 1.5f);
            }
        }

        // ═══════════════════════════════════════════════════════════════
        //  微信群二维码卡片
        // ═══════════════════════════════════════════════════════════════
        float qrY = catY + 4 * (catCardH + catCardGap);
        float qrCardH = 130.0f;

        // 卡片背景 - 玻璃效果
        dl->AddRectFilled(ImVec2(cardX, qrY), ImVec2(cardX + cardW, qrY + qrCardH),
                          IM_COL32(30, 38, 52, 220), 10.0f);
        // 顶部高光
        dl->AddRectFilledMultiColor(
            ImVec2(cardX, qrY), ImVec2(cardX + cardW, qrY + 20),
            IM_COL32(87, 203, 100, 15), IM_COL32(87, 203, 100, 15),
            IM_COL32(87, 203, 100, 0), IM_COL32(87, 203, 100, 0));
        // 边框
        dl->AddRect(ImVec2(cardX, qrY), ImVec2(cardX + cardW, qrY + qrCardH),
                    IM_COL32(87, 203, 100, 60), 10.0f, 0, 1.0f);

        // 二维码区域
        float qrSize = 85.0f;
        float qrX = cardX + 12;
        float qrImgY = qrY + (qrCardH - qrSize) / 2;

        // 二维码背景（白色）
        dl->AddRectFilled(ImVec2(qrX - 4, qrImgY - 4), ImVec2(qrX + qrSize + 4, qrImgY + qrSize + 4),
                          IM_COL32(255, 255, 255, 255), 5.0f);

        // 尝试加载二维码图片
        static ImTextureID qrTexture = (ImTextureID)0;
        static bool qrLoaded = false;
        if (!qrLoaded) {
            // 尝试从 resources/icons/wechat_qr.png 加载
            qrLoaded = true;
            // 图片会通过外部加载机制处理
        }

        // 如果有二维码图片则显示，否则显示占位符
        if (qrTexture != (ImTextureID)0) {
            dl->AddImageRounded(qrTexture, ImVec2(qrX, qrImgY), ImVec2(qrX + qrSize, qrImgY + qrSize),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
        } else {
            // 占位符 - 绘制微信图标
            float iconCx = qrX + qrSize / 2;
            float iconCy = qrImgY + qrSize / 2;

            // 微信图标 - 两个气泡（缩小）
            dl->AddCircleFilled(ImVec2(iconCx - 10, iconCy - 3), 20.0f, IM_COL32(87, 203, 100, 255));
            dl->AddCircleFilled(ImVec2(iconCx + 12, iconCy + 5), 15.0f, IM_COL32(87, 203, 100, 255));

            // 眼睛
            dl->AddCircleFilled(ImVec2(iconCx - 15, iconCy - 7), 3.0f, IM_COL32(255, 255, 255, 255));
            dl->AddCircleFilled(ImVec2(iconCx - 5, iconCy - 7), 3.0f, IM_COL32(255, 255, 255, 255));
            dl->AddCircleFilled(ImVec2(iconCx + 8, iconCy + 2), 2.0f, IM_COL32(255, 255, 255, 255));
            dl->AddCircleFilled(ImVec2(iconCx + 16, iconCy + 2), 2.0f, IM_COL32(255, 255, 255, 255));
        }

        // 右侧文字
        float textX = qrX + qrSize + 15;
        float textCenterY = qrY + qrCardH / 2;

        const char* qrTitle = u8"加入微信群";
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(textX, textCenterY - 18),
                    IM_COL32(87, 203, 100, 255), qrTitle);

        const char* scanTip = u8"扫码加群交流";
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(textX, textCenterY + 2),
                    IM_COL32(140, 155, 175, 200), scanTip);
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void StorePage::RenderMainContent(float x, float y, float width, float height) {
    // 如果不是正版游戏分类，显示特殊内容
    if (currentCategory_ != StoreCategory::Official) {
        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(width, height));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

        if (ImGui::Begin("##StoreSpecialContent", nullptr, flags)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 winPos = ImGui::GetWindowPos();
            ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
            ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;
            ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;

            if (currentCategory_ == StoreCategory::FreePlay) {
                // ═══════════════════════════════════════════════════════════
                //  畅玩游戏页面
                // ═══════════════════════════════════════════════════════════
                float contentPadding = 20.0f;
                float topBarH = 280.0f;  // 顶部区域高度 - 再增加高度

                // 热门游戏数据
                struct HotGame {
                    const char* name;
                    const char* appId;
                    const char* desc;
                };
                HotGame hotGames[] = {
                    {u8"Black Myth: Wukong", "2358720", u8"国产3A大作，西游记题材动作游戏"},
                    {u8"Elden Ring", "1245620", u8"开放世界魂系游戏，宫崎英高新作"},
                    {u8"Cyberpunk 2077", "1091500", u8"赛博朋克风格开放世界RPG"},
                    {u8"GTA V", "271590", u8"经典开放世界犯罪游戏"},
                    {u8"Red Dead Redemption 2", "1174180", u8"西部题材开放世界冒险"},
                };
                int hotCount = sizeof(hotGames) / sizeof(hotGames[0]);

                // 请求热门游戏图片
                for (int i = 0; i < hotCount; i++) {
                    RequestGameTexture(hotGames[i].appId);
                }

                // 轮播计时器
                float dt = ImGui::GetIO().DeltaTime;
                freeplayCarouselTimer_ += dt;
                if (freeplayCarouselTimer_ > 4.0f) {
                    freeplayCarouselTimer_ = 0.0f;
                    freeplayCarouselIdx_ = (freeplayCarouselIdx_ + 1) % hotCount;
                }

                ImVec2 contentPos = ImGui::GetCursorScreenPos();
                float cx = contentPos.x + contentPadding;
                float cy = contentPos.y + contentPadding;
                float contentW = width - contentPadding * 2 - 48;

                // ─────────────────────────────────────────────────────────
                //  顶部：轮播图（左） + 搜索栏（右）
                // ─────────────────────────────────────────────────────────
                float carouselW = contentW * 0.65f;
                float searchW = contentW - carouselW - 16;
                float carouselH = topBarH - 20;  // 轮播图高度 260px

                // 轮播图区域
                HotGame& current = hotGames[freeplayCarouselIdx_];
                ImTextureID tex = GetGameTexture(current.appId);

                // 轮播背景
                dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + carouselW, cy + carouselH),
                                  IM_COL32(30, 35, 45, 255), 12.0f);

                if (tex) {
                    dl->PushClipRect(ImVec2(cx, cy), ImVec2(cx + carouselW, cy + carouselH), true);
                    dl->AddImageRounded(tex, ImVec2(cx, cy), ImVec2(cx + carouselW, cy + carouselH),
                                        ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 12.0f);
                    dl->PopClipRect();

                    // 底部渐变遮罩
                    dl->AddRectFilledMultiColor(
                        ImVec2(cx, cy + carouselH - 80), ImVec2(cx + carouselW, cy + carouselH),
                        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                        IM_COL32(0, 0, 0, 230), IM_COL32(0, 0, 0, 230));
                }

                // 游戏名称
                dl->AddText(largeFont, largeFont->FontSize, ImVec2(cx + 16, cy + carouselH - 55),
                            IM_COL32(255, 255, 255, 255), current.name);
                // 游戏描述
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + 16, cy + carouselH - 30),
                            IM_COL32(200, 210, 220, 200), current.desc);

                // 轮播指示点
                float dotX = cx + carouselW / 2 - (hotCount * 12) / 2;
                float dotY = cy + carouselH - 12;
                for (int i = 0; i < hotCount; i++) {
                    ImU32 dotColor = (i == freeplayCarouselIdx_) ? IM_COL32(255, 180, 50, 255) : IM_COL32(255, 255, 255, 100);
                    dl->AddCircleFilled(ImVec2(dotX + i * 12, dotY), 3.0f, dotColor);
                }

                // 左右箭头
                ImGui::SetCursorScreenPos(ImVec2(cx + 8, cy + carouselH / 2 - 15));
                ImGui::PushID("carousel_left");
                ImGui::InvisibleButton("##left", ImVec2(30, 30));
                if (ImGui::IsItemHovered()) {
                    dl->AddCircleFilled(ImVec2(cx + 23, cy + carouselH / 2), 15.0f, IM_COL32(0, 0, 0, 120));
                    dl->AddTriangleFilled(
                        ImVec2(cx + 28, cy + carouselH / 2 - 6),
                        ImVec2(cx + 28, cy + carouselH / 2 + 6),
                        ImVec2(cx + 18, cy + carouselH / 2),
                        IM_COL32(255, 255, 255, 200));
                }
                if (ImGui::IsItemClicked()) {
                    freeplayCarouselIdx_ = (freeplayCarouselIdx_ - 1 + hotCount) % hotCount;
                    freeplayCarouselTimer_ = 0;
                }
                ImGui::PopID();

                ImGui::SetCursorScreenPos(ImVec2(cx + carouselW - 38, cy + carouselH / 2 - 15));
                ImGui::PushID("carousel_right");
                ImGui::InvisibleButton("##right", ImVec2(30, 30));
                if (ImGui::IsItemHovered()) {
                    dl->AddCircleFilled(ImVec2(cx + carouselW - 23, cy + carouselH / 2), 15.0f, IM_COL32(0, 0, 0, 120));
                    dl->AddTriangleFilled(
                        ImVec2(cx + carouselW - 28, cy + carouselH / 2 - 6),
                        ImVec2(cx + carouselW - 28, cy + carouselH / 2 + 6),
                        ImVec2(cx + carouselW - 18, cy + carouselH / 2),
                        IM_COL32(255, 255, 255, 200));
                }
                if (ImGui::IsItemClicked()) {
                    freeplayCarouselIdx_ = (freeplayCarouselIdx_ + 1) % hotCount;
                    freeplayCarouselTimer_ = 0;
                }
                ImGui::PopID();

                // ─────────────────────────────────────────────────────────
                //  右侧：搜索栏 + 快捷入口
                // ─────────────────────────────────────────────────────────
                float searchX = cx + carouselW + 16;
                float searchY = cy;

                // 搜索框背景
                dl->AddRectFilled(ImVec2(searchX, searchY), ImVec2(searchX + searchW, searchY + 40),
                                  IM_COL32(35, 42, 55, 255), 8.0f);
                dl->AddRect(ImVec2(searchX, searchY), ImVec2(searchX + searchW, searchY + 40),
                            IM_COL32(60, 75, 95, 150), 8.0f);

                // 搜索图标
                float iconCx = searchX + 20;
                float iconCy = searchY + 20;
                dl->AddCircle(ImVec2(iconCx, iconCy), 8.0f, IM_COL32(140, 155, 175, 200), 12, 1.5f);
                dl->AddLine(ImVec2(iconCx + 6, iconCy + 6), ImVec2(iconCx + 11, iconCy + 11),
                            IM_COL32(140, 155, 175, 200), 1.5f);

                // 搜索输入框
                ImGui::SetCursorScreenPos(ImVec2(searchX + 36, searchY + 8));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 230, 240, 255));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 4));
                ImGui::PushItemWidth(searchW - 50);
                ImGui::InputTextWithHint("##freeplay_search", u8"搜索游戏...", freeplaySearchBuf_, sizeof(freeplaySearchBuf_));
                ImGui::PopItemWidth();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);

                // 快捷分类标签
                float tagY = searchY + 52;
                const char* tags[] = {u8"动作", u8"角色扮演", u8"冒险", u8"策略"};
                float tagX = searchX;
                for (int i = 0; i < 4; i++) {
                    ImVec2 tagSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, tags[i]);
                    float tw = tagSize.x + 16;
                    float th = 26;

                    ImGui::SetCursorScreenPos(ImVec2(tagX, tagY));
                    ImGui::PushID(i + 2000);
                    ImGui::InvisibleButton("##tag", ImVec2(tw, th));
                    bool tagHovered = ImGui::IsItemHovered();
                    ImGui::PopID();

                    ImU32 tagBg = tagHovered ? IM_COL32(255, 180, 50, 60) : IM_COL32(45, 55, 70, 255);
                    ImU32 tagBorder = tagHovered ? IM_COL32(255, 180, 50, 150) : IM_COL32(70, 85, 105, 150);
                    dl->AddRectFilled(ImVec2(tagX, tagY), ImVec2(tagX + tw, tagY + th), tagBg, 6.0f);
                    dl->AddRect(ImVec2(tagX, tagY), ImVec2(tagX + tw, tagY + th), tagBorder, 6.0f);
                    dl->AddText(smallFont, smallFont->FontSize, ImVec2(tagX + 8, tagY + 5),
                                IM_COL32(200, 210, 225, 255), tags[i]);

                    tagX += tw + 8;
                    if (tagX + 60 > searchX + searchW) {
                        tagX = searchX;
                        tagY += th + 8;
                    }
                }

                // 统计信息 - 玻璃卡片效果
                float infoY = searchY + carouselH - 80;  // 上移
                float glassW = searchW;
                float glassH = 65.0f;

                // 玻璃卡片背景
                dl->AddRectFilled(ImVec2(searchX, infoY), ImVec2(searchX + glassW, infoY + glassH),
                                  IM_COL32(255, 255, 255, 8), 10.0f);
                // 玻璃边框
                dl->AddRect(ImVec2(searchX, infoY), ImVec2(searchX + glassW, infoY + glassH),
                            IM_COL32(255, 255, 255, 25), 10.0f);
                // 顶部高光
                dl->AddRectFilledMultiColor(
                    ImVec2(searchX + 1, infoY + 1), ImVec2(searchX + glassW - 1, infoY + 20),
                    IM_COL32(255, 255, 255, 15), IM_COL32(255, 255, 255, 15),
                    IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

                // 分隔线
                float dividerX = searchX + glassW / 2;
                dl->AddLine(ImVec2(dividerX, infoY + 12), ImVec2(dividerX, infoY + glassH - 12),
                            IM_COL32(255, 255, 255, 30));

                // 左侧统计：可畅玩游戏 - 水平居中
                float halfW = glassW / 2;
                const char* label1 = u8"可畅玩游戏";
                const char* value1 = "2,888";
                ImVec2 label1Size = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, label1);
                ImVec2 value1Size = largeFont->CalcTextSizeA(largeFont->FontSize, FLT_MAX, 0, value1);
                float stat1LabelX = searchX + (halfW - label1Size.x) / 2;
                float stat1ValueX = searchX + (halfW - value1Size.x) / 2;
                float labelY = infoY + 12;
                float valueY = infoY + 32;
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(stat1LabelX, labelY),
                            IM_COL32(180, 190, 205, 200), label1);
                dl->AddText(largeFont, largeFont->FontSize, ImVec2(stat1ValueX, valueY),
                            IM_COL32(255, 180, 50, 255), value1);

                // 右侧统计：在线账号 - 水平居中
                const char* label2 = u8"在线账号";
                char onlineBuf[32];
                snprintf(onlineBuf, sizeof(onlineBuf), "%d", onlineUsers_);
                ImVec2 label2Size = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, label2);
                ImVec2 value2Size = largeFont->CalcTextSizeA(largeFont->FontSize, FLT_MAX, 0, onlineBuf);
                float stat2LabelX = dividerX + (halfW - label2Size.x) / 2;
                float stat2ValueX = dividerX + (halfW - value2Size.x) / 2;
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(stat2LabelX, labelY),
                            IM_COL32(180, 190, 205, 200), label2);
                dl->AddText(largeFont, largeFont->FontSize, ImVec2(stat2ValueX, valueY),
                            IM_COL32(87, 203, 100, 255), onlineBuf);

                // ─────────────────────────────────────────────────────────
                //  游戏列表区域
                // ─────────────────────────────────────────────────────────
                float listY = cy + topBarH;
                ImGui::SetCursorScreenPos(ImVec2(cx, listY));

                // 标题
                dl->AddText(font, font->FontSize, ImVec2(cx, listY),
                            IM_COL32(255, 255, 255, 255), u8"全部游戏");

                // 游戏列表 - 产品展示风格
                float gameListY = listY + 35;
                float itemH = 120.0f;  // 每个游戏项高度
                float itemGap = 12.0f;
                float coverW = 200.0f;  // 封面宽度

                struct FreePlayGame {
                    const char* name;
                    const char* appId;
                    const char* desc;
                    const char* tags;
                };
                FreePlayGame games[] = {
                    {u8"Black Myth: Wukong", "2358720", u8"国产3A动作游戏，西游记题材", u8"动作 · 冒险 · 单人"},
                    {u8"Elden Ring", "1245620", u8"开放世界魂系游戏，宫崎英高新作", u8"动作 · RPG · 开放世界"},
                    {u8"Cyberpunk 2077", "1091500", u8"赛博朋克风格开放世界角色扮演", u8"RPG · 开放世界 · 科幻"},
                    {u8"Red Dead Redemption 2", "1174180", u8"西部题材开放世界冒险游戏", u8"冒险 · 开放世界 · 西部"},
                    {u8"GTA V", "271590", u8"经典开放世界犯罪动作游戏", u8"动作 · 开放世界 · 犯罪"},
                    {u8"Hogwarts Legacy", "990080", u8"哈利波特魔法世界冒险RPG", u8"RPG · 冒险 · 魔法"},
                    {u8"Baldur's Gate 3", "1086940", u8"经典CRPG续作，回合制战斗", u8"RPG · 回合制 · 奇幻"},
                    {u8"Starfield", "1716740", u8"贝塞斯达太空探索RPG大作", u8"RPG · 太空 · 探索"},
                };
                int gameCount = sizeof(games) / sizeof(games[0]);

                // 请求游戏图片
                for (int i = 0; i < gameCount; i++) {
                    RequestGameTexture(games[i].appId);
                }

                ImGui::SetCursorScreenPos(ImVec2(cx, gameListY));
                ImGui::BeginChild("##freeplayGames", ImVec2(contentW, height - topBarH - 90), false);
                ImDrawList* childDl = ImGui::GetWindowDrawList();

                for (int i = 0; i < gameCount; i++) {
                    ImVec2 itemPos = ImGui::GetCursorScreenPos();
                    float itemW = contentW - 10;

                    ImGui::PushID(i);
                    ImGui::InvisibleButton("##gameitem", ImVec2(itemW, itemH));
                    bool hovered = ImGui::IsItemHovered();
                    ImGui::PopID();

                    // 卡片背景
                    ImU32 bgColor = hovered ? IM_COL32(40, 48, 62, 255) : IM_COL32(30, 38, 50, 255);
                    childDl->AddRectFilled(itemPos, ImVec2(itemPos.x + itemW, itemPos.y + itemH), bgColor, 10.0f);

                    // 左侧：游戏封面
                    float coverH = itemH - 16;
                    float coverX = itemPos.x + 8;
                    float coverY = itemPos.y + 8;

                    childDl->AddRectFilled(ImVec2(coverX, coverY), ImVec2(coverX + coverW, coverY + coverH),
                                           IM_COL32(25, 30, 40, 255), 8.0f);

                    ImTextureID gameTex = GetGameTexture(games[i].appId);
                    if (gameTex) {
                        childDl->PushClipRect(ImVec2(coverX, coverY), ImVec2(coverX + coverW, coverY + coverH), true);
                        childDl->AddImageRounded(gameTex, ImVec2(coverX, coverY), ImVec2(coverX + coverW, coverY + coverH),
                                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 8.0f);
                        childDl->PopClipRect();
                    }

                    // 右侧：游戏信息
                    float infoX = coverX + coverW + 20;
                    float infoY = itemPos.y + 16;

                    // 游戏名称
                    childDl->AddText(largeFont, largeFont->FontSize, ImVec2(infoX, infoY),
                                     IM_COL32(255, 255, 255, 255), games[i].name);

                    // 游戏标签
                    childDl->AddText(smallFont, smallFont->FontSize, ImVec2(infoX, infoY + 28),
                                     IM_COL32(140, 155, 175, 200), games[i].tags);

                    // 游戏描述
                    childDl->AddText(smallFont, smallFont->FontSize, ImVec2(infoX, infoY + 48),
                                     IM_COL32(180, 190, 205, 220), games[i].desc);

                    // 状态标签
                    float statusY = itemPos.y + itemH - 36;
                    const char* statusText = u8"可畅玩";
                    ImVec2 statusSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, statusText);
                    childDl->AddRectFilled(ImVec2(infoX, statusY), ImVec2(infoX + statusSize.x + 14, statusY + 22),
                                           IM_COL32(87, 203, 100, 30), 4.0f);
                    childDl->AddText(smallFont, smallFont->FontSize, ImVec2(infoX + 7, statusY + 4),
                                     IM_COL32(87, 203, 100, 255), statusText);

                    // 畅玩按钮
                    float btnW = 90.0f;
                    float btnH = 36.0f;
                    float btnX = itemPos.x + itemW - btnW - 16;
                    float btnY = itemPos.y + (itemH - btnH) / 2;

                    ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
                    ImGui::PushID(i + 500);
                    ImGui::InvisibleButton("##playbtn", ImVec2(btnW, btnH));
                    bool btnHovered = ImGui::IsItemHovered();
                    ImGui::PopID();

                    ImU32 btnBg = btnHovered ? IM_COL32(255, 200, 80, 255) : IM_COL32(255, 180, 50, 255);
                    childDl->AddRectFilled(ImVec2(btnX, btnY), ImVec2(btnX + btnW, btnY + btnH), btnBg, 8.0f);

                    const char* playText = u8"畅玩";
                    ImVec2 playSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, playText);
                    childDl->AddText(font, font->FontSize,
                                     ImVec2(btnX + (btnW - playSize.x) / 2, btnY + (btnH - font->FontSize) / 2),
                                     IM_COL32(30, 30, 30, 255), playText);

                    // 悬停边框
                    if (hovered) {
                        childDl->AddRect(itemPos, ImVec2(itemPos.x + itemW, itemPos.y + itemH),
                                         IM_COL32(255, 180, 50, 150), 10.0f, 0, 2.0f);
                    }

                    // 移动光标到下一个项目位置（包含间距）
                    ImGui::SetCursorScreenPos(ImVec2(itemPos.x, itemPos.y + itemH + itemGap));
                }

                ImGui::EndChild();

            } else if (currentCategory_ == StoreCategory::Offline) {
                // 离线游戏提示
                ImGui::PushFont(largeFont);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(87, 203, 100, 255));
                ImGui::Text(u8"离线游戏");
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 175, 190, 200));
                ImGui::TextWrapped(u8"离线游戏功能正在开发中...");
                ImGui::Spacing();
                ImGui::TextWrapped(u8"此功能将允许您在没有网络连接的情况下游玩已下载的游戏。");
                ImGui::PopStyleColor();
            } else if (currentCategory_ == StoreCategory::Pirated) {
                // 盗版破解提示
                ImGui::PushFont(largeFont);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
                ImGui::Text(u8"盗版破解");
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 175, 190, 200));
                ImGui::TextWrapped(u8"盗版破解功能正在开发中...");
                ImGui::Spacing();
                ImGui::TextWrapped(u8"此功能将提供破解游戏资源下载。");
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    // 正版游戏分类 - 显示原有的商店内容

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(24, 34, 48, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("##StorePage", nullptr, flags)) {
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));

        float cw = width - 48;

        if (showDetail_) {
            // Detail page in its own child (independent scroll)
            ImGui::BeginChild("##detailScroll", ImVec2(width, height), false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            RenderGameDetail(dl, origin.x, origin.y, width, height);
            ImGui::EndChild();
        } else if (showCategoryDetail_) {
            // Category detail page
            ImGui::BeginChild("##categoryDetailScroll", ImVec2(width, height), false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            RenderCategoryDetail(dl, origin.x, origin.y, width, height);
            ImGui::EndChild();
        } else if (showSearchResults_) {
            // Search results in its own child (independent scroll)
            ImGui::BeginChild("##searchScroll", ImVec2(width, height), false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            float cx = origin.x + 24;
            float contentY = 16;  // track relative content height

            RenderSearchBar(dl, cx, origin.y + contentY, cw);
            contentY += 50;
            RenderSearchResults(dl, cx, origin.y + contentY, cw, height - 66);

            // Calculate actual height after render
            contentY += 36;  // title
            if (searchResultsLoaded_ && !searchResults_.empty()) {
                float rowH = 70.0f, gap = 2.0f;
                contentY += (int)searchResults_.size() * (rowH + gap) + 40;
            } else {
                contentY += 80;
            }
            contentY += 40;  // bottom padding

            ImGui::SetCursorPosY(contentY);
            ImGui::EndChild();
        } else if (featured_.empty() && IsStoreListingsLoading()) {
            // Loading state
            ImGui::BeginChild("##storeLoading", ImVec2(width, height), false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            float cx = origin.x + width * 0.5f;
            float cy = origin.y + height * 0.35f;
            ImFont* font = g_mainFontLarge ? g_mainFontLarge : ImGui::GetFont();
            const char* loadText = u8"\u6B63\u5728\u52A0\u8F7D Steam \u5546\u5E97\u6570\u636E...";
            ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, loadText);
            dl->AddText(font, font->FontSize, ImVec2(cx - ts.x * 0.5f, cy),
                        IM_COL32(102, 192, 244, 200), loadText);
            float t = fmodf((float)ImGui::GetTime(), 1.0f);
            for (int i = 0; i < 3; i++) {
                float alpha = (t > i * 0.3f) ? 255.0f : 80.0f;
                dl->AddCircleFilled(ImVec2(cx - 20 + i * 20, cy + 40), 4.0f,
                                    IM_COL32(102, 192, 244, (int)alpha));
            }
            ImGui::Dummy(ImVec2(width, height));
            ImGui::EndChild();
        } else {
            // Main store scrollable area
            ImGui::BeginChild("##storeScroll", ImVec2(width, height), false);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            float cx = origin.x + 24;
            float cy = origin.y + 16;
            float contentY = 16;  // track relative content height

            // Helper for card section height
            auto calcCardSectionH = [&](int count, int cols) {
                float cardW = (cw - (cols - 1) * 10.0f) / (float)cols;
                float cardH = cardW * 0.5625f + 65.0f;
                int rows = (count + cols - 1) / cols;
                return 36.0f + rows * (cardH + 10.0f) + 10.0f;
            };

            // Search bar at top
            RenderSearchBar(dl, cx, cy, cw);
            cy += 50;
            contentY += 50;

            // Normal store layout
            if (!featured_.empty()) {
                RenderFeaturedBanner(dl, cx, cy, cw);
                cy += 400;  // Increased from 360 to add more space
                contentY += 400;
            }

            dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + 1),
                IM_COL32(80,100,120,0), IM_COL32(80,100,120,60), IM_COL32(80,100,120,60), IM_COL32(80,100,120,0));
            cy += 50;  // Increased from 30 to add more space before specials
            contentY += 50;

            if (!specials_.empty()) {
                RenderSpecialOffers(dl, cx, cy, cw);
                int specRows = (std::min(9, (int)specials_.size()) + 2) / 3;
                float h = 35 + specRows * (150 + 14) + 15;  // Updated for larger card height (150) and gap (14)
                cy += h;
                contentY += h;
            }

            RenderCategories(dl, cx, cy, cw);
            // Single row of 4 cards + title + dots: title=40, cardH=140, dots=24
            float catH = 40 + 140 + 24;
            cy += catH;
            contentY += catH;

            if (!newReleases_.empty()) {
                RenderNewReleases(dl, cx, cy, cw);
                float h = calcCardSectionH(std::min(8, (int)newReleases_.size()), 4);
                cy += h;
                contentY += h;
            }

            if (!trending_.empty()) {
                RenderTrending(dl, cx, cy, cw);
                float h = calcCardSectionH(std::min(8, (int)trending_.size()), 4);
                cy += h;
                contentY += h;
            }

            if (!comingSoon_.empty()) {
                RenderComingSoon(dl, cx, cy, cw);
                float h = calcCardSectionH(std::min(4, (int)comingSoon_.size()), 4);
                cy += h;
                contentY += h;
            }

            // Divider
            dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cw, cy + 1),
                IM_COL32(80,100,120,0), IM_COL32(80,100,120,80), IM_COL32(80,100,120,80), IM_COL32(80,100,120,0));
            cy += 20;
            contentY += 20;

            // Browse all games - cy is modified by reference
            float cyBefore = cy;
            RenderBrowseAll(dl, cx, cy, cw);
            contentY += (cy - cyBefore);

            contentY += 40;  // bottom padding
            ImGui::SetCursorPosY(contentY);
            ImGui::EndChild();
        }

        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// Helper: draw a rounded button, returns true if clicked
static bool StyledButton(ImDrawList* dl, const char* id, const char* label,
                         float x, float y, float w, float h,
                         ImU32 bgNormal, ImU32 bgHover, ImU32 textCol,
                         float rounding = 4.0f, const char* iconStr = nullptr) {
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::PushID(id);
    ImGui::InvisibleButton("##sb", ImVec2(w, h));
    bool hov = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImGui::PopID();

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), hov ? bgHover : bgNormal, rounding);

    float textX = x;
    if (iconStr && g_iconFont) {
        DrawIcon(dl, iconStr, ImVec2(x + 12, y + (h - 14) * 0.5f), textCol);
        textX += 30;
    }
    ImVec2 ts = ImGui::CalcTextSize(label);
    float labelX = iconStr ? textX : x + (w - ts.x) * 0.5f;
    float labelY = y + (h - ts.y) * 0.5f;
    dl->AddText(ImVec2(labelX, labelY), textCol, label);
    return clicked;
}

// Helper: section header with blue accent line
static void SectionHeader(ImDrawList* dl, const char* text, float x, float y, float width) {
    if (g_mainFontLarge)
        dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                    ImVec2(x, y), IM_COL32(255, 255, 255, 240), text);
    float lineY = y + (g_mainFontLarge ? g_mainFontLarge->FontSize : 18.0f) + 6;
    dl->AddRectFilledMultiColor(ImVec2(x, lineY), ImVec2(x + width * 0.4f, lineY + 2),
        IM_COL32(26, 159, 255, 180), IM_COL32(26, 159, 255, 0),
        IM_COL32(26, 159, 255, 0), IM_COL32(26, 159, 255, 180));
}

void StorePage::RenderGameDetail(ImDrawList* dl, float x, float y, float width, float height) {
    const StoreGame* game = FindGameByAppId(selectedAppId_);

    // Debug: log selected appId
    static std::string lastLoggedAppId;
    if (lastLoggedAppId != selectedAppId_) {
        lastLoggedAppId = selectedAppId_;
        fprintf(stderr, "[DETAIL] RenderGameDetail for appId: %s\n", selectedAppId_.c_str());
    }

    // If game not in any list, create a temporary placeholder from Steam data
    static StoreGame tempGame;
    if (!game) {
        const SteamStoreData* sdCheck = GetStoreData(selectedAppId_);
        if (sdCheck) {
            tempGame.appId = selectedAppId_;
            tempGame.name = sdCheck->name;
            tempGame.price = sdCheck->priceFormatted;
            game = &tempGame;
        } else if (!IsStoreDataLoading(selectedAppId_)) {
            // Not loading and not cached - nothing to show
            if (playingVideo_) { StopVideo(); playingVideo_ = false; playingMovieIdx_ = -1; }
            showDetail_ = false;
            return;
        } else {
            // Still loading - create placeholder with appId
            tempGame.appId = selectedAppId_;
            tempGame.name = u8"\u52A0\u8F7D\u4E2D..."; // 加载中...
            game = &tempGame;
        }
    }

    // Fetch real Steam data (async)
    const SteamStoreData* sd = GetStoreData(selectedAppId_);
    bool loading = IsStoreDataLoading(selectedAppId_);

    // When store data arrives, request all screenshot/movie/header textures once
    if (sd && !detailTexturesRequested_) {
        detailTexturesRequested_ = true;

        // Header image from API (more reliable than CDN for some games)
        if (!sd->headerImage.empty())
            RequestTextureFromUrl("hdr_" + selectedAppId_, sd->headerImage);

        // Screenshots
        for (int i = 0; i < (int)sd->screenshotUrls.size(); i++) {
            std::string key = "ss_" + selectedAppId_ + "_" + std::to_string(i);
            RequestTextureFromUrl(key, sd->screenshotUrls[i]);
        }

        // Movie thumbnails
        for (int i = 0; i < (int)sd->movies.size(); i++) {
            if (!sd->movies[i].thumbnailUrl.empty()) {
                std::string key = "mov_" + selectedAppId_ + "_" + std::to_string(i);
                RequestTextureFromUrl(key, sd->movies[i].thumbnailUrl);
            }
        }

        // About rich content images
        int aboutImgIdx = 0;
        for (auto& seg : sd->aboutRich) {
            if (seg.type == SteamStoreData::RichSegment::IMAGE && !seg.text.empty()) {
                std::string key = "about_" + selectedAppId_ + "_" + std::to_string(aboutImgIdx++);
                RequestTextureFromUrl(key, seg.text);
            }
        }
        fprintf(stderr, "[DETAIL] Requested textures for %s: screenshots=%d, movies=%d, aboutImages=%d, aboutRich=%d\n",
                selectedAppId_.c_str(), (int)sd->screenshotUrls.size(), (int)sd->movies.size(),
                aboutImgIdx, (int)sd->aboutRich.size());
    }

    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : ImGui::GetFont();
    ImFont* mainFont = g_mainFont ? g_mainFont : ImGui::GetFont();

    float pad = 28.0f;
    float cx = x + pad;
    float cy = y + 14;
    float cw = width - pad * 2;

    // Use real name if available
    const char* gameName = sd ? sd->name.c_str() : game->name.c_str();

    // ──── Back button + breadcrumb ────
    if (StyledButton(dl, "detail_back", u8"\u8FD4\u56DE\u5546\u5E97", cx, cy, 120, 30,
                     IM_COL32(30, 44, 62, 255), IM_COL32(45, 65, 90, 255),
                     IM_COL32(102, 192, 244, 230), 15.0f, icon::CHEVLEFT)) {
        if (playingVideo_) { StopVideo(); playingVideo_ = false; playingMovieIdx_ = -1; }
        showDetail_ = false;
    }

    dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + 134, cy + 8),
                IM_COL32(100, 120, 140, 160), u8"\u5546\u5E97");
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + 170, cy + 8),
                IM_COL32(60, 80, 100, 120), ">");
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + 184, cy + 8),
                IM_COL32(180, 200, 220, 200), gameName);
    cy += 42;

    // ──── Top: Steam-style layout (Left=screenshots, Right=info card) ────
    float rightW = 350.0f;
    float leftW = cw - rightW - 20;
    float leftX = cx;
    float rightX = cx + leftW + 20;

    // ── LEFT: Main screenshot + thumbnail strip ──
    ImTextureID tex = GetBestTexture(game->appId);
    float mainSSH = leftW * 0.5625f; // 16:9

    // Try to show a real screenshot as main image (based on selected index)
    ImTextureID mainSSTex = (ImTextureID)0;
    if (sd && !sd->screenshotUrls.empty()) {
        int ssIdx = selectedSSIdx_;
        if (ssIdx >= (int)sd->screenshotUrls.size()) ssIdx = 0;
        std::string ssKey = "ss_" + selectedAppId_ + "_" + std::to_string(ssIdx);
        mainSSTex = GetTextureByKey(ssKey);
    }

    DrawShadow(dl, leftX, cy, leftW, mainSSH, 10.0f);

    // Show video frame if playing, otherwise show screenshot
    ImTextureID videoTex = (ImTextureID)0;
    if (playingVideo_) {
        int vw, vh;
        videoTex = GetVideoFrame(&vw, &vh);
    }

    // Check if video ended
    if (playingVideo_ && !IsVideoActive()) {
        playingVideo_ = false;
        playingMovieIdx_ = -1;
    }

    if (playingVideo_ && videoTex) {
        dl->AddImageRounded(videoTex, ImVec2(leftX, cy), ImVec2(leftX + leftW, cy + mainSSH),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);

        // ── Video controls bar at bottom of video ──
        float barH = 36.0f;
        float barY = cy + mainSSH - barH;
        float barX = leftX;
        float barW = leftW;

        // Semi-transparent background
        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                          IM_COL32(0, 0, 0, 180), 0, ImDrawFlags_RoundCornersBottom);

        // Pause/Play button (left side)
        float btnX = barX + 8;
        float btnY = barY + 8;
        float btnSz = 20.0f;
        ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
        ImGui::PushID("vidpause");
        ImGui::InvisibleButton("##vp", ImVec2(btnSz, btnSz));
        bool pauseHov = ImGui::IsItemHovered();

        if (IsVideoPaused()) {
            // Draw play triangle
            ImVec2 tri[3] = {
                ImVec2(btnX + 4, btnY + 2), ImVec2(btnX + 4, btnY + btnSz - 2), ImVec2(btnX + btnSz - 2, btnY + btnSz * 0.5f)
            };
            dl->AddTriangleFilled(tri[0], tri[1], tri[2],
                                  pauseHov ? IM_COL32(102, 192, 244, 255) : IM_COL32(255, 255, 255, 220));
        } else {
            // Draw pause bars
            ImU32 pc = pauseHov ? IM_COL32(102, 192, 244, 255) : IM_COL32(255, 255, 255, 220);
            dl->AddRectFilled(ImVec2(btnX + 3, btnY + 2), ImVec2(btnX + 8, btnY + btnSz - 2), pc);
            dl->AddRectFilled(ImVec2(btnX + 12, btnY + 2), ImVec2(btnX + 17, btnY + btnSz - 2), pc);
        }
        if (ImGui::IsItemClicked()) {
            if (IsVideoPaused()) ResumeVideo();
            else PauseVideo();
        }
        ImGui::PopID();

        // Stop button
        float stopX = btnX + btnSz + 8;
        ImGui::SetCursorScreenPos(ImVec2(stopX, btnY));
        ImGui::PushID("vidstop");
        ImGui::InvisibleButton("##vs", ImVec2(btnSz, btnSz));
        bool stopHov = ImGui::IsItemHovered();
        dl->AddRectFilled(ImVec2(stopX + 3, btnY + 3), ImVec2(stopX + btnSz - 3, btnY + btnSz - 3),
                          stopHov ? IM_COL32(200, 80, 80, 255) : IM_COL32(255, 255, 255, 200));
        if (ImGui::IsItemClicked()) {
            StopVideo();
            playingVideo_ = false;
            playingMovieIdx_ = -1;
        }
        ImGui::PopID();

        // Progress bar
        double duration = GetVideoDuration();
        double position = GetVideoPosition();
        float progX = stopX + btnSz + 12;
        float progW = barX + barW - progX - 60; // leave room for time text
        float progY = barY + barH * 0.5f - 3;
        float progH = 6.0f;

        if (duration > 0 && progW > 20) {
            float pct = (float)(position / duration);
            if (pct > 1.0f) pct = 1.0f;

            // Track background
            dl->AddRectFilled(ImVec2(progX, progY), ImVec2(progX + progW, progY + progH),
                              IM_COL32(60, 60, 60, 200), 3.0f);
            // Filled portion
            if (pct > 0.001f)
                dl->AddRectFilled(ImVec2(progX, progY), ImVec2(progX + progW * pct, progY + progH),
                                  IM_COL32(102, 192, 244, 255), 3.0f);
            // Seek handle
            float handleX = progX + progW * pct;
            dl->AddCircleFilled(ImVec2(handleX, progY + progH * 0.5f), 5.0f,
                                IM_COL32(255, 255, 255, 230), 12);

            // Clickable seek area
            ImGui::SetCursorScreenPos(ImVec2(progX, progY - 6));
            ImGui::PushID("vidseek");
            ImGui::InvisibleButton("##vseek", ImVec2(progW, progH + 12));
            if (ImGui::IsItemActive()) {
                float mx = ImGui::GetIO().MousePos.x;
                float seekPct = (mx - progX) / progW;
                if (seekPct < 0) seekPct = 0;
                if (seekPct > 1) seekPct = 1;
                SeekVideo(seekPct * duration);
            }
            ImGui::PopID();

            // Time text: current / total
            auto fmtTime = [](double secs) -> std::string {
                int m = (int)(secs / 60);
                int s = (int)secs % 60;
                char buf[16];
                snprintf(buf, sizeof(buf), "%d:%02d", m, s);
                return buf;
            };
            std::string timeStr = fmtTime(position) + " / " + fmtTime(duration);
            ImFont* font = ImGui::GetFont();
            float timeX = progX + progW + 8;
            dl->AddText(font, font->FontSize * 0.85f, ImVec2(timeX, barY + 10),
                        IM_COL32(200, 210, 220, 220), timeStr.c_str());
        }

    } else if (playingVideo_ && !videoTex) {
        // Video loading — show loading indicator over black
        dl->AddRectFilled(ImVec2(leftX, cy), ImVec2(leftX + leftW, cy + mainSSH),
                          IM_COL32(10, 10, 10, 255), 6.0f);
        const char* loadTxt = u8"\u52A0\u8F7D\u4E2D...";
        ImFont* font = ImGui::GetFont();
        ImVec2 sz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, loadTxt);
        dl->AddText(font, font->FontSize,
                    ImVec2(leftX + leftW * 0.5f - sz.x * 0.5f, cy + mainSSH * 0.5f - sz.y * 0.5f),
                    IM_COL32(180, 200, 220, 200), loadTxt);
    } else if (mainSSTex) {
        dl->AddImageRounded(mainSSTex, ImVec2(leftX, cy), ImVec2(leftX + leftW, cy + mainSSH),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
    } else if (tex) {
        dl->AddImageRounded(tex, ImVec2(leftX, cy), ImVec2(leftX + leftW, cy + mainSSH),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
    } else {
        DrawProceduralCover(dl, leftX, cy, leftW, mainSSH, game->appId);
    }
    dl->AddRect(ImVec2(leftX, cy), ImVec2(leftX + leftW, cy + mainSSH),
                IM_COL32(255, 255, 255, 10), 6.0f);

    // Thumbnail strip below main screenshot (movies first, then screenshots)
    float thumbStripY = cy + mainSSH + 8;
    int movieCount = sd ? std::min(3, (int)sd->movies.size()) : 0;
    int ssCount = sd ? std::min(5 - movieCount, (int)sd->screenshotUrls.size()) : 4;
    int thumbCount = movieCount + ssCount;
    if (thumbCount < 2) thumbCount = 4; // fallback
    float thumbGap = 6.0f;
    float thumbW = (leftW - thumbGap * (thumbCount - 1)) / thumbCount;
    float thumbH = thumbW * 0.5625f;

    for (int i = 0; i < thumbCount; i++) {
        float tx = leftX + i * (thumbW + thumbGap);
        ImTextureID ssTex = (ImTextureID)0;
        bool isMovie = (i < movieCount);

        if (isMovie && sd) {
            std::string movKey = "mov_" + selectedAppId_ + "_" + std::to_string(i);
            ssTex = GetTextureByKey(movKey);
        } else if (sd) {
            int ssIdx = i - movieCount;
            if (ssIdx < (int)sd->screenshotUrls.size()) {
                std::string ssKey = "ss_" + selectedAppId_ + "_" + std::to_string(ssIdx);
                ssTex = GetTextureByKey(ssKey);
            }
        }

        if (ssTex) {
            dl->AddImageRounded(ssTex, ImVec2(tx, thumbStripY), ImVec2(tx + thumbW, thumbStripY + thumbH),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
        } else if (tex) {
            float uOff = i * 0.06f;
            dl->AddImageRounded(tex, ImVec2(tx, thumbStripY), ImVec2(tx + thumbW, thumbStripY + thumbH),
                                 ImVec2(uOff, 0.05f), ImVec2(0.75f + uOff, 0.85f),
                                 IM_COL32(255, 255, 255, 200), 4.0f);
        } else {
            DrawProceduralCover(dl, tx, thumbStripY, thumbW, thumbH, game->appId);
        }

        // Play button overlay for movies
        if (isMovie) {
            float pcx = tx + thumbW * 0.5f;
            float pcy = thumbStripY + thumbH * 0.5f;
            float pr = 14.0f;
            dl->AddCircleFilled(ImVec2(pcx, pcy), pr, IM_COL32(0, 0, 0, 160), 24);
            dl->AddCircle(ImVec2(pcx, pcy), pr, IM_COL32(255, 255, 255, 200), 24, 1.5f);
            // Triangle play icon
            ImVec2 tri[3] = {
                ImVec2(pcx - 5, pcy - 7), ImVec2(pcx - 5, pcy + 7), ImVec2(pcx + 7, pcy)
            };
            dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 255, 255, 220));
        }

        dl->AddRect(ImVec2(tx, thumbStripY), ImVec2(tx + thumbW, thumbStripY + thumbH),
                    IM_COL32(255, 255, 255, 12), 4.0f);

        // Hover + click
        char tid[16]; snprintf(tid, sizeof(tid), "ssth_%d", i);
        ImGui::SetCursorScreenPos(ImVec2(tx, thumbStripY));
        ImGui::PushID(tid);
        ImGui::InvisibleButton("##sst", ImVec2(thumbW, thumbH));
        bool thumbHov = ImGui::IsItemHovered();
        bool isPlaying = (isMovie && playingVideo_ && i == playingMovieIdx_);
        bool isSelected = (!isMovie && !playingVideo_ && (i - movieCount) == selectedSSIdx_);
        if (isPlaying || isSelected)
            dl->AddRect(ImVec2(tx - 2, thumbStripY - 2), ImVec2(tx + thumbW + 2, thumbStripY + thumbH + 2),
                        IM_COL32(102, 192, 244, 255), 4.0f, 0, 2.5f);
        else if (thumbHov)
            dl->AddRect(ImVec2(tx - 1, thumbStripY - 1), ImVec2(tx + thumbW + 1, thumbStripY + thumbH + 1),
                        IM_COL32(255, 255, 255, 180), 4.0f, 0, 2.0f);

        if (ImGui::IsItemClicked()) {
            if (isMovie && sd && i < (int)sd->movies.size()) {
                // Click movie thumbnail -> play video in-app
                const auto& mov = sd->movies[i];
                std::string videoUrl;
                if (!mov.mp4Url.empty()) videoUrl = mov.mp4Url;
                else if (!mov.hlsUrl.empty()) videoUrl = mov.hlsUrl;
                else if (!mov.dashUrl.empty()) videoUrl = mov.dashUrl;

                if (!videoUrl.empty()) {
                    if (playingVideo_) StopVideo();
                    if (PlayVideo(videoUrl)) {
                        playingVideo_ = true;
                        playingMovieIdx_ = i;
                    }
                }
            } else if (!isMovie) {
                // Click screenshot thumbnail -> show in main area, stop video
                if (playingVideo_) { StopVideo(); playingVideo_ = false; playingMovieIdx_ = -1; }
                selectedSSIdx_ = i - movieCount;
            }
        }
        ImGui::PopID();
    }

    // ── RIGHT: Game info sidebar (Steam style) ──
    float ry = cy;
    float rPad = 14.0f; // inner padding for sidebar panels

    // ── Header image (larger, 460:215 aspect) ──
    float headerImgH = rightW * (215.0f / 460.0f); // true Steam aspect
    DrawShadow(dl, rightX, ry, rightW, headerImgH, 4.0f);
    if (tex) {
        dl->AddImageRounded(tex, ImVec2(rightX, ry), ImVec2(rightX + rightW, ry + headerImgH),
                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 6.0f);
    } else {
        DrawProceduralCover(dl, rightX, ry, rightW, headerImgH, game->appId);
    }
    dl->AddRect(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry + headerImgH),
                IM_COL32(255, 255, 255, 8), 6.0f);
    ry += headerImgH + 12;

    // ── Short description ──
    const char* shortDesc = (sd && !sd->shortDesc.empty()) ? sd->shortDesc.c_str() : game->desc.c_str();
    dl->AddText(mainFont, mainFont->FontSize, ImVec2(rightX + 2, ry),
                IM_COL32(190, 210, 230, 220), shortDesc, nullptr, rightW - 4);
    ImVec2 descSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, rightW - 4, shortDesc);
    ry += descSz.y + 14;

    // ── Reviews section (2 lines: recent + all) ──
    dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), IM_COL32(255, 255, 255, 15));
    ry += 10;

    const char* reviewLabel = (sd && !sd->reviewDesc.empty()) ? sd->reviewDesc.c_str() :
                              game->reviewLabel.c_str();
    int reviewPct = (sd && sd->reviewScore > 0) ? sd->reviewScore : game->reviewPct;
    ImU32 reviewCol = reviewPct >= 80 ? IM_COL32(102, 192, 244, 255) :
                      reviewPct >= 60 ? IM_COL32(180, 200, 120, 255) :
                                        IM_COL32(200, 160, 60, 255);

    // Recent reviews line
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                IM_COL32(120, 140, 165, 180), u8"\u6700\u8FD1\u8BC4\u6D4B\uFF1A"); // 最近评测：
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX + 80, ry), reviewCol, reviewLabel);
    ry += 20;

    // All reviews line
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                IM_COL32(120, 140, 165, 180), u8"\u6240\u6709\u8BC4\u6D4B\uFF1A"); // 所有评测：
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX + 80, ry), reviewCol, reviewLabel);
    if (sd && sd->totalReviews > 0) {
        char revCount[48]; snprintf(revCount, sizeof(revCount), " (%d)", sd->totalReviews);
        float labelW = ImGui::CalcTextSize(reviewLabel).x;
        dl->AddText(smallFont, smallFont->FontSize,
                    ImVec2(rightX + 80 + labelW, ry),
                    IM_COL32(120, 140, 165, 140), revCount);
    }
    ry += 16;

    dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), IM_COL32(255, 255, 255, 15));
    ry += 10;

    // ── Info card panel (dark background) ──
    {
        float infoStartY = ry;
        float infoX = rightX;
        float lineH = 22.0f;

        auto DrawInfoRow = [&](const char* label, const char* value, ImU32 valCol = IM_COL32(200, 215, 230, 220)) {
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(infoX + rPad, ry),
                        IM_COL32(120, 140, 165, 180), label);
            float labelW = ImGui::CalcTextSize(label).x;
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(infoX + rPad + labelW + 6, ry),
                        valCol, value, nullptr, rightW - rPad * 2 - labelW - 6);
            ImVec2 valSz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX,
                            rightW - rPad * 2 - labelW - 6, value);
            ry += std::max(lineH, valSz.y + 4);
        };

        // Release date
        const char* relDate = (sd && !sd->releaseDate.empty()) ? sd->releaseDate.c_str() :
                              game->releaseDate.c_str();
        DrawInfoRow(u8"\u53D1\u884C\u65E5\u671F\uFF1A", relDate); // 发行日期：

        // Developer
        const char* dev = (sd && !sd->developer.empty()) ? sd->developer.c_str() : game->developer.c_str();
        DrawInfoRow(u8"\u5F00\u53D1\u8005\uFF1A", dev, IM_COL32(102, 192, 244, 220)); // 开发者：

        // Publisher
        const char* pub = (sd && !sd->publisher.empty()) ? sd->publisher.c_str() : game->publisher.c_str();
        DrawInfoRow(u8"\u53D1\u884C\u5546\uFF1A", pub, IM_COL32(102, 192, 244, 220)); // 发行商：

        // Draw background behind info section
        float infoH = ry - infoStartY + 6;
        dl->AddRectFilled(ImVec2(infoX, infoStartY - 4), ImVec2(infoX + rightW, infoStartY + infoH),
                          IM_COL32(0, 0, 0, 40), 4.0f);
        ry += 6;
    }

    ry += 6;

    // ── Platform icons ──
    if (sd && (sd->platformWindows || sd->platformMac || sd->platformLinux)) {
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                    IM_COL32(120, 140, 165, 180), u8"\u5E73\u53F0\uFF1A"); // 平台：
        float platX = rightX + ImGui::CalcTextSize(u8"\u5E73\u53F0\uFF1A").x + 8;
        if (sd->platformWindows) {
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(platX, ry),
                        IM_COL32(200, 215, 230, 220), "Windows");
            platX += ImGui::CalcTextSize("Windows").x + 10;
        }
        if (sd->platformMac) {
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(platX, ry),
                        IM_COL32(200, 215, 230, 220), "macOS");
            platX += ImGui::CalcTextSize("macOS").x + 10;
        }
        if (sd->platformLinux) {
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(platX, ry),
                        IM_COL32(200, 215, 230, 220), "Linux");
        }
        ry += 22;
    }

    // ── Tags / Genres ──
    {
        const auto& genres = sd ? sd->genres : std::vector<std::string>{};
        std::vector<std::string> tagList;
        if (!genres.empty()) {
            tagList = genres;
        } else {
            std::string tags = game->tags;
            size_t pos = 0;
            while (pos < tags.size()) {
                size_t comma = tags.find(',', pos);
                if (comma == std::string::npos) comma = tags.size();
                std::string tag = tags.substr(pos, comma - pos);
                while (!tag.empty() && tag[0] == ' ') tag.erase(0, 1);
                pos = comma + 1;
                if (!tag.empty()) tagList.push_back(tag);
            }
        }

        if (!tagList.empty()) {
            dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), IM_COL32(255, 255, 255, 15));
            ry += 10;
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                        IM_COL32(120, 140, 165, 180), u8"\u6807\u7B7E\uFF1A"); // 标签：
            ry += 20;
            float tagX = rightX;
            for (auto& tag : tagList) {
                ImVec2 ts = ImGui::CalcTextSize(tag.c_str());
                float tw = ts.x + 14, th = 22.0f;
                if (tagX + tw > rightX + rightW) { tagX = rightX; ry += th + 4; }
                dl->AddRectFilled(ImVec2(tagX, ry), ImVec2(tagX + tw, ry + th),
                                  IM_COL32(26, 159, 255, 30), th * 0.5f);
                dl->AddRect(ImVec2(tagX, ry), ImVec2(tagX + tw, ry + th),
                            IM_COL32(26, 159, 255, 80), th * 0.5f);
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(tagX + 7, ry + (th - ts.y) * 0.5f),
                            IM_COL32(102, 192, 244, 220), tag.c_str());
                tagX += tw + 5;
            }
            ry += 30;
        }
    }

    // ── Game features / categories in sidebar ──
    if (sd && !sd->categories.empty()) {
        dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), IM_COL32(255, 255, 255, 15));
        ry += 10;
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                    IM_COL32(120, 140, 165, 180), u8"\u6E38\u620F\u7279\u8272"); // 游戏特色
        ry += 20;

        auto catIcon = [](const std::string& cat) -> const char* {
            if (cat.find(u8"\u5355\u4EBA") != std::string::npos) return icon::USER;
            if (cat.find(u8"\u591A\u4EBA") != std::string::npos) return icon::PEOPLE;
            if (cat.find(u8"\u5408\u4F5C") != std::string::npos) return icon::PEOPLE;
            if (cat.find(u8"\u6210\u5C31") != std::string::npos) return icon::TROPHY;
            if (cat.find("Steam") != std::string::npos && cat.find(u8"\u4E91") != std::string::npos) return icon::CLOUD;
            if (cat.find(u8"\u5361\u724C") != std::string::npos) return icon::GIFT;
            if (cat.find(u8"\u624B\u67C4") != std::string::npos) return icon::GAMEPAD;
            if (cat.find(u8"\u8FDC\u7A0B") != std::string::npos) return icon::NETWORK;
            if (cat.find(u8"\u5171\u4EAB") != std::string::npos) return icon::PEOPLE;
            if (cat.find("Workshop") != std::string::npos) return icon::EDIT;
            if (cat.find("Leaderboard") != std::string::npos) return icon::CHART;
            return icon::CHECK;
        };

        int catCount = std::min(10, (int)sd->categories.size());
        for (int i = 0; i < catCount; i++) {
            float fy = ry + i * 24;
            dl->AddRectFilled(ImVec2(rightX, fy), ImVec2(rightX + rightW, fy + 22),
                              IM_COL32(22, 32, 48, (i % 2 == 0) ? (ImU8)180 : (ImU8)120), 3.0f);
            if (g_iconFont)
                DrawIcon(dl, catIcon(sd->categories[i]),
                         ImVec2(rightX + 8, fy + 4), IM_COL32(102, 192, 244, 180));
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX + 28, fy + 4),
                        IM_COL32(200, 215, 230, 220), sd->categories[i].c_str());
        }
        ry += catCount * 24 + 8;
    }

    // ── Links section (website) ──
    if (sd && !sd->website.empty()) {
        dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), IM_COL32(255, 255, 255, 15));
        ry += 10;
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                    IM_COL32(120, 140, 165, 180), u8"\u94FE\u63A5"); // 链接
        ry += 20;
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX + 4, ry),
                    IM_COL32(102, 192, 244, 220), u8"\u8BBF\u95EE\u5B98\u7F51"); // 访问官网
        ry += 20;
        dl->AddText(smallFont, smallFont->FontSize * 0.85f, ImVec2(rightX + 4, ry),
                    IM_COL32(100, 120, 140, 140), sd->website.c_str(), nullptr, rightW - 8);
        ry += 18;
    }

    // ── DLC count ──
    if (sd && sd->dlcCount > 0) {
        dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), IM_COL32(255, 255, 255, 15));
        ry += 10;
        char dlcTxt[64]; snprintf(dlcTxt, sizeof(dlcTxt), u8"\u5305\u542B %d \u4E2A DLC", sd->dlcCount); // 包含 X 个 DLC
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                    IM_COL32(180, 200, 220, 200), dlcTxt);
        ry += 22;
    }

    // ── Language count ──
    if (sd && !sd->languages.empty()) {
        dl->AddLine(ImVec2(rightX, ry), ImVec2(rightX + rightW, ry), IM_COL32(255, 255, 255, 15));
        ry += 10;
        // Count languages by commas
        int langCount = 1;
        for (char c : sd->languages) { if (c == ',') langCount++; }
        char langTxt[64]; snprintf(langTxt, sizeof(langTxt), u8"\u652F\u6301 %d \u79CD\u8BED\u8A00", langCount); // 支持 X 种语言
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(rightX, ry),
                    IM_COL32(180, 200, 220, 200), langTxt);
        ry += 22;
    }

    // ══════ LEFT COLUMN content below screenshots (beside sidebar) ══════
    float ly = cy + mainSSH + 8 + thumbH + 16;

    // ── Purchase Bar (left column, Steam style) ──
    {
        const char* price = (sd && !sd->priceFormatted.empty()) ? sd->priceFormatted.c_str() : game->price.c_str();
        int disc = (sd && sd->discountPercent > 0) ? sd->discountPercent :
                   (!game->discount.empty() ? atoi(game->discount.c_str() + 1) : 0);
        bool isFreeGame = (sd && sd->isFree);

        // Title: "购买 [游戏名]" or "免费游玩 [游戏名]"
        char buyTitle[256];
        if (isFreeGame) {
            snprintf(buyTitle, sizeof(buyTitle), u8"\u514D\u8D39\u6E38\u73A9 %s", gameName); // 免费游玩
        } else {
            snprintf(buyTitle, sizeof(buyTitle), u8"\u8D2D\u4E70 %s", gameName); // 购买
        }
        dl->AddText(mainFont, mainFont->FontSize, ImVec2(leftX, ly),
                    IM_COL32(200, 215, 230, 230), buyTitle, nullptr, leftW);
        ly += mainFont->FontSize + 8;

        float barH = 50.0f;
        dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + barH),
                          IM_COL32(0, 0, 0, 80), 6.0f);
        dl->AddRect(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + barH),
                    IM_COL32(255, 255, 255, 8), 6.0f);

        float px = leftX + 16;
        if (isFreeGame) {
            // Free game - show "免费开玩" label
            if (g_mainFontLarge)
                dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                            ImVec2(px, ly + (barH - g_mainFontLarge->FontSize) * 0.5f),
                            IM_COL32(144, 238, 144, 255), u8"\u514D\u8D39\u5F00\u73A9"); // 免费开玩
        } else if (disc > 0) {
            // Discount badge
            char discStr[8]; snprintf(discStr, sizeof(discStr), "-%d%%", disc);
            ImVec2 discSz = ImGui::CalcTextSize(discStr);
            float badgeW = discSz.x + 14, badgeH = 30.0f;
            float badgeY = ly + (barH - badgeH) * 0.5f;
            dl->AddRectFilled(ImVec2(px, badgeY), ImVec2(px + badgeW, badgeY + badgeH),
                              IM_COL32(76, 107, 34, 255), 4.0f);
            dl->AddText(ImVec2(px + 7, badgeY + (badgeH - discSz.y) * 0.5f),
                        IM_COL32(190, 230, 20, 255), discStr);
            px += badgeW + 12;

            // Original price (strikethrough)
            const char* origPrice = (sd && !sd->originalPrice.empty()) ? sd->originalPrice.c_str() : "";
            if (origPrice[0]) {
                ImVec2 opSz = ImGui::CalcTextSize(origPrice);
                float opY = ly + barH * 0.5f - opSz.y - 1;
                dl->AddText(ImVec2(px, opY), IM_COL32(140, 150, 160, 180), origPrice);
                dl->AddLine(ImVec2(px, opY + opSz.y * 0.5f), ImVec2(px + opSz.x, opY + opSz.y * 0.5f),
                            IM_COL32(140, 150, 160, 140));
            }
            // Final price
            dl->AddText(ImVec2(px, ly + barH * 0.5f + 1),
                        IM_COL32(190, 230, 20, 255), price);
        } else {
            if (g_mainFontLarge)
                dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                            ImVec2(px, ly + (barH - g_mainFontLarge->FontSize) * 0.5f),
                            IM_COL32(255, 255, 255, 245), price);
        }

        // Action button (Install for free games, Add to Cart for paid)
        float buyW = 150, buyBtnH = 36;
        float buyX = leftX + leftW - buyW - 12;
        float buyY = ly + (barH - buyBtnH) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(buyX, buyY));
        ImGui::PushID("buy_left");
        ImGui::InvisibleButton("##buyleft", ImVec2(buyW, buyBtnH));
        bool buyHov = ImGui::IsItemHovered();
        bool buyClk = ImGui::IsItemClicked();
        ImGui::PopID();

        if (isFreeGame) {
            // Green install button for free games
            ImU32 bTop = buyHov ? IM_COL32(80, 180, 100, 255) : IM_COL32(60, 160, 80, 255);
            ImU32 bBot = buyHov ? IM_COL32(50, 150, 70, 255) : IM_COL32(40, 130, 60, 255);
            dl->AddRectFilledMultiColor(ImVec2(buyX, buyY), ImVec2(buyX + buyW, buyY + buyBtnH),
                bTop, bTop, bBot, bBot);
            const char* installTxt = u8"\u5B89\u88C5\u6E38\u620F"; // 安装游戏
            ImVec2 btSz = ImGui::CalcTextSize(installTxt);
            dl->AddText(ImVec2(buyX + (buyW - btSz.x) * 0.5f, buyY + (buyBtnH - btSz.y) * 0.5f),
                        IM_COL32(255, 255, 255, 255), installTxt);

            if (buyClk) {
                // Get game name and size for install dialog
                float gameSize = (sd && sd->diskSpaceBytes > 0) ? (float)sd->diskSpaceBytes : 0.0f;
                Navigate(NavAction::InstallGame, selectedAppId_, gameName, gameSize);
            }
        } else {
            // Normal buy button
            ImU32 bTop = buyHov ? IM_COL32(110, 150, 50, 255) : IM_COL32(90, 130, 40, 255);
            ImU32 bBot = buyHov ? IM_COL32(80, 120, 35, 255) : IM_COL32(65, 100, 28, 255);
            dl->AddRectFilledMultiColor(ImVec2(buyX, buyY), ImVec2(buyX + buyW, buyY + buyBtnH),
                bTop, bTop, bBot, bBot);
            const char* buyTxt = u8"\u52A0\u5165\u8D2D\u7269\u8F66"; // 加入购物车
            ImVec2 btSz = ImGui::CalcTextSize(buyTxt);
            dl->AddText(ImVec2(buyX + (buyW - btSz.x) * 0.5f, buyY + (buyBtnH - btSz.y) * 0.5f),
                        IM_COL32(255, 255, 255, 255), buyTxt);
        }

        ly += barH + 10;
    }

    // ── Wishlist + Follow row (left column) ──
    StyledButton(dl, "wish_left", u8"\u6DFB\u52A0\u5230\u613F\u671B\u5355",
                 leftX, ly, 180, 32, IM_COL32(28, 40, 58, 255),
                 IM_COL32(38, 55, 78, 255), IM_COL32(102, 192, 244, 230), 4.0f, icon::HEART);
    StyledButton(dl, "follow_left", u8"\u5173\u6CE8",
                 leftX + 190, ly, 90, 32, IM_COL32(28, 40, 58, 255),
                 IM_COL32(38, 55, 78, 255), IM_COL32(180, 200, 220, 200), 4.0f, icon::BELL);
    ly += 48;

    // ── DLC section (left column, Steam style) ──
    if (sd && sd->dlcCount > 0) {
        dl->AddLine(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly), IM_COL32(255, 255, 255, 15));
        ly += 12;
        char dlcTitle[128]; snprintf(dlcTitle, sizeof(dlcTitle), u8"\u8BE5\u6E38\u620F\u7684\u5185\u5BB9 (%d)", sd->dlcCount);
        SectionHeader(dl, dlcTitle, leftX, ly, leftW);
        ly += 38;

        // If DLC count < 4, show stacked glass cards (larger size)
        if (sd->dlcCount < 4 && !sd->dlcItems.empty()) {
            // Get owned games to check DLC ownership
            SteamLibraryData libData;
            GetSteamLibraryCopy(libData);
            std::unordered_set<std::string> ownedAppIds;
            for (const auto& g : libData.games) {
                ownedAppIds.insert(g.appId);
            }

            int numCards = std::min((int)sd->dlcItems.size(), sd->dlcCount);

            // Special layout for single DLC - larger horizontal card
            if (numCards == 1) {
                auto& dlc = sd->dlcItems[0];
                bool dlcOwned = ownedAppIds.count(dlc.appId) > 0;

                const float cardW = leftW - 20.0f;  // Almost full width
                const float cardH = 160.0f;
                float cx = leftX + 10.0f;
                float cy = ly;

                // Glass card background with gradient
                ImU32 bgTop = IM_COL32(35, 50, 75, 200);
                ImU32 bgBot = IM_COL32(25, 38, 58, 220);
                dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                                            bgTop, bgTop, bgBot, bgBot);

                // Glass border
                dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                            IM_COL32(100, 140, 180, 100), 6.0f, 0, 1.0f);

                // Top highlight (glass effect)
                dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cardW, cy + 3),
                                            IM_COL32(255, 255, 255, 30), IM_COL32(255, 255, 255, 30),
                                            IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

                // DLC image on the left (larger)
                ImTextureID dlcTex = GetTextureByKey("dlc_" + dlc.appId);
                float imgPad = 12.0f;
                float imgW = 280.0f;  // Wider image
                float imgH = cardH - imgPad * 2;
                if (dlcTex) {
                    dl->AddImageRounded(dlcTex, ImVec2(cx + imgPad, cy + imgPad),
                                        ImVec2(cx + imgPad + imgW, cy + imgPad + imgH),
                                        ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 230), 5.0f);
                } else {
                    dl->AddRectFilled(ImVec2(cx + imgPad, cy + imgPad),
                                      ImVec2(cx + imgPad + imgW, cy + imgPad + imgH),
                                      IM_COL32(40, 55, 75, 200), 5.0f);
                }

                // DLC name on the right (larger font)
                float textX = cx + imgPad + imgW + 20.0f;
                float textW = cardW - imgW - imgPad * 2 - 40.0f;
                ImFont* nameF = g_mainFontLarge ? g_mainFontLarge : mainFont;
                if (dlc.loaded && !dlc.name.empty()) {
                    dl->PushClipRect(ImVec2(textX, cy + imgPad), ImVec2(cx + cardW - imgPad, cy + cardH - imgPad));
                    dl->AddText(nameF, nameF->FontSize, ImVec2(textX, cy + imgPad + 10),
                                IM_COL32(230, 240, 250, 255), dlc.name.c_str(), nullptr, textW);
                    dl->PopClipRect();

                    // Price below name
                    if (!dlc.price.empty()) {
                        float priceY = cy + imgPad + nameF->FontSize + 20;
                        ImFont* priceF = g_mainFont ? g_mainFont : mainFont;
                        if (dlc.discountPercent > 0) {
                            char discBuf[16];
                            snprintf(discBuf, sizeof(discBuf), "-%d%%", dlc.discountPercent);
                            dl->AddText(priceF, priceF->FontSize, ImVec2(textX, priceY),
                                        IM_COL32(166, 207, 106, 255), discBuf);
                            float discW = ImGui::CalcTextSize(discBuf).x + 8;
                            dl->AddText(priceF, priceF->FontSize, ImVec2(textX + discW, priceY),
                                        IM_COL32(166, 207, 106, 255), dlc.price.c_str());
                        } else {
                            dl->AddText(priceF, priceF->FontSize, ImVec2(textX, priceY),
                                        IM_COL32(166, 207, 106, 255), dlc.price.c_str());
                        }
                    }
                }

                // "Owned" ribbon in top-right corner if owned
                if (dlcOwned) {
                    float ribbonW = 90.0f;
                    float ribbonH = 26.0f;
                    float rx = cx + cardW - ribbonW - 8;
                    float ry = cy + 8;

                    // Ribbon background (diagonal style)
                    ImVec2 p1(rx, ry);
                    ImVec2 p2(rx + ribbonW, ry);
                    ImVec2 p3(rx + ribbonW, ry + ribbonH);
                    ImVec2 p4(rx, ry + ribbonH);

                    // Green gradient background
                    dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx + ribbonW, ry + ribbonH),
                                      IM_COL32(76, 175, 80, 220), 4.0f);

                    // Checkmark icon + text
                    const char* ownedTxt = u8"\u2713 \u5DF2\u62E5\u6709";  // ✓ 已拥有
                    ImVec2 txtSz = ImGui::CalcTextSize(ownedTxt);
                    dl->AddText(ImVec2(rx + (ribbonW - txtSz.x) * 0.5f, ry + (ribbonH - txtSz.y) * 0.5f),
                                IM_COL32(255, 255, 255, 255), ownedTxt);
                }

                // Clickable area
                ImGui::SetCursorScreenPos(ImVec2(cx, cy));
                ImGui::PushID("dlc_single_card");
                ImGui::InvisibleButton("##dlcsingle", ImVec2(cardW, cardH));
                if (ImGui::IsItemHovered()) {
                    dl->AddRect(ImVec2(cx - 2, cy - 2), ImVec2(cx + cardW + 2, cy + cardH + 2),
                                IM_COL32(102, 192, 244, 150), 8.0f, 0, 2.0f);
                }
                if (ImGui::IsItemClicked()) {
                    dlcDetailIdx_ = 0;
                    showDlcDetailPopup_ = true;
                    RequestStoreData(dlc.appId);
                }
                ImGui::PopID();

                ly += cardH + 16;
            } else {
                // Multiple DLCs (2-3): stacked cards
                const float cardW = 320.0f;
                const float cardH = 190.0f;
                const float overlapRatio = 0.80f;
                const float cardStep = cardW * overlapRatio;

                float totalW = cardW + (numCards - 1) * cardStep;
                float startX = leftX + (leftW - totalW) * 0.5f;

                int hoveredCard = -1;

                // Draw cards from back to front
                for (int i = numCards - 1; i >= 0; i--) {
                    auto& dlc = sd->dlcItems[i];
                    bool dlcOwned = ownedAppIds.count(dlc.appId) > 0;
                    float cx = startX + i * cardStep;
                    float cy = ly;

                    // Glass card background with gradient
                    ImU32 bgTop = IM_COL32(35, 50, 75, 200);
                    ImU32 bgBot = IM_COL32(25, 38, 58, 220);
                    dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                                                bgTop, bgTop, bgBot, bgBot);

                    // Glass border
                    dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
                                IM_COL32(100, 140, 180, 100), 6.0f, 0, 1.0f);

                    // Top highlight (glass effect)
                    dl->AddRectFilledMultiColor(ImVec2(cx, cy), ImVec2(cx + cardW, cy + 3),
                                                IM_COL32(255, 255, 255, 30), IM_COL32(255, 255, 255, 30),
                                                IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

                    // DLC image
                    ImTextureID dlcTex = GetTextureByKey("dlc_" + dlc.appId);
                    float imgPad = 10.0f;
                    float imgH = 115.0f;
                    if (dlcTex) {
                        dl->AddImageRounded(dlcTex, ImVec2(cx + imgPad, cy + imgPad),
                                            ImVec2(cx + cardW - imgPad, cy + imgPad + imgH),
                                            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 230), 5.0f);
                    } else {
                        dl->AddRectFilled(ImVec2(cx + imgPad, cy + imgPad),
                                          ImVec2(cx + cardW - imgPad, cy + imgPad + imgH),
                                          IM_COL32(40, 55, 75, 200), 5.0f);
                    }

                    // "Owned" corner ribbon if owned
                    if (dlcOwned) {
                        float ribbonSize = 60.0f;
                        float rx = cx + cardW - ribbonSize;
                        float ry = cy;

                        // Diagonal ribbon in top-right corner
                        ImVec2 triPts[3] = {
                            ImVec2(rx, ry),
                            ImVec2(cx + cardW, ry),
                            ImVec2(cx + cardW, ry + ribbonSize)
                        };
                        dl->AddTriangleFilled(triPts[0], triPts[1], triPts[2], IM_COL32(76, 175, 80, 230));

                        // Checkmark icon (rotated text)
                        const char* checkTxt = u8"\u2713";  // ✓
                        ImVec2 checkPos(cx + cardW - 18, cy + 8);
                        dl->AddText(checkPos, IM_COL32(255, 255, 255, 255), checkTxt);
                    }

                    // DLC name
                    dl->PushClipRect(ImVec2(cx + imgPad, cy + imgPad + imgH + 4),
                                     ImVec2(cx + cardW - imgPad, cy + cardH - 4));
                    ImFont* nameF = g_mainFont ? g_mainFont : mainFont;
                    if (dlc.loaded && !dlc.name.empty()) {
                        dl->AddText(nameF, nameF->FontSize, ImVec2(cx + imgPad, cy + imgPad + imgH + 10),
                                    IM_COL32(220, 235, 250, 240), dlc.name.c_str(), nullptr, cardW - imgPad * 2);
                    }
                    dl->PopClipRect();
                }

                // Individual clickable areas
                for (int i = 0; i < numCards; i++) {
                    auto& dlc = sd->dlcItems[i];
                    float cx = startX + i * cardStep;
                    float cy = ly;
                    float clickW = (i < numCards - 1) ? cardStep : cardW;

                    ImGui::SetCursorScreenPos(ImVec2(cx, cy));
                    ImGui::PushID(("dlc_card_" + std::to_string(i)).c_str());
                    ImGui::InvisibleButton("##dlccard", ImVec2(clickW, cardH));

                    if (ImGui::IsItemHovered()) {
                        hoveredCard = i;
                        dl->AddRect(ImVec2(cx - 2, cy - 2), ImVec2(cx + cardW + 2, cy + cardH + 2),
                                    IM_COL32(102, 192, 244, 150), 8.0f, 0, 2.0f);
                    }
                    if (ImGui::IsItemClicked()) {
                        dlcDetailIdx_ = i;
                        showDlcDetailPopup_ = true;
                        RequestStoreData(dlc.appId);
                    }
                    ImGui::PopID();
                }

                ly += cardH + 16;
            }
        } else {
            // Original card style for 8+ DLCs
            float dlcCardH = 80.0f;
            dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + dlcCardH),
                              IM_COL32(18, 26, 38, 240), 6.0f);

            // Use game header as left thumbnail
            float dlcThumbW = 120.0f;
            if (tex) {
                dl->AddImageRounded(tex, ImVec2(leftX, ly), ImVec2(leftX + dlcThumbW, ly + dlcCardH),
                                     ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 200), 6.0f, ImDrawFlags_RoundCornersLeft);
            } else {
                dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + dlcThumbW, ly + dlcCardH),
                                  IM_COL32(30, 45, 65, 255), 6.0f, ImDrawFlags_RoundCornersLeft);
            }

            // DLC info text
            float dlcTx = leftX + dlcThumbW + 16;
            char dlcCount[64]; snprintf(dlcCount, sizeof(dlcCount), u8"%d \u4E2A\u53EF\u4E0B\u8F7D\u5185\u5BB9", sd->dlcCount); // X 个可下载内容
            dl->AddText(mainFont, mainFont->FontSize, ImVec2(dlcTx, ly + 14),
                        IM_COL32(220, 230, 240, 240), dlcCount);
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(dlcTx, ly + 14 + mainFont->FontSize + 4),
                        IM_COL32(140, 160, 180, 180), u8"\u5305\u542B\u989D\u5916\u5185\u5BB9\u3001\u5730\u56FE\u548C\u6269\u5C55\u5305"); // 包含额外内容、地图和扩展包

            // Browse button on the right (clickable)
            float bw = 120, bh = 32;
            float bx = leftX + leftW - bw - 14;
            float by = ly + (dlcCardH - bh) * 0.5f;

            ImGui::SetCursorScreenPos(ImVec2(bx, by));
            ImGui::PushID("dlc_browse");
            ImGui::InvisibleButton("##dlcbr", ImVec2(bw, bh));
            bool dlcHov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                showDlcPopup_ = true;
            }
            ImGui::PopID();

            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
                              dlcHov ? IM_COL32(26, 159, 255, 80) : IM_COL32(26, 159, 255, 40), 4.0f);
            dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
                        dlcHov ? IM_COL32(102, 192, 244, 200) : IM_COL32(102, 192, 244, 120), 4.0f);
            const char* browseTxt = u8"\u6D4F\u89C8\u5168\u90E8"; // 浏览全部
            ImVec2 brSz = ImGui::CalcTextSize(browseTxt);
            dl->AddText(ImVec2(bx + (bw - brSz.x) * 0.5f, by + (bh - brSz.y) * 0.5f),
                        IM_COL32(102, 192, 244, 230), browseTxt);

            dl->AddRect(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + dlcCardH),
                        IM_COL32(255, 255, 255, 10), 6.0f);
            ly += dlcCardH + 16;
        }
    }

    // ── Recent Events & Announcements (left column, Steam card style) ──
    if (sd && !sd->news.empty()) {
        dl->AddLine(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly), IM_COL32(255, 255, 255, 15));
        ly += 12;
        SectionHeader(dl, u8"\u8FD1\u671F\u6D3B\u52A8\u4E0E\u516C\u544A", leftX, ly, leftW);
        ly += 38;

        int newsCount = std::min(3, (int)sd->news.size());

        // First news item: large card with image background
        if (newsCount > 0) {
            const auto& firstNews = sd->news[0];

            // Calculate title height to size card properly
            ImFont* titleFont = g_mainFontLarge ? g_mainFontLarge : mainFont;
            ImVec2 titleSz = titleFont->CalcTextSizeA(titleFont->FontSize, FLT_MAX, leftW - 32, firstNews.title.c_str());
            float topAreaH = 40.0f; // feed label + date area
            float bottomPad = 16.0f;
            float excerptH = firstNews.contents.empty() ? 0 : smallFont->FontSize + 4;
            float textBlockH = titleSz.y + excerptH + bottomPad;
            // Use moderate aspect ratio for better proportions
            float bigCardH = leftW * 0.38f; // Lower height ratio
            if (bigCardH < 200.0f) bigCardH = 200.0f;
            if (bigCardH > 280.0f) bigCardH = 280.0f;

            // Image background (news thumbnail -> game screenshot -> header)
            {
                std::string thumbKey = "news_thumb_" + selectedAppId_ + "_0";
                ImTextureID newsTex = GetTextureByKey(thumbKey);
                if (!newsTex) {
                    // Use game screenshot as fallback (different for each card)
                    std::string ssKey = "ss_" + selectedAppId_ + "_0";
                    newsTex = GetTextureByKey(ssKey);
                }
                ImTextureID bgTex = newsTex ? newsTex : tex;
                if (bgTex) {
                    // Get actual texture dimensions for proper aspect ratio (cover mode)
                    int texW = 0, texH = 0;
                    if (GetTextureSize(thumbKey, texW, texH) && texW > 0 && texH > 0) {
                        float srcAspect = (float)texW / texH;
                        float dstAspect = leftW / bigCardH;
                        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
                        if (srcAspect > dstAspect) {
                            // Source is wider, crop sides
                            float scale = dstAspect / srcAspect;
                            u0 = (1.0f - scale) * 0.5f;
                            u1 = 1.0f - u0;
                        } else {
                            // Source is taller, crop top/bottom
                            float scale = srcAspect / dstAspect;
                            v0 = (1.0f - scale) * 0.5f;
                            v1 = 1.0f - v0;
                        }
                        dl->AddImageRounded(bgTex, ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + bigCardH),
                                             ImVec2(u0, v0), ImVec2(u1, v1), IM_COL32(255, 255, 255, 200), 6.0f);
                    } else {
                        dl->AddImageRounded(bgTex, ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + bigCardH),
                                             ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 200), 6.0f);
                    }
                } else {
                    dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + bigCardH),
                                      IM_COL32(30, 45, 65, 255), 6.0f);
                }
            }
            // Dark gradient overlay from bottom (cover bottom 60%)
            float gradStart = ly + bigCardH * 0.4f;
            float gradH = bigCardH * 0.6f;
            for (int g = 0; g < (int)gradH; g++) {
                float gy = gradStart + g;
                int alpha = (int)(g / gradH * 230.0f);
                if (alpha > 230) alpha = 230;
                dl->AddLine(ImVec2(leftX, gy), ImVec2(leftX + leftW, gy),
                            IM_COL32(12, 18, 28, alpha));
            }
            // Darken top for readability
            dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + 36),
                              IM_COL32(12, 18, 28, 120), 6.0f, ImDrawFlags_RoundCornersTop);

            // Clip all text to card bounds
            dl->PushClipRect(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + bigCardH), true);

            // Feed label + date at top
            float nx = leftX + 16;
            float ny = ly + 10;
            if (!firstNews.feedLabel.empty()) {
                ImVec2 flSz = ImGui::CalcTextSize(firstNews.feedLabel.c_str());
                dl->AddRectFilled(ImVec2(nx - 4, ny - 2), ImVec2(nx + flSz.x + 4, ny + flSz.y + 2),
                                  IM_COL32(102, 192, 244, 60), 3.0f);
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(nx, ny),
                            IM_COL32(102, 192, 244, 240), firstNews.feedLabel.c_str());
            }
            if (firstNews.date > 0) {
                time_t t = (time_t)firstNews.date;
                struct tm tm;
                localtime_s(&tm, &t);
                char dateBuf[32];
                strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tm);
                ImVec2 dateSz = ImGui::CalcTextSize(dateBuf);
                dl->AddText(smallFont, smallFont->FontSize,
                            ImVec2(leftX + leftW - dateSz.x - 16, ly + 10),
                            IM_COL32(200, 215, 230, 180), dateBuf);
            }

            // Title at bottom (clipped to card)
            float titleY = ly + bigCardH - textBlockH;
            dl->AddText(titleFont, titleFont->FontSize, ImVec2(nx, titleY),
                        IM_COL32(255, 255, 255, 250), firstNews.title.c_str(), nullptr, leftW - 32);
            // Excerpt: only show first ~80 chars to avoid overflow
            if (!firstNews.contents.empty()) {
                std::string excerpt = firstNews.contents.substr(0, 80);
                if (firstNews.contents.size() > 80) excerpt += "...";
                float exY = titleY + titleSz.y + 4;
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(nx, exY),
                            IM_COL32(190, 205, 220, 200), excerpt.c_str(), nullptr, leftW - 32);
            }

            dl->PopClipRect();

            // Hover + click
            ImGui::SetCursorScreenPos(ImVec2(leftX, ly));
            ImGui::PushID("news_0");
            ImGui::InvisibleButton("##n0", ImVec2(leftW, bigCardH));
            bool n0Hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                showNewsPopup_ = true;
                newsPopupIdx_ = 0;
            }
            ImGui::PopID();

            dl->AddRect(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + bigCardH),
                        n0Hov ? IM_COL32(102, 192, 244, 100) : IM_COL32(255, 255, 255, 12), 6.0f,
                        0, n0Hov ? 2.0f : 1.0f);
            ly += bigCardH + 8;
        }

        // Remaining news items: compact horizontal cards with better proportions
        if (newsCount > 1) {
            float gap = 12.0f;
            int remaining = newsCount - 1;
            float cardW = (leftW - gap * (remaining - 1)) / remaining;
            // Use 16:9 aspect ratio for thumbnail, capped for reasonable size
            float imgH = cardW * 0.5f; // Better thumbnail ratio
            if (imgH > 140.0f) imgH = 140.0f;
            if (imgH < 80.0f) imgH = 80.0f;
            float smallCardH = imgH + 70.0f; // More space for title

            for (int ni = 1; ni < newsCount; ni++) {
                const auto& news = sd->news[ni];
                float cx2 = leftX + (ni - 1) * (cardW + gap);

                // Card with image top (news thumbnail -> screenshot -> header)
                {
                    char thumbKeyBuf[64];
                    snprintf(thumbKeyBuf, sizeof(thumbKeyBuf), "news_thumb_%s_%d", selectedAppId_.c_str(), ni);
                    ImTextureID newsTex = GetTextureByKey(thumbKeyBuf);
                    if (!newsTex) {
                        // Use different screenshot for each card
                        char ssKeyBuf[64];
                        snprintf(ssKeyBuf, sizeof(ssKeyBuf), "ss_%s_%d", selectedAppId_.c_str(), ni);
                        newsTex = GetTextureByKey(ssKeyBuf);
                    }
                    ImTextureID cardTex = newsTex ? newsTex : tex;
                    if (cardTex) {
                        // Get actual texture dimensions for proper aspect ratio
                        int texW = 0, texH = 0;
                        char texKey[64];
                        snprintf(texKey, sizeof(texKey), "news_thumb_%s_%d", selectedAppId_.c_str(), ni);
                        if (GetTextureSize(texKey, texW, texH) && texW > 0 && texH > 0) {
                            // Calculate UV to crop center of image (cover mode)
                            float srcAspect = (float)texW / texH;
                            float dstAspect = cardW / imgH;
                            float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
                            if (srcAspect > dstAspect) {
                                // Source is wider, crop sides
                                float scale = dstAspect / srcAspect;
                                u0 = (1.0f - scale) * 0.5f;
                                u1 = 1.0f - u0;
                            } else {
                                // Source is taller, crop top/bottom
                                float scale = srcAspect / dstAspect;
                                v0 = (1.0f - scale) * 0.5f;
                                v1 = 1.0f - v0;
                            }
                            dl->AddImageRounded(cardTex, ImVec2(cx2, ly), ImVec2(cx2 + cardW, ly + imgH),
                                                 ImVec2(u0, v0), ImVec2(u1, v1),
                                                 IM_COL32(255, 255, 255, 200), 6.0f, ImDrawFlags_RoundCornersTop);
                        } else {
                            dl->AddImageRounded(cardTex, ImVec2(cx2, ly), ImVec2(cx2 + cardW, ly + imgH),
                                                 ImVec2(0, 0), ImVec2(1, 1),
                                                 IM_COL32(255, 255, 255, 200), 6.0f, ImDrawFlags_RoundCornersTop);
                        }
                    } else {
                        dl->AddRectFilled(ImVec2(cx2, ly), ImVec2(cx2 + cardW, ly + imgH),
                                          IM_COL32(30, 45, 65, 255), 6.0f, ImDrawFlags_RoundCornersTop);
                    }
                }

                // Dark body
                float bodyY = ly + imgH;
                dl->AddRectFilled(ImVec2(cx2, bodyY), ImVec2(cx2 + cardW, ly + smallCardH),
                                  IM_COL32(22, 32, 48, 240), 0, ImDrawFlags_RoundCornersBottom);

                // Clip text to card bounds
                dl->PushClipRect(ImVec2(cx2, bodyY), ImVec2(cx2 + cardW, ly + smallCardH), true);

                // Title (wrap within card width, truncated)
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx2 + 10, bodyY + 8),
                            IM_COL32(220, 230, 240, 240), news.title.c_str(), nullptr, cardW - 20);

                // Date at bottom
                if (news.date > 0) {
                    time_t t = (time_t)news.date;
                    struct tm tm;
                    localtime_s(&tm, &t);
                    char dateBuf[32];
                    strftime(dateBuf, sizeof(dateBuf), "%m-%d", &tm);
                    dl->AddText(smallFont, smallFont->FontSize,
                                ImVec2(cx2 + 10, ly + smallCardH - smallFont->FontSize - 8),
                                IM_COL32(120, 140, 165, 160), dateBuf);
                }

                dl->PopClipRect();

                // Hover + click
                char nid[16]; snprintf(nid, sizeof(nid), "news_%d", ni);
                ImGui::SetCursorScreenPos(ImVec2(cx2, ly));
                ImGui::PushID(nid);
                ImGui::InvisibleButton("##ni", ImVec2(cardW, smallCardH));
                bool niHov = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) {
                    showNewsPopup_ = true;
                    newsPopupIdx_ = ni;
                }
                ImGui::PopID();

                dl->AddRect(ImVec2(cx2, ly), ImVec2(cx2 + cardW, ly + smallCardH),
                            niHov ? IM_COL32(102, 192, 244, 100) : IM_COL32(255, 255, 255, 10), 6.0f,
                            0, niHov ? 2.0f : 1.0f);
            }
            ly += smallCardH + 8;
        }
        ly += 8;
    }

    // ── About This Game (left column) ──
    dl->AddLine(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly), IM_COL32(255, 255, 255, 15));
    ly += 12;
    SectionHeader(dl, u8"\u5173\u4E8E\u6B64\u6E38\u620F", leftX, ly, leftW); // 关于此游戏
    ly += 38;

    // Rich "About this game" with inline images, headings, text (Steam style)
    {
        bool hasRich = sd && !sd->aboutRich.empty();
        float contentW = leftW - 28;
        float textX = leftX + 14;

        if (hasRich) {
            int aboutImgIdx = 0;
            for (size_t si = 0; si < sd->aboutRich.size(); si++) {
                auto& seg = sd->aboutRich[si];

                if (seg.type == SteamStoreData::RichSegment::IMAGE) {
                    char imgKeyBuf[128];
                    snprintf(imgKeyBuf, sizeof(imgKeyBuf), "about_%s_%d",
                             selectedAppId_.c_str(), aboutImgIdx++);
                    ImTextureID imgTex = GetTextureByKey(imgKeyBuf);
                    if (imgTex) {
                        ly += 8;
                        // Get actual image dimensions for proper aspect ratio
                        int imgW = 0, imgH = 0;
                        float aspectH = contentW * 0.5625f; // default 16:9
                        if (GetTextureSize(imgKeyBuf, imgW, imgH) && imgW > 0 && imgH > 0) {
                            aspectH = contentW * ((float)imgH / (float)imgW);
                            // Clamp height to reasonable bounds
                            if (aspectH > contentW * 1.5f) aspectH = contentW * 1.5f;
                            if (aspectH < 60.0f) aspectH = 60.0f;
                        }
                        dl->AddImageRounded(imgTex,
                            ImVec2(textX, ly), ImVec2(textX + contentW, ly + aspectH),
                            ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), 4.0f);
                        ly += aspectH + 8;
                    } else {
                        // Still loading or failed - show placeholder with loading animation
                        ly += 4;
                        float placeholderH = contentW * 0.35f;
                        dl->AddRectFilled(ImVec2(textX, ly), ImVec2(textX + contentW, ly + placeholderH),
                                          IM_COL32(30, 40, 55, 200), 4.0f);
                        float t = fmodf((float)ImGui::GetTime(), 1.0f);
                        for (int di = 0; di < 3; di++) {
                            float alpha = (t > di * 0.3f) ? 200.0f : 60.0f;
                            dl->AddCircleFilled(
                                ImVec2(textX + contentW * 0.5f - 16 + di * 16, ly + placeholderH * 0.5f),
                                3.0f, IM_COL32(102, 192, 244, (int)alpha));
                        }
                        ly += placeholderH + 4;
                    }
                } else if (seg.type == SteamStoreData::RichSegment::HEADING) {
                    ly += 10;
                    ImFont* hFont = (seg.headingLevel <= 2 && g_mainFontLarge) ? g_mainFontLarge : mainFont;
                    ImVec2 hSz = hFont->CalcTextSizeA(hFont->FontSize, FLT_MAX, contentW, seg.text.c_str());
                    dl->AddText(hFont, hFont->FontSize, ImVec2(textX, ly),
                                IM_COL32(240, 245, 250, 255), seg.text.c_str(), nullptr, contentW);
                    ly += hSz.y + 4;
                    if (seg.headingLevel <= 2) {
                        dl->AddLine(ImVec2(textX, ly), ImVec2(textX + contentW, ly),
                                    IM_COL32(102, 192, 244, 40));
                        ly += 6;
                    }
                } else if (seg.type == SteamStoreData::RichSegment::BULLET) {
                    ly += 3;
                    dl->AddCircleFilled(ImVec2(textX + 4, ly + 7), 3.0f, IM_COL32(102, 192, 244, 180));
                    ImVec2 bSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, contentW - 16,
                                                          seg.text.c_str());
                    dl->AddText(mainFont, mainFont->FontSize, ImVec2(textX + 16, ly),
                                IM_COL32(200, 215, 230, 220), seg.text.c_str(), nullptr, contentW - 16);
                    ly += bSz.y + 3;
                } else if (seg.type == SteamStoreData::RichSegment::VIDEO) {
                    // Video in about section - show play button, click to play
                    ly += 8;
                    float videoW = contentW;
                    float videoH = videoW * 0.5625f; // 16:9

                    // Check if this video is playing (reuse playingVideo_ state)
                    bool isThisPlaying = playingVideo_ && playingMovieIdx_ == -2 - (int)si;

                    if (isThisPlaying && IsVideoActive()) {
                        // Show video frame
                        int frameW = 0, frameH = 0;
                        ImTextureID frameTex = GetVideoFrame(&frameW, &frameH);
                        if (frameTex && frameW > 0 && frameH > 0) {
                            videoH = videoW * ((float)frameH / (float)frameW);
                            dl->AddImage(frameTex, ImVec2(textX, ly), ImVec2(textX + videoW, ly + videoH));
                        } else {
                            dl->AddRectFilled(ImVec2(textX, ly), ImVec2(textX + videoW, ly + videoH),
                                              IM_COL32(15, 20, 30, 240), 6.0f);
                            const char* loadLabel = u8"\u52A0\u8F7D\u89C6\u9891\u4E2D...";
                            ImVec2 loadSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, 0, loadLabel);
                            dl->AddText(mainFont, mainFont->FontSize,
                                        ImVec2(textX + videoW * 0.5f - loadSz.x * 0.5f, ly + videoH * 0.5f - loadSz.y * 0.5f),
                                        IM_COL32(180, 200, 220, 200), loadLabel);
                        }

                        // Stop button
                        float btnR = 20.0f;
                        float btnX = textX + videoW - btnR - 10;
                        float btnY = ly + btnR + 10;
                        dl->AddCircleFilled(ImVec2(btnX, btnY), btnR, IM_COL32(200, 60, 60, 200));
                        float sqS = 6.0f;
                        dl->AddRectFilled(ImVec2(btnX - sqS, btnY - sqS), ImVec2(btnX + sqS, btnY + sqS),
                                          IM_COL32(255, 255, 255, 255));

                        // Click to stop
                        ImGui::SetCursorScreenPos(ImVec2(textX, ly));
                        char btnId[64];
                        snprintf(btnId, sizeof(btnId), "##aboutvid_%d", (int)si);
                        if (ImGui::InvisibleButton(btnId, ImVec2(videoW, videoH))) {
                            StopVideo();
                            playingVideo_ = false;
                            playingMovieIdx_ = -1;
                        }
                    } else {
                        // Show play button
                        dl->AddRectFilled(ImVec2(textX, ly), ImVec2(textX + videoW, ly + videoH),
                                          IM_COL32(15, 20, 30, 240), 6.0f);
                        dl->AddRect(ImVec2(textX, ly), ImVec2(textX + videoW, ly + videoH),
                                    IM_COL32(60, 80, 110, 120), 6.0f);

                        float cx = textX + videoW * 0.5f;
                        float cy = ly + videoH * 0.5f;
                        float btnR = 24.0f;

                        ImGui::SetCursorScreenPos(ImVec2(textX, ly));
                        char btnId[64];
                        snprintf(btnId, sizeof(btnId), "##aboutvid_%d", (int)si);
                        bool clicked = ImGui::InvisibleButton(btnId, ImVec2(videoW, videoH));
                        bool hovered = ImGui::IsItemHovered();

                        ImU32 btnCol = hovered ? IM_COL32(130, 210, 255, 220) : IM_COL32(102, 192, 244, 200);
                        dl->AddCircleFilled(ImVec2(cx, cy), btnR, btnCol);
                        float triS = 10.0f;
                        ImVec2 tri[3] = {
                            ImVec2(cx - triS * 0.4f, cy - triS),
                            ImVec2(cx - triS * 0.4f, cy + triS),
                            ImVec2(cx + triS * 0.9f, cy)
                        };
                        dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 255, 255, 255));

                        const char* playLabel = u8"\u70B9\u51FB\u64AD\u653E";
                        ImVec2 playSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, 0, playLabel);
                        dl->AddText(mainFont, mainFont->FontSize,
                                    ImVec2(cx - playSz.x * 0.5f, cy + btnR + 8),
                                    IM_COL32(180, 200, 220, 200), playLabel);

                        if (clicked) {
                            if (playingVideo_) StopVideo();
                            std::string videoUrl = seg.text;
                            if (!seg.videoUrl2.empty()) videoUrl = seg.videoUrl2;
                            if (!videoUrl.empty() && PlayVideo(videoUrl)) {
                                playingVideo_ = true;
                                playingMovieIdx_ = -2 - (int)si; // negative index for about videos
                            }
                        }
                    }
                    ly += videoH + 8;
                } else {
                    // TEXT
                    ImVec2 tSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, contentW,
                                                          seg.text.c_str());
                    dl->AddText(mainFont, mainFont->FontSize, ImVec2(textX, ly),
                                IM_COL32(200, 215, 230, 220), seg.text.c_str(), nullptr, contentW);
                    ly += tSz.y + 5;
                }
            }
        } else {
            // Fallback: plain text
            const char* aboutText = (sd && !sd->aboutGame.empty()) ? sd->aboutGame.c_str() : game->desc.c_str();
            ImVec2 aboutSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, contentW, aboutText);
            float aboutH = aboutSz.y + 24;

            dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + aboutH),
                              IM_COL32(18, 26, 38, 200), 6.0f);
            dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + 3, ly + aboutH),
                              IM_COL32(26, 159, 255, 120), 2.0f);
            dl->AddText(mainFont, mainFont->FontSize, ImVec2(textX, ly + 12),
                        IM_COL32(200, 215, 230, 230), aboutText, nullptr, contentW);
            ly += aboutH;
        }
        ly += 20;
    }

    // ── User Review Summary (left column, Steam style) ──
    if (sd && sd->totalReviews > 0) {
        dl->AddLine(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly), IM_COL32(255, 255, 255, 15));
        ly += 12;
        SectionHeader(dl, u8"\u7528\u6237\u8BC4\u6D4B", leftX, ly, leftW);
        ly += 38;

        int revPct = sd->reviewScore;
        ImU32 revCol = revPct >= 80 ? IM_COL32(102, 192, 244, 255) :
                       revPct >= 60 ? IM_COL32(180, 200, 120, 255) :
                                      IM_COL32(200, 160, 60, 255);
        ImU32 revColDark = revPct >= 80 ? IM_COL32(102, 192, 244, 40) :
                           revPct >= 60 ? IM_COL32(180, 200, 120, 40) :
                                          IM_COL32(200, 160, 60, 40);

        float revPanelH = 120.0f;
        // Panel background with colored top accent
        dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + revPanelH),
                          IM_COL32(18, 26, 38, 220), 6.0f);
        dl->AddRectFilled(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + 4),
                          revCol, 6.0f, ImDrawFlags_RoundCornersTop);

        // Left side: big review icon (thumbs up/down circle)
        float circleR = 30.0f;
        float circleX = leftX + 44;
        float circleY = ly + revPanelH * 0.5f;
        dl->AddCircleFilled(ImVec2(circleX, circleY), circleR, revColDark, 32);
        dl->AddCircle(ImVec2(circleX, circleY), circleR, revCol, 32, 2.0f);

        // Thumbs up/down icon inside circle
        if (revPct >= 60) {
            // Thumbs up — draw a simple arrow-up shape
            ImVec2 pts[3] = {
                ImVec2(circleX - 10, circleY + 6),
                ImVec2(circleX + 10, circleY + 6),
                ImVec2(circleX, circleY - 12)
            };
            dl->AddTriangleFilled(pts[0], pts[1], pts[2], revCol);
            dl->AddRectFilled(ImVec2(circleX - 5, circleY + 6), ImVec2(circleX + 5, circleY + 14), revCol, 2.0f);
        } else {
            // Thumbs down
            ImVec2 pts[3] = {
                ImVec2(circleX - 10, circleY - 6),
                ImVec2(circleX + 10, circleY - 6),
                ImVec2(circleX, circleY + 12)
            };
            dl->AddTriangleFilled(pts[0], pts[1], pts[2], revCol);
            dl->AddRectFilled(ImVec2(circleX - 5, circleY - 14), ImVec2(circleX + 5, circleY - 6), revCol, 2.0f);
        }

        // Right side: text info
        float textX = leftX + 90;
        float textY = ly + 16;

        // Review label large (e.g. 好评如潮)
        if (g_mainFontLarge)
            dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize, ImVec2(textX, textY),
                        revCol, sd->reviewDesc.c_str());
        textY += (g_mainFontLarge ? g_mainFontLarge->FontSize : 18) + 8;

        // Summary line
        char revSummary[128];
        snprintf(revSummary, sizeof(revSummary), u8"%d%% \u7684 %d \u7BC7\u7528\u6237\u8BC4\u6D4B\u4E3A\u597D\u8BC4",
                 sd->reviewScore, sd->totalReviews);
        dl->AddText(mainFont, mainFont->FontSize, ImVec2(textX, textY),
                    IM_COL32(200, 215, 230, 220), revSummary);
        textY += mainFont->FontSize + 10;

        // Full-width progress bar
        float barX = textX;
        float barW = leftX + leftW - textX - 16;
        float barH = 10.0f;
        dl->AddRectFilled(ImVec2(barX, textY), ImVec2(barX + barW, textY + barH),
                          IM_COL32(200, 80, 60, 80), 5.0f);
        float fillW = barW * (sd->reviewScore / 100.0f);
        if (fillW > 0)
            dl->AddRectFilled(ImVec2(barX, textY), ImVec2(barX + fillW, textY + barH),
                              revCol, 5.0f);
        textY += barH + 8;

        // Positive / Negative counts below bar
        if (sd->totalPositive > 0) {
            char posTxt[64]; snprintf(posTxt, sizeof(posTxt), u8"\u25B2 %d \u597D\u8BC4", sd->totalPositive);
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(barX, textY),
                        IM_COL32(102, 192, 244, 220), posTxt);

            int neg = sd->totalReviews - sd->totalPositive;
            if (neg > 0) {
                char negTxt[64]; snprintf(negTxt, sizeof(negTxt), u8"\u25BC %d \u5DEE\u8BC4", neg);
                float posW = ImGui::CalcTextSize(posTxt).x;
                dl->AddText(smallFont, smallFont->FontSize, ImVec2(barX + posW + 24, textY),
                            IM_COL32(200, 100, 70, 220), negTxt);
            }
        }

        dl->AddRect(ImVec2(leftX, ly), ImVec2(leftX + leftW, ly + revPanelH),
                    IM_COL32(255, 255, 255, 8), 6.0f);
        ly += revPanelH + 16;
    }

    // ── Determine where full-width sections begin ──
    float belowY = std::max(ly, ry + 10);

    // ══════ BELOW: Full-width sections (after both columns) ══════
    cy = belowY;

    dl->AddLine(ImVec2(cx, cy), ImVec2(cx + cw, cy), IM_COL32(255, 255, 255, 15));
    cy += 20;

    // ── System Requirements (real data) ──
    SectionHeader(dl, u8"\u7CFB\u7EDF\u9700\u6C42", cx, cy, cw);
    cy += 38;

    bool hasRealReqs = sd && (!sd->reqMinimum.empty() || !sd->reqRecommended.empty());
    float halfW = cw * 0.5f;

    if (hasRealReqs) {
        // Real system requirements from Steam
        float reqPadding = 14.0f;

        // Calculate heights
        ImVec2 minSz = {0, 60};
        ImVec2 recSz = {0, 60};
        if (!sd->reqMinimum.empty())
            minSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, halfW - 30, sd->reqMinimum.c_str());
        if (!sd->reqRecommended.empty())
            recSz = mainFont->CalcTextSizeA(mainFont->FontSize, FLT_MAX, halfW - 30, sd->reqRecommended.c_str());
        float specPanelH = std::max(minSz.y, recSz.y) + 50;

        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + specPanelH),
                          IM_COL32(18, 26, 38, 255), 6.0f);
        dl->AddRect(ImVec2(cx, cy), ImVec2(cx + cw, cy + specPanelH),
                    IM_COL32(255, 255, 255, 10), 6.0f);

        // Headers
        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + 28), IM_COL32(26, 159, 255, 20), 6.0f);
        dl->AddText(ImVec2(cx + 16, cy + 6), IM_COL32(26, 159, 255, 220),
                    u8"\u6700\u4F4E\u914D\u7F6E");
        dl->AddText(ImVec2(cx + halfW + 16, cy + 6), IM_COL32(26, 159, 255, 220),
                    u8"\u63A8\u8350\u914D\u7F6E");
        dl->AddLine(ImVec2(cx + halfW, cy + 28), ImVec2(cx + halfW, cy + specPanelH),
                    IM_COL32(255, 255, 255, 15));

        if (!sd->reqMinimum.empty())
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + 16, cy + 36),
                        IM_COL32(190, 205, 220, 200), sd->reqMinimum.c_str(), nullptr, halfW - 30);
        if (!sd->reqRecommended.empty())
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + halfW + 16, cy + 36),
                        IM_COL32(190, 205, 220, 200), sd->reqRecommended.c_str(), nullptr, halfW - 30);
        cy += specPanelH + 16;
    } else {
        // Fallback static requirements
        float specPanelH = 160.0f;
        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + specPanelH),
                          IM_COL32(18, 26, 38, 255), 6.0f);
        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + 28), IM_COL32(26, 159, 255, 20), 6.0f);
        dl->AddText(ImVec2(cx + 16, cy + 6), IM_COL32(26, 159, 255, 220), u8"\u6700\u4F4E\u914D\u7F6E");
        dl->AddText(ImVec2(cx + halfW + 16, cy + 6), IM_COL32(26, 159, 255, 220), u8"\u63A8\u8350\u914D\u7F6E");
        dl->AddLine(ImVec2(cx + halfW, cy + 28), ImVec2(cx + halfW, cy + specPanelH), IM_COL32(255, 255, 255, 15));

        struct SpecLine { const char* label; const char* minV; const char* recV; };
        SpecLine specs[] = {
            { u8"\u64CD\u4F5C\u7CFB\u7EDF", "Windows 10 64-bit", "Windows 11 64-bit" },
            { u8"\u5904\u7406\u5668", "Intel i5-8400", "Intel i7-12700" },
            { u8"\u5185\u5B58", "16 GB RAM", "32 GB RAM" },
            { u8"\u663E\u5361", "GTX 1060 6GB", "RTX 3070" },
            { u8"\u5B58\u50A8\u7A7A\u95F4", "80 GB", "80 GB SSD" },
        };
        float sy = cy + 36;
        for (auto& sp : specs) {
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + 16, sy), IM_COL32(120, 140, 165, 180), sp.label);
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + 16, sy + 12), IM_COL32(200, 215, 230, 210), sp.minV);
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + halfW + 16, sy), IM_COL32(120, 140, 165, 180), sp.label);
            dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx + halfW + 16, sy + 12), IM_COL32(200, 215, 230, 210), sp.recV);
            sy += 24;
        }
        cy += specPanelH + 16;
    }

    // ── Languages (if available) ──
    if (sd && !sd->languages.empty()) {
        SectionHeader(dl, u8"\u652F\u6301\u8BED\u8A00", cx, cy, cw); // 支持语言
        cy += 34;
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(cx, cy),
                    IM_COL32(180, 200, 220, 200), sd->languages.c_str(), nullptr, cw);
        ImVec2 langSz = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, cw, sd->languages.c_str());
        cy += langSz.y + 20;
    }

    // Loading indicator
    if (loading && !sd) {
        dl->AddText(ImVec2(cx + cw * 0.5f - 50, cy),
                    IM_COL32(102, 192, 244, 180), u8"\u6B63\u5728\u52A0\u8F7D\u6E38\u620F\u6570\u636E..."); // 正在加载游戏数据...
        cy += 30;
    }

    // Set scroll content size based on actual content height (relative, not screen coords)
    float contentHeight = cy - y + 60;
    ImGui::SetCursorPosY(contentHeight);

    // ══════ In-app Popup Windows ══════

    // ── News Detail Popup ──
    if (showNewsPopup_ && sd && newsPopupIdx_ >= 0 && newsPopupIdx_ < (int)sd->news.size()) {
        const auto& newsItem = sd->news[newsPopupIdx_];

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(950, 720), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(600, 500), ImVec2(1200, 950));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 28, 42, 245));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, IM_COL32(16, 22, 34, 255));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, IM_COL32(26, 40, 60, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 80, 110, 120));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        bool open = showNewsPopup_;
        if (ImGui::Begin(u8"\u6D3B\u52A8\u8BE6\u60C5##news_popup", &open,
                         ImGuiWindowFlags_NoCollapse)) {
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            ImVec2 wPos = ImGui::GetWindowPos();
            ImVec2 wSize = ImGui::GetWindowSize();

            // ═══ Translation button in top-right corner ═══
            {
                float btnSize = 28.0f;
                float btnX = wPos.x + wSize.x - btnSize - 50;  // Leave space for close button
                float btnY = wPos.y + 32;  // Move down below title bar
                ImVec2 btnPos(btnX, btnY);

                // Generate unique translation request ID
                char transReqId[64];
                snprintf(transReqId, sizeof(transReqId), "news_%s_%d", selectedAppId_.c_str(), newsPopupIdx_);

                bool isLoading = IsTranslationLoading(transReqId);
                const TranslationResult* transResult = GetTranslationResult(transReqId);
                bool isTranslated = transResult && transResult->loaded;

                // Button background
                ImU32 btnBg = IM_COL32(40, 55, 75, 200);
                ImU32 btnHover = IM_COL32(60, 80, 110, 220);
                ImU32 iconCol = isTranslated ? IM_COL32(102, 192, 244, 255) : IM_COL32(180, 200, 220, 220);

                ImVec2 mousePos = ImGui::GetMousePos();
                bool hovered = mousePos.x >= btnX && mousePos.x <= btnX + btnSize &&
                               mousePos.y >= btnY && mousePos.y <= btnY + btnSize;

                pdl->AddRectFilled(btnPos, ImVec2(btnX + btnSize, btnY + btnSize),
                                   hovered ? btnHover : btnBg, 4.0f);

                // Draw translate icon
                if (isLoading) {
                    // Loading spinner
                    float cx = btnX + btnSize * 0.5f;
                    float cy = btnY + btnSize * 0.5f;
                    float angle = (float)ImGui::GetTime() * 3.0f;
                    for (int i = 0; i < 8; i++) {
                        float a = angle + i * IM_PI * 0.25f;
                        float alpha = 255 - i * 25;
                        pdl->AddCircleFilled(
                            ImVec2(cx + cosf(a) * 8, cy + sinf(a) * 8),
                            2.0f, IM_COL32(102, 192, 244, (int)alpha));
                    }
                } else {
                    icons::DrawTranslate(pdl, ImVec2(btnX + 2, btnY + 2), btnSize - 4, iconCol);
                }

                // Handle click
                if (hovered && ImGui::IsMouseClicked(0) && !isLoading && !isTranslated) {
                    // Collect texts to translate
                    std::vector<std::string> textsToTranslate;
                    textsToTranslate.push_back(newsItem.title);
                    for (const auto& seg : newsItem.richContent) {
                        if (seg.type == SteamStoreData::RichSegment::TEXT ||
                            seg.type == SteamStoreData::RichSegment::HEADING) {
                            if (!seg.text.empty()) {
                                textsToTranslate.push_back(seg.text);
                            }
                        }
                    }
                    RequestTranslation(textsToTranslate, transReqId);
                }

                // Tooltip
                if (hovered) {
                    ImGui::BeginTooltip();
                    if (isLoading) {
                        ImGui::TextUnformatted(u8"\u7FFB\u8BD1\u4E2D...");  // 翻译中...
                    } else if (isTranslated) {
                        ImGui::TextUnformatted(u8"\u5DF2\u7FFB\u8BD1");  // 已翻译
                    } else {
                        ImGui::TextUnformatted(u8"\u7FFB\u8BD1\u4E3A\u4E2D\u6587");  // 翻译为中文
                    }
                    ImGui::EndTooltip();
                }
            }

            // Feed label badge
            if (!newsItem.feedLabel.empty()) {
                ImVec2 flSz = ImGui::CalcTextSize(newsItem.feedLabel.c_str());
                ImVec2 cp = ImGui::GetCursorScreenPos();
                pdl->AddRectFilled(ImVec2(cp.x - 2, cp.y - 1), ImVec2(cp.x + flSz.x + 6, cp.y + flSz.y + 2),
                                   IM_COL32(102, 192, 244, 50), 3.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(102, 192, 244, 230));
                ImGui::TextUnformatted(newsItem.feedLabel.c_str());
                ImGui::PopStyleColor();
            }

            // Date
            if (newsItem.date > 0) {
                time_t t = (time_t)newsItem.date;
                struct tm tm;
                localtime_s(&tm, &t);
                char dateBuf[64];
                strftime(dateBuf, sizeof(dateBuf), u8"%Y \u5E74 %m \u6708 %d \u65E5", &tm);
                if (!newsItem.feedLabel.empty()) { ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine(); }
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(140, 160, 180, 180));
                ImGui::TextUnformatted(dateBuf);
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            // Get translation result for display
            char transReqId[64];
            snprintf(transReqId, sizeof(transReqId), "news_%s_%d", selectedAppId_.c_str(), newsPopupIdx_);
            const TranslationResult* transResult = GetTranslationResult(transReqId);
            size_t transIdx = 0;  // Index into translated texts (0 = title, then content segments)

            // Title (large) - use translated if available
            if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 245, 250, 255));
            if (transResult && transResult->loaded && transIdx < transResult->translatedTexts.size()) {
                ImGui::TextWrapped("%s", transResult->translatedTexts[transIdx].c_str());
            } else {
                ImGui::TextWrapped("%s", newsItem.title.c_str());
            }
            transIdx++;
            ImGui::PopStyleColor();
            if (g_mainFontLarge) ImGui::PopFont();

            ImGui::Spacing();

            // Divider
            ImVec2 divP = ImGui::GetCursorScreenPos();
            pdl->AddLine(ImVec2(divP.x, divP.y), ImVec2(divP.x + wSize.x - 48, divP.y),
                         IM_COL32(102, 192, 244, 60));
            ImGui::Dummy(ImVec2(0, 8));

            // Rich content rendering
            if (!newsItem.richContent.empty()) {
                float contentW = ImGui::GetContentRegionAvail().x;
                int imgIdx = 0;
                for (size_t si = 0; si < newsItem.richContent.size(); si++) {
                    auto& seg = newsItem.richContent[si];
                    if (seg.type == SteamStoreData::RichSegment::IMAGE) {
                        // Use pre-requested texture key
                        char imgKeyBuf[128];
                        snprintf(imgKeyBuf, sizeof(imgKeyBuf), "news_inline_%s_%d_%d",
                                 selectedAppId_.c_str(), newsPopupIdx_, imgIdx++);
                        ImTextureID imgTex = GetTextureByKey(imgKeyBuf);
                        if (imgTex) {
                            float imgW = contentW;
                            float imgH = imgW * 0.5625f; // 16:9 default
                            ImGui::Dummy(ImVec2(0, 4));
                            ImGui::Image(imgTex, ImVec2(imgW, imgH));
                            ImGui::Dummy(ImVec2(0, 4));
                        } else {
                            // Show placeholder while loading
                            ImGui::Dummy(ImVec2(0, 2));
                            float placeholderH = contentW * 0.3f;
                            ImVec2 cp = ImGui::GetCursorScreenPos();
                            ImDrawList* idl = ImGui::GetWindowDrawList();
                            idl->AddRectFilled(cp, ImVec2(cp.x + contentW, cp.y + placeholderH),
                                               IM_COL32(30, 40, 55, 200), 4.0f);
                            idl->AddText(ImVec2(cp.x + contentW * 0.5f - 40, cp.y + placeholderH * 0.5f - 8),
                                         IM_COL32(100, 130, 160, 150), u8"\u52A0\u8F7D\u56FE\u7247\u4E2D...");
                            ImGui::Dummy(ImVec2(contentW, placeholderH));
                            ImGui::Dummy(ImVec2(0, 2));
                        }
                    } else if (seg.type == SteamStoreData::RichSegment::VIDEO) {
                        // In-app video player
                        ImGui::Dummy(ImVec2(0, 4));
                        float videoW = contentW;
                        float videoH = videoW * 0.5625f; // 16:9
                        ImVec2 vp = ImGui::GetCursorScreenPos();
                        ImDrawList* vdl = ImGui::GetWindowDrawList();

                        // Check if this video is currently playing
                        bool isThisPlaying = (newsVideoPlaying_ && newsVideoIdx_ == (int)si);

                        if (isThisPlaying && IsVideoActive()) {
                            // Show video frame
                            int frameW = 0, frameH = 0;
                            ImTextureID frameTex = GetVideoFrame(&frameW, &frameH);
                            if (frameTex && frameW > 0 && frameH > 0) {
                                // Adjust height based on actual video aspect ratio
                                videoH = videoW * ((float)frameH / (float)frameW);
                                vdl->AddImage(frameTex, vp, ImVec2(vp.x + videoW, vp.y + videoH));
                            } else {
                                // Loading - show dark background
                                vdl->AddRectFilled(vp, ImVec2(vp.x + videoW, vp.y + videoH),
                                                   IM_COL32(15, 20, 30, 240), 6.0f);
                                const char* loadingLabel = u8"\u52A0\u8F7D\u89C6\u9891\u4E2D...";
                                ImVec2 loadSz = ImGui::CalcTextSize(loadingLabel);
                                vdl->AddText(ImVec2(vp.x + videoW * 0.5f - loadSz.x * 0.5f,
                                                    vp.y + videoH * 0.5f - loadSz.y * 0.5f),
                                             IM_COL32(180, 200, 220, 200), loadingLabel);
                            }

                            // Stop button overlay
                            float btnR = 24.0f;
                            float btnX = vp.x + videoW - btnR - 12;
                            float btnY = vp.y + btnR + 12;
                            vdl->AddCircleFilled(ImVec2(btnX, btnY), btnR, IM_COL32(200, 60, 60, 200));
                            // Stop square icon
                            float sqS = 8.0f;
                            vdl->AddRectFilled(ImVec2(btnX - sqS, btnY - sqS),
                                               ImVec2(btnX + sqS, btnY + sqS),
                                               IM_COL32(255, 255, 255, 255));

                            // Click to stop
                            ImGui::SetCursorScreenPos(vp);
                            char videoBtnId[64];
                            snprintf(videoBtnId, sizeof(videoBtnId), "##video_%d", (int)si);
                            if (ImGui::InvisibleButton(videoBtnId, ImVec2(videoW, videoH))) {
                                StopVideo();
                                newsVideoPlaying_ = false;
                                newsVideoIdx_ = -1;
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                            }
                        } else {
                            // Show play button placeholder
                            vdl->AddRectFilled(vp, ImVec2(vp.x + videoW, vp.y + videoH),
                                               IM_COL32(15, 20, 30, 240), 6.0f);
                            vdl->AddRect(vp, ImVec2(vp.x + videoW, vp.y + videoH),
                                         IM_COL32(60, 80, 110, 120), 6.0f);

                            // Play button circle
                            float cx = vp.x + videoW * 0.5f;
                            float cy = vp.y + videoH * 0.5f;
                            float btnR = 28.0f;
                            bool hovered = false;

                            // Invisible button for click detection
                            ImGui::SetCursorScreenPos(vp);
                            char videoBtnId[64];
                            snprintf(videoBtnId, sizeof(videoBtnId), "##video_%d", (int)si);
                            if (ImGui::InvisibleButton(videoBtnId, ImVec2(videoW, videoH))) {
                                // Stop any other video first
                                if (newsVideoPlaying_) {
                                    StopVideo();
                                }
                                // Play this video
                                std::string videoUrl = seg.text;
                                // Prefer mp4 over webm for better compatibility
                                if (!seg.videoUrl2.empty()) {
                                    videoUrl = seg.videoUrl2;
                                }
                                if (!videoUrl.empty() && PlayVideo(videoUrl)) {
                                    newsVideoPlaying_ = true;
                                    newsVideoIdx_ = (int)si;
                                }
                            }
                            hovered = ImGui::IsItemHovered();

                            // Draw play button
                            ImU32 btnCol = hovered ? IM_COL32(130, 210, 255, 220) : IM_COL32(102, 192, 244, 200);
                            vdl->AddCircleFilled(ImVec2(cx, cy), btnR, btnCol);
                            // Play triangle
                            float triS = 12.0f;
                            ImVec2 tri[3] = {
                                ImVec2(cx - triS * 0.4f, cy - triS),
                                ImVec2(cx - triS * 0.4f, cy + triS),
                                ImVec2(cx + triS * 0.9f, cy)
                            };
                            vdl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 255, 255, 255));

                            // "Click to play video" text
                            const char* videoLabel = u8"\u70B9\u51FB\u64AD\u653E\u89C6\u9891";
                            ImVec2 labelSize = ImGui::CalcTextSize(videoLabel);
                            vdl->AddText(ImVec2(cx - labelSize.x * 0.5f, cy + btnR + 12),
                                         IM_COL32(180, 200, 220, 200), videoLabel);

                            if (hovered) {
                                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                            }
                        }
                        ImGui::Dummy(ImVec2(videoW, videoH));
                        ImGui::Dummy(ImVec2(0, 4));
                    } else if (seg.type == SteamStoreData::RichSegment::HEADING) {
                        ImGui::Dummy(ImVec2(0, 6));
                        if (seg.headingLevel <= 2 && g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 245, 250, 255));
                        // Use translated text if available
                        if (transResult && transResult->loaded && transIdx < transResult->translatedTexts.size()) {
                            ImGui::TextWrapped("%s", transResult->translatedTexts[transIdx].c_str());
                        } else {
                            ImGui::TextWrapped("%s", seg.text.c_str());
                        }
                        transIdx++;
                        ImGui::PopStyleColor();
                        if (seg.headingLevel <= 2 && g_mainFontLarge) ImGui::PopFont();
                        ImGui::Dummy(ImVec2(0, 4));
                    } else {
                        // TEXT - use translated if available
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 215, 230, 220));
                        if (transResult && transResult->loaded && transIdx < transResult->translatedTexts.size()) {
                            ImGui::TextWrapped("%s", transResult->translatedTexts[transIdx].c_str());
                        } else {
                            ImGui::TextWrapped("%s", seg.text.c_str());
                        }
                        transIdx++;
                        ImGui::PopStyleColor();
                        ImGui::Dummy(ImVec2(0, 2));
                    }
                }
            } else {
                // Fallback to plain text
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 215, 230, 220));
                ImGui::TextWrapped("%s", newsItem.contents.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing(); ImGui::Spacing();
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);

        if (!open) {
            // Stop video when closing popup
            if (newsVideoPlaying_) {
                StopVideo();
                newsVideoPlaying_ = false;
                newsVideoIdx_ = -1;
            }
            // Clear translation cache when closing popup
            char transReqId[64];
            snprintf(transReqId, sizeof(transReqId), "news_%s_%d", selectedAppId_.c_str(), newsPopupIdx_);
            ClearTranslation(transReqId);
            showNewsPopup_ = false;
        }
    }

    // ── DLC List Popup (Grid Layout with Glass Cards) ──
    if (showDlcPopup_ && sd && sd->dlcCount > 0) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(1100, 700), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(800, 500), ImVec2(1400, 900));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(12, 18, 28, 240));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, IM_COL32(10, 14, 22, 255));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, IM_COL32(18, 28, 42, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 90, 130, 100));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 24));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        bool open = showDlcPopup_;
        if (ImGui::Begin(u8"\u53EF\u4E0B\u8F7D\u5185\u5BB9 (DLC)##dlc_popup", &open,
                         ImGuiWindowFlags_NoCollapse)) {
            ImDrawList* pdl = ImGui::GetWindowDrawList();

            // Game name + DLC count header
            if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 245, 250, 255));
            ImGui::TextWrapped("%s", gameName);
            ImGui::PopStyleColor();
            if (g_mainFontLarge) ImGui::PopFont();

            char dlcInfo[128];
            snprintf(dlcInfo, sizeof(dlcInfo), u8"\u5171 %d \u4E2A\u53EF\u4E0B\u8F7D\u5185\u5BB9 (DLC)", sd->dlcCount);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(140, 165, 190, 200));
            ImGui::TextUnformatted(dlcInfo);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImVec2 divP = ImGui::GetCursorScreenPos();
            float contentW = ImGui::GetContentRegionAvail().x;
            pdl->AddLine(divP, ImVec2(divP.x + contentW, divP.y), IM_COL32(102, 192, 244, 40));
            ImGui::Dummy(ImVec2(0, 12));

            // Grid layout parameters
            const float cardW = 280.0f;
            const float cardH = 180.0f;
            const float cardGap = 16.0f;
            const float imgH = 100.0f;  // 460:215 aspect ratio area

            int cols = (int)((contentW + cardGap) / (cardW + cardGap));
            if (cols < 1) cols = 1;

            // Calculate horizontal offset to center the grid
            float gridTotalW = cols * cardW + (cols - 1) * cardGap;
            float gridOffsetX = (contentW - gridTotalW) * 0.5f;
            if (gridOffsetX < 0) gridOffsetX = 0;

            // DLC items grid with proper clipping
            if (!sd->dlcItems.empty()) {
                // Calculate total height needed
                int totalRows = ((int)sd->dlcItems.size() + cols - 1) / cols;
                float totalH = totalRows * (cardH + cardGap);

                ImGui::BeginChild("##dlc_grid_scroll", ImVec2(0, 0), false);
                ImDrawList* cdl = ImGui::GetWindowDrawList();

                // Get scroll region clip rect for proper clipping
                ImVec2 clipMin = ImGui::GetWindowPos();
                ImVec2 clipMax = ImVec2(clipMin.x + ImGui::GetWindowSize().x, clipMin.y + ImGui::GetWindowSize().y);

                float scrollY = ImGui::GetScrollY();
                float childStartX = ImGui::GetWindowPos().x + gridOffsetX;
                float childStartY = ImGui::GetWindowPos().y;  // Use window pos, not cursor pos

                // Render all cards (let clip rect handle visibility)
                for (size_t di = 0; di < sd->dlcItems.size(); di++) {
                    auto& dlc = sd->dlcItems[di];
                    int col = (int)(di % cols);
                    int row = (int)(di / cols);

                    float cx = childStartX + col * (cardW + cardGap);
                    float cy = childStartY + row * (cardH + cardGap) - scrollY;
                    ImVec2 cardMin(cx, cy);
                    ImVec2 cardMax(cx + cardW, cy + cardH);

                    // Skip if completely outside visible area
                    if (cardMax.y < clipMin.y || cardMin.y > clipMax.y) continue;

                    // Hover detection - need to set cursor for InvisibleButton
                    ImGui::SetCursorScreenPos(cardMin);
                    char dlcBtnId[32];
                    snprintf(dlcBtnId, sizeof(dlcBtnId), "##dlccard_%d", (int)di);
                    ImGui::PushID(dlcBtnId);
                    ImGui::InvisibleButton(dlcBtnId, ImVec2(cardW, cardH));
                    bool hov = ImGui::IsItemHovered();
                    bool clicked = ImGui::IsItemClicked();
                    ImGui::PopID();

                    // Push clip rect to prevent drawing outside scroll area
                    cdl->PushClipRect(clipMin, clipMax, true);

                    // Glass card background with gradient
                    ImU32 bgTop = hov ? IM_COL32(45, 65, 95, 200) : IM_COL32(30, 42, 60, 180);
                    ImU32 bgBot = hov ? IM_COL32(35, 50, 75, 220) : IM_COL32(22, 32, 48, 200);
                    cdl->AddRectFilledMultiColor(cardMin, cardMax, bgTop, bgTop, bgBot, bgBot);

                    // Glass border effect
                    ImU32 borderCol = hov ? IM_COL32(102, 192, 244, 150) : IM_COL32(80, 120, 160, 80);
                    cdl->AddRect(cardMin, cardMax, borderCol, 8.0f, 0, 1.5f);

                    // Inner glow on hover
                    if (hov) {
                        cdl->AddRect(ImVec2(cx + 1, cy + 1), ImVec2(cx + cardW - 1, cy + cardH - 1),
                                     IM_COL32(102, 192, 244, 40), 7.0f);
                    }

                    // Top highlight (glass effect)
                    cdl->AddRectFilledMultiColor(cardMin, ImVec2(cx + cardW, cy + 3),
                                                  IM_COL32(255, 255, 255, 25), IM_COL32(255, 255, 255, 25),
                                                  IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));

                    // DLC header image
                    ImTextureID dlcTex = GetTextureByKey("dlc_" + dlc.appId);
                    float imgPad = 8.0f;
                    ImVec2 imgMin(cx + imgPad, cy + imgPad);
                    ImVec2 imgMax(cx + cardW - imgPad, cy + imgPad + imgH);

                    if (dlcTex) {
                        cdl->AddImageRounded(dlcTex, imgMin, imgMax,
                                              ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 240), 6.0f);
                    } else {
                        cdl->AddRectFilled(imgMin, imgMax, IM_COL32(35, 50, 70, 200), 6.0f);
                        const char* loadTxt = dlc.loaded ? "DLC" : u8"\u52A0\u8F7D\u4E2D...";
                        ImVec2 txtSz = ImGui::CalcTextSize(loadTxt);
                        cdl->AddText(ImVec2(cx + (cardW - txtSz.x) * 0.5f, cy + imgPad + (imgH - txtSz.y) * 0.5f),
                                     IM_COL32(100, 130, 160, 180), loadTxt);
                    }

                    // Text area below image - use card-local clip rect
                    float textY = cy + imgPad + imgH + 8;
                    float textX = cx + imgPad;
                    float textMaxW = cardW - imgPad * 2;

                    cdl->PushClipRect(ImVec2(textX, textY), ImVec2(cx + cardW - imgPad, cy + cardH - 8), true);

                    // DLC name (truncated)
                    ImFont* nameFont = g_mainFont ? g_mainFont : ImGui::GetFont();
                    if (dlc.loaded && !dlc.name.empty()) {
                        cdl->AddText(nameFont, nameFont->FontSize, ImVec2(textX, textY),
                                     IM_COL32(230, 240, 250, 255), dlc.name.c_str(), nullptr, textMaxW);
                    } else {
                        cdl->AddText(ImVec2(textX, textY), IM_COL32(150, 170, 190, 180), u8"\u52A0\u8F7D\u4E2D...");
                    }

                    // Price at bottom
                    if (!dlc.price.empty()) {
                        ImFont* smallF = g_mainFontSmall ? g_mainFontSmall : nameFont;
                        float priceY = cy + cardH - smallF->FontSize - 10;
                        if (dlc.discountPercent > 0) {
                            char discBuf[16];
                            snprintf(discBuf, sizeof(discBuf), "-%d%%", dlc.discountPercent);
                            // Discount badge
                            cdl->AddRectFilled(ImVec2(textX, priceY - 2), ImVec2(textX + 40, priceY + smallF->FontSize + 2),
                                               IM_COL32(76, 130, 34, 255), 3.0f);
                            cdl->AddText(smallF, smallF->FontSize, ImVec2(textX + 4, priceY),
                                         IM_COL32(200, 240, 200, 255), discBuf);
                            cdl->AddText(smallF, smallF->FontSize, ImVec2(textX + 48, priceY),
                                         IM_COL32(170, 220, 170, 255), dlc.price.c_str());
                        } else {
                            cdl->AddText(smallF, smallF->FontSize, ImVec2(textX, priceY),
                                         IM_COL32(180, 200, 220, 220), dlc.price.c_str());
                        }
                    }

                    cdl->PopClipRect();  // text clip
                    cdl->PopClipRect();  // scroll area clip

                    // Click -> open DLC detail
                    if (clicked && dlc.loaded) {
                        showDlcDetailPopup_ = true;
                        dlcDetailIdx_ = (int)di;
                    }
                }

                // Reserve space for all rows to enable proper scrolling
                ImGui::SetCursorPos(ImVec2(0, totalH));

                ImGui::EndChild();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(140, 165, 190, 180));
                ImGui::TextWrapped(u8"DLC \u8BE6\u60C5\u52A0\u8F7D\u4E2D...");
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);

        if (!open) showDlcPopup_ = false;
    }

    // ── DLC Detail Popup ──
    if (showDlcDetailPopup_ && sd && dlcDetailIdx_ >= 0 && dlcDetailIdx_ < (int)sd->dlcItems.size()) {
        auto& dlc = sd->dlcItems[dlcDetailIdx_];

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(ImVec2(center.x + 30, center.y), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(700, 550), ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2(450, 350), ImVec2(900, 750));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(18, 26, 38, 250));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, IM_COL32(14, 20, 32, 255));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, IM_COL32(24, 38, 58, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 80, 110, 120));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        bool detailOpen = showDlcDetailPopup_;
        char dlcTitle[256];
        snprintf(dlcTitle, sizeof(dlcTitle), "%s##dlc_detail", dlc.name.empty() ? "DLC" : dlc.name.c_str());
        if (ImGui::Begin(dlcTitle, &detailOpen, ImGuiWindowFlags_NoCollapse)) {
            float contentW = ImGui::GetContentRegionAvail().x;

            // Header image (large, at top)
            ImTextureID dlcTex = GetTextureByKey("dlc_" + dlc.appId);
            if (dlcTex) {
                float imgH = contentW * (215.0f / 460.0f);
                ImGui::Image(dlcTex, ImVec2(contentW, imgH));
                ImGui::Spacing();
            }

            // DLC name
            if (g_mainFontLarge) ImGui::PushFont(g_mainFontLarge);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 245, 250, 255));
            ImGui::TextWrapped("%s", dlc.name.c_str());
            ImGui::PopStyleColor();
            if (g_mainFontLarge) ImGui::PopFont();

            ImGui::Spacing();

            // Price section
            if (!dlc.price.empty()) {
                if (dlc.discountPercent > 0) {
                    char discBuf[16];
                    snprintf(discBuf, sizeof(discBuf), "-%d%%", dlc.discountPercent);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 230, 200, 255));
                    ImGui::TextUnformatted(discBuf);
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (!dlc.originalPrice.empty()) {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(140, 155, 170, 150));
                        ImGui::TextUnformatted(dlc.originalPrice.c_str());
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                    }
                }
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(170, 210, 170, 255));
                ImGui::TextUnformatted(dlc.price.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();

            // Divider
            ImDrawList* ddl = ImGui::GetWindowDrawList();
            ImVec2 dp = ImGui::GetCursorScreenPos();
            ddl->AddLine(dp, ImVec2(dp.x + contentW, dp.y), IM_COL32(102, 192, 244, 40));
            ImGui::Dummy(ImVec2(0, 8));

            // Short description
            if (!dlc.shortDesc.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 200, 220, 220));
                ImGui::TextWrapped("%s", dlc.shortDesc.c_str());
                ImGui::PopStyleColor();
                ImGui::Spacing(); ImGui::Spacing();
            }

            // Full description
            if (!dlc.aboutGame.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 215, 230, 200));
                ImGui::TextWrapped("%s", dlc.aboutGame.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(4);

        if (!detailOpen) showDlcDetailPopup_ = false;
    }
}

// ====================== CommunityPage ======================

void CommunityPage::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(24, 34, 48, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));

    if (ImGui::Begin("##Community", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cx = x + width * 0.5f, cy = y + height * 0.35f;

        if (g_iconFontLarge)
            DrawIconLarge(dl, icon::COMMUNITY, ImVec2(cx - 12, cy - 50), IM_COL32(102, 192, 244, 100));

        if (g_mainFontLarge) {
            const char* t = u8"\u793E\u533A\u4E2D\u5FC3"; // 社区中心
            ImVec2 ts = g_mainFontLarge->CalcTextSizeA(g_mainFontLarge->FontSize, FLT_MAX, 0, t);
            dl->AddText(g_mainFontLarge, g_mainFontLarge->FontSize,
                        ImVec2(cx - ts.x * 0.5f, cy), IM_COL32(255, 255, 255, 220), t);
        }

        const char* sub = u8"\u8BA8\u8BBA\u8BBA\u575B\u3001\u653B\u7565\u6307\u5357\u548C\u521B\u610F\u5DE5\u574A"; // 讨论论坛、攻略指南和创意工坊
        ImVec2 ss = ImGui::CalcTextSize(sub);
        dl->AddText(ImVec2(cx - ss.x * 0.5f, cy + 28), IM_COL32(140, 160, 180, 160), sub);

        const char* badge = u8"\u5373\u5C06\u4E0A\u7EBF"; // 即将上线
        ImVec2 bs = ImGui::CalcTextSize(badge);
        float bw = bs.x + 24, bh = bs.y + 12;

        // Clickable badge
        ImGui::SetCursorScreenPos(ImVec2(cx - bw * 0.5f, cy + 60));
        ImGui::PushID("coming_soon");
        ImGui::InvisibleButton("##cs", ImVec2(bw, bh));
        bool csHov = ImGui::IsItemHovered();
        ImGui::PopID();

        dl->AddRectFilled(ImVec2(cx - bw * 0.5f, cy + 60), ImVec2(cx + bw * 0.5f, cy + 60 + bh),
                          IM_COL32(102, 192, 244, csHov ? 60 : 40), bh * 0.5f);
        dl->AddRect(ImVec2(cx - bw * 0.5f, cy + 60), ImVec2(cx + bw * 0.5f, cy + 60 + bh),
                    IM_COL32(102, 192, 244, csHov ? 150 : 100), bh * 0.5f);
        dl->AddText(ImVec2(cx - bs.x * 0.5f, cy + 60 + 6), IM_COL32(102, 192, 244, 220), badge);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace sf
