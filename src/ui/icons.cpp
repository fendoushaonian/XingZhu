#include "ui/icons.h"
#include <cmath>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

namespace sf { namespace icons {

// Helper: draw a circle outline
static void Circle(ImDrawList* dl, ImVec2 center, float r, ImU32 col, float thickness = 2.0f, int segments = 0) {
    dl->AddCircle(center, r, col, segments, thickness);
}

// Helper: filled circle
static void CircleFilled(ImDrawList* dl, ImVec2 center, float r, ImU32 col) {
    dl->AddCircleFilled(center, r, col);
}

// Helper: line
static void Line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float thickness = 2.0f) {
    dl->AddLine(a, b, col, thickness);
}

// Helper: rounded rect
static void RRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float rounding = 2.0f, float thickness = 2.0f) {
    dl->AddRect(a, b, col, rounding, 0, thickness);
}

static void RRectFilled(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float rounding = 2.0f) {
    dl->AddRectFilled(a, b, col, rounding);
}

// ====================== Icons ======================

// Store: shopping bag
void DrawStore(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.15f;
    // Bag body
    RRect(dl, ImVec2(p.x + m, p.y + s * 0.35f), ImVec2(p.x + s - m, p.y + s - m), c, 3.0f, 1.8f);
    // Handle
    float handleW = s * 0.22f;
    float cx = p.x + s * 0.5f;
    // Arc for handle
    for (int i = 0; i < 12; i++) {
        float a1 = IM_PI + IM_PI * i / 12.0f;
        float a2 = IM_PI + IM_PI * (i + 1) / 12.0f;
        Line(dl,
            ImVec2(cx + cosf(a1) * handleW, p.y + s * 0.35f + sinf(a1) * handleW),
            ImVec2(cx + cosf(a2) * handleW, p.y + s * 0.35f + sinf(a2) * handleW),
            c, 1.8f);
    }
}

// Library: open book / game controller
void DrawLibrary(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.12f;
    float cx = p.x + s * 0.5f;
    // Left page
    RRect(dl, ImVec2(p.x + m, p.y + m), ImVec2(cx - 1, p.y + s - m), c, 2.0f, 1.8f);
    // Right page
    RRect(dl, ImVec2(cx + 1, p.y + m), ImVec2(p.x + s - m, p.y + s - m), c, 2.0f, 1.8f);
    // Spine
    Line(dl, ImVec2(cx, p.y + m - 2), ImVec2(cx, p.y + s - m + 2), c, 1.8f);
    // Lines on left page
    for (int i = 0; i < 3; i++) {
        float ly = p.y + m + s * 0.15f + i * s * 0.16f;
        Line(dl, ImVec2(p.x + m + 4, ly), ImVec2(cx - 5, ly), c, 1.2f);
    }
}

// Free Games: gift box
void DrawFreeGames(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.12f;
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.42f;
    // Box bottom
    RRect(dl, ImVec2(p.x + m, cy), ImVec2(p.x + s - m, p.y + s - m), c, 2.0f, 1.8f);
    // Box lid
    RRect(dl, ImVec2(p.x + m - 2, cy - s * 0.14f), ImVec2(p.x + s - m + 2, cy), c, 2.0f, 1.8f);
    // Ribbon vertical
    Line(dl, ImVec2(cx, cy - s * 0.14f), ImVec2(cx, p.y + s - m), c, 1.8f);
    // Ribbon bow
    dl->AddCircle(ImVec2(cx - s * 0.1f, cy - s * 0.22f), s * 0.08f, c, 8, 1.5f);
    dl->AddCircle(ImVec2(cx + s * 0.1f, cy - s * 0.22f), s * 0.08f, c, 8, 1.5f);
}

// Tools: wrench
void DrawTools(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;
    // Gear outer circle
    Circle(dl, ImVec2(cx, cy), s * 0.32f, c, 1.8f);
    // Gear inner circle
    Circle(dl, ImVec2(cx, cy), s * 0.16f, c, 1.8f);
    // Gear teeth (8 teeth)
    for (int i = 0; i < 8; i++) {
        float angle = IM_PI * 2.0f * i / 8.0f;
        float r1 = s * 0.30f;
        float r2 = s * 0.40f;
        float tw = 0.15f;
        ImVec2 a1(cx + cosf(angle - tw) * r1, cy + sinf(angle - tw) * r1);
        ImVec2 a2(cx + cosf(angle - tw) * r2, cy + sinf(angle - tw) * r2);
        ImVec2 b1(cx + cosf(angle + tw) * r1, cy + sinf(angle + tw) * r1);
        ImVec2 b2(cx + cosf(angle + tw) * r2, cy + sinf(angle + tw) * r2);
        Line(dl, a1, a2, c, 1.8f);
        Line(dl, a2, b2, c, 1.8f);
        Line(dl, b2, b1, c, 1.8f);
    }
}

// Cards: playing cards stacked
void DrawCards(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.1f;
    // Back card (offset)
    RRect(dl, ImVec2(p.x + m + 4, p.y + m), ImVec2(p.x + s - m + 4, p.y + s - m - 4), c, 3.0f, 1.5f);
    // Front card
    RRect(dl, ImVec2(p.x + m, p.y + m + 4), ImVec2(p.x + s - m, p.y + s - m), c, 3.0f, 1.8f);
    // Diamond symbol on front card
    float cx = p.x + s * 0.45f;
    float dy = p.y + s * 0.55f;
    float ds = s * 0.1f;
    ImVec2 diamond[4] = {
        ImVec2(cx, dy - ds),
        ImVec2(cx + ds * 0.7f, dy),
        ImVec2(cx, dy + ds),
        ImVec2(cx - ds * 0.7f, dy)
    };
    dl->AddPolyline(diamond, 4, c, ImDrawFlags_Closed, 1.5f);
}

// Achievement: trophy
void DrawAchievement(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float m = s * 0.14f;
    // Cup body (trapezoid-ish)
    ImVec2 cup[4] = {
        ImVec2(cx - s * 0.25f, p.y + m),
        ImVec2(cx + s * 0.25f, p.y + m),
        ImVec2(cx + s * 0.15f, p.y + s * 0.55f),
        ImVec2(cx - s * 0.15f, p.y + s * 0.55f)
    };
    dl->AddPolyline(cup, 4, c, ImDrawFlags_Closed, 1.8f);
    // Handles
    dl->AddBezierQuadratic(
        ImVec2(cx - s * 0.25f, p.y + m + 4),
        ImVec2(cx - s * 0.42f, p.y + s * 0.35f),
        ImVec2(cx - s * 0.18f, p.y + s * 0.48f),
        c, 1.5f);
    dl->AddBezierQuadratic(
        ImVec2(cx + s * 0.25f, p.y + m + 4),
        ImVec2(cx + s * 0.42f, p.y + s * 0.35f),
        ImVec2(cx + s * 0.18f, p.y + s * 0.48f),
        c, 1.5f);
    // Stem
    Line(dl, ImVec2(cx, p.y + s * 0.55f), ImVec2(cx, p.y + s * 0.70f), c, 1.8f);
    // Base
    Line(dl, ImVec2(cx - s * 0.18f, p.y + s * 0.70f), ImVec2(cx + s * 0.18f, p.y + s * 0.70f), c, 1.8f);
    Line(dl, ImVec2(cx - s * 0.22f, p.y + s - m), ImVec2(cx + s * 0.22f, p.y + s - m), c, 2.0f);
    // Star inside cup
    float starCx = cx, starCy = p.y + s * 0.33f, starR = s * 0.08f;
    CircleFilled(dl, ImVec2(starCx, starCy), starR, c);
}

// Cloud: cloud shape
void DrawCloud(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.52f;
    // Main body
    Circle(dl, ImVec2(cx, cy), s * 0.18f, c, 1.8f);
    // Left bump
    Circle(dl, ImVec2(cx - s * 0.18f, cy + s * 0.04f), s * 0.14f, c, 1.8f);
    // Right bump
    Circle(dl, ImVec2(cx + s * 0.2f, cy + s * 0.02f), s * 0.16f, c, 1.8f);
    // Top bump
    Circle(dl, ImVec2(cx + s * 0.04f, cy - s * 0.12f), s * 0.15f, c, 1.8f);
    // Bottom line
    float baseY = cy + s * 0.18f;
    Line(dl, ImVec2(cx - s * 0.30f, baseY), ImVec2(cx + s * 0.34f, baseY), c, 1.8f);
    // Up arrow inside
    Line(dl, ImVec2(cx, baseY + s * 0.05f), ImVec2(cx, baseY + s * 0.20f), c, 1.5f);
    Line(dl, ImVec2(cx - s * 0.07f, baseY + s * 0.12f), ImVec2(cx, baseY + s * 0.05f), c, 1.5f);
    Line(dl, ImVec2(cx + s * 0.07f, baseY + s * 0.12f), ImVec2(cx, baseY + s * 0.05f), c, 1.5f);
}

// Settings: gear
void DrawSettings(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;
    Circle(dl, ImVec2(cx, cy), s * 0.15f, c, 1.8f);
    for (int i = 0; i < 6; i++) {
        float angle = IM_PI * 2.0f * i / 6.0f;
        float r1 = s * 0.24f;
        float r2 = s * 0.38f;
        float tw = 0.25f;
        ImVec2 a(cx + cosf(angle - tw) * r2, cy + sinf(angle - tw) * r2);
        ImVec2 b(cx + cosf(angle + tw) * r2, cy + sinf(angle + tw) * r2);
        ImVec2 a1(cx + cosf(angle - tw) * r1, cy + sinf(angle - tw) * r1);
        ImVec2 b1(cx + cosf(angle + tw) * r1, cy + sinf(angle + tw) * r1);
        Line(dl, a1, a, c, 2.0f);
        Line(dl, a, b, c, 2.0f);
        Line(dl, b, b1, c, 2.0f);
    }
}

// Info: (i) in circle
void DrawInfo(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;
    Circle(dl, ImVec2(cx, cy), s * 0.38f, c, 1.8f);
    CircleFilled(dl, ImVec2(cx, cy - s * 0.14f), s * 0.045f, c);
    Line(dl, ImVec2(cx, cy - s * 0.04f), ImVec2(cx, cy + s * 0.20f), c, 2.0f);
}

// Search: magnifying glass
void DrawSearch(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.42f;
    float cy = p.y + s * 0.42f;
    float r = s * 0.24f;
    Circle(dl, ImVec2(cx, cy), r, c, 2.0f);
    float dx = r * 0.707f;
    Line(dl, ImVec2(cx + dx, cy + dx), ImVec2(p.x + s * 0.82f, p.y + s * 0.82f), c, 2.5f);
}

// Grid: 4 squares
void DrawGrid(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.15f;
    float gap = s * 0.06f;
    float bw = (s - 2 * m - gap) * 0.5f;
    RRectFilled(dl, ImVec2(p.x + m, p.y + m), ImVec2(p.x + m + bw, p.y + m + bw), c, 2.0f);
    RRectFilled(dl, ImVec2(p.x + m + bw + gap, p.y + m), ImVec2(p.x + s - m, p.y + m + bw), c, 2.0f);
    RRectFilled(dl, ImVec2(p.x + m, p.y + m + bw + gap), ImVec2(p.x + m + bw, p.y + s - m), c, 2.0f);
    RRectFilled(dl, ImVec2(p.x + m + bw + gap, p.y + m + bw + gap), ImVec2(p.x + s - m, p.y + s - m), c, 2.0f);
}

// List: 3 horizontal lines
void DrawList(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.18f;
    for (int i = 0; i < 3; i++) {
        float y = p.y + m + i * s * 0.26f;
        CircleFilled(dl, ImVec2(p.x + m + 2, y + 2), 2.5f, c);
        Line(dl, ImVec2(p.x + m + 10, y + 2), ImVec2(p.x + s - m, y + 2), c, 2.0f);
    }
}

// Play: triangle
void DrawPlay(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f + s * 0.04f;
    float cy = p.y + s * 0.5f;
    float hs = s * 0.30f;
    ImVec2 tri[3] = {
        ImVec2(cx - hs * 0.6f, cy - hs),
        ImVec2(cx - hs * 0.6f, cy + hs),
        ImVec2(cx + hs * 0.8f, cy)
    };
    dl->AddTriangleFilled(tri[0], tri[1], tri[2], c);
}

// Download: arrow down + line
void DrawDownload(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float m = s * 0.15f;
    // Arrow shaft
    Line(dl, ImVec2(cx, p.y + m), ImVec2(cx, p.y + s * 0.62f), c, 2.0f);
    // Arrow head
    Line(dl, ImVec2(cx - s * 0.15f, p.y + s * 0.50f), ImVec2(cx, p.y + s * 0.65f), c, 2.0f);
    Line(dl, ImVec2(cx + s * 0.15f, p.y + s * 0.50f), ImVec2(cx, p.y + s * 0.65f), c, 2.0f);
    // Tray
    Line(dl, ImVec2(p.x + m, p.y + s * 0.70f), ImVec2(p.x + m, p.y + s - m), c, 1.8f);
    Line(dl, ImVec2(p.x + m, p.y + s - m), ImVec2(p.x + s - m, p.y + s - m), c, 1.8f);
    Line(dl, ImVec2(p.x + s - m, p.y + s - m), ImVec2(p.x + s - m, p.y + s * 0.70f), c, 1.8f);
}

// Check: checkmark
void DrawCheck(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;
    Line(dl, ImVec2(cx - s * 0.22f, cy), ImVec2(cx - s * 0.05f, cy + s * 0.18f), c, 2.5f);
    Line(dl, ImVec2(cx - s * 0.05f, cy + s * 0.18f), ImVec2(cx + s * 0.25f, cy - s * 0.18f), c, 2.5f);
}

// User: person silhouette
void DrawUser(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    // Head
    Circle(dl, ImVec2(cx, p.y + s * 0.30f), s * 0.15f, c, 1.8f);
    // Body (arc)
    dl->AddBezierQuadratic(
        ImVec2(cx - s * 0.30f, p.y + s * 0.82f),
        ImVec2(cx, p.y + s * 0.48f),
        ImVec2(cx + s * 0.30f, p.y + s * 0.82f),
        c, 1.8f);
    Line(dl, ImVec2(cx - s * 0.30f, p.y + s * 0.82f), ImVec2(cx + s * 0.30f, p.y + s * 0.82f), c, 1.8f);
}

// Notification: bell
void DrawNotification(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float m = s * 0.12f;
    // Bell body
    dl->AddBezierQuadratic(
        ImVec2(cx - s * 0.28f, p.y + s * 0.65f),
        ImVec2(cx, p.y + m),
        ImVec2(cx + s * 0.28f, p.y + s * 0.65f),
        c, 1.8f);
    // Bell base
    Line(dl, ImVec2(cx - s * 0.32f, p.y + s * 0.65f), ImVec2(cx + s * 0.32f, p.y + s * 0.65f), c, 1.8f);
    // Clapper
    CircleFilled(dl, ImVec2(cx, p.y + s * 0.78f), s * 0.06f, c);
    // Top knob
    CircleFilled(dl, ImVec2(cx, p.y + m - 1), s * 0.04f, c);
}

// Network: signal bars
void DrawNetwork(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.15f;
    float barW = s * 0.12f;
    float gap = s * 0.06f;
    float baseY = p.y + s - m;
    for (int i = 0; i < 4; i++) {
        float barH = s * 0.15f * (i + 1);
        float x = p.x + m + i * (barW + gap);
        RRectFilled(dl, ImVec2(x, baseY - barH), ImVec2(x + barW, baseY), c, 1.5f);
    }
}

// Shield: shield shape
void DrawShield(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float m = s * 0.12f;
    ImVec2 pts[5] = {
        ImVec2(cx, p.y + m),
        ImVec2(p.x + s - m, p.y + s * 0.28f),
        ImVec2(p.x + s - m - s * 0.06f, p.y + s * 0.65f),
        ImVec2(cx, p.y + s - m),
        ImVec2(p.x + m + s * 0.06f, p.y + s * 0.65f),
    };
    // Close it with left side
    dl->AddPolyline(pts, 5, c, ImDrawFlags_Closed, 1.8f);
    // Checkmark inside
    DrawCheck(dl, ImVec2(p.x + s * 0.22f, p.y + s * 0.15f), s * 0.56f, c);
}

// Chart: bar chart
void DrawChart(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.15f;
    float baseY = p.y + s - m;
    float barW = s * 0.13f;
    float gap = s * 0.05f;
    float heights[] = { 0.3f, 0.6f, 0.45f, 0.75f };
    for (int i = 0; i < 4; i++) {
        float x = p.x + m + i * (barW + gap);
        float barH = (s - 2 * m) * heights[i];
        RRectFilled(dl, ImVec2(x, baseY - barH), ImVec2(x + barW, baseY), c, 1.5f);
    }
    // Axis
    Line(dl, ImVec2(p.x + m - 2, p.y + m), ImVec2(p.x + m - 2, baseY), c, 1.5f);
    Line(dl, ImVec2(p.x + m - 2, baseY), ImVec2(p.x + s - m, baseY), c, 1.5f);
}

// Home: house
void DrawHome(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float m = s * 0.12f;
    // Roof
    ImVec2 roof[3] = {
        ImVec2(cx, p.y + m),
        ImVec2(p.x + s - m, p.y + s * 0.48f),
        ImVec2(p.x + m, p.y + s * 0.48f)
    };
    dl->AddPolyline(roof, 3, c, ImDrawFlags_Closed, 1.8f);
    // Body
    RRect(dl, ImVec2(p.x + s * 0.22f, p.y + s * 0.48f), ImVec2(p.x + s * 0.78f, p.y + s - m), c, 0, 1.8f);
    // Door
    RRect(dl, ImVec2(cx - s * 0.08f, p.y + s * 0.60f), ImVec2(cx + s * 0.08f, p.y + s - m), c, 1.0f, 1.5f);
}

// Back: left arrow
void DrawBack(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;
    Line(dl, ImVec2(cx + s * 0.2f, cy), ImVec2(cx - s * 0.2f, cy), c, 2.0f);
    Line(dl, ImVec2(cx - s * 0.2f, cy), ImVec2(cx - s * 0.05f, cy - s * 0.15f), c, 2.0f);
    Line(dl, ImVec2(cx - s * 0.2f, cy), ImVec2(cx - s * 0.05f, cy + s * 0.15f), c, 2.0f);
}

// Close: X
void DrawClose(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.25f;
    Line(dl, ImVec2(p.x + m, p.y + m), ImVec2(p.x + s - m, p.y + s - m), c, 2.0f);
    Line(dl, ImVec2(p.x + s - m, p.y + m), ImVec2(p.x + m, p.y + s - m), c, 2.0f);
}

// Minimize: horizontal line
void DrawMinimize(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cy = p.y + s * 0.5f;
    float m = s * 0.25f;
    Line(dl, ImVec2(p.x + m, cy), ImVec2(p.x + s - m, cy), c, 2.0f);
}

// Maximize: square
void DrawMaximize(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.25f;
    RRect(dl, ImVec2(p.x + m, p.y + m), ImVec2(p.x + s - m, p.y + s - m), c, 1.0f, 2.0f);
}

// Translate: globe with "A文" text overlay (language translation icon)
void DrawTranslate(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;
    float r = s * 0.36f;

    // Globe circle
    Circle(dl, ImVec2(cx, cy), r, c, 1.8f);

    // Horizontal line (equator)
    Line(dl, ImVec2(cx - r, cy), ImVec2(cx + r, cy), c, 1.2f);

    // Vertical ellipse (meridian)
    float ellipseW = r * 0.5f;
    for (int i = 0; i < 16; i++) {
        float a1 = IM_PI * 2.0f * i / 16.0f;
        float a2 = IM_PI * 2.0f * (i + 1) / 16.0f;
        Line(dl,
            ImVec2(cx + cosf(a1) * ellipseW, cy + sinf(a1) * r),
            ImVec2(cx + cosf(a2) * ellipseW, cy + sinf(a2) * r),
            c, 1.2f);
    }

    // "A" letter on left side
    float ax = cx - s * 0.18f;
    float ay = cy + s * 0.02f;
    float ah = s * 0.22f;
    float aw = s * 0.12f;
    Line(dl, ImVec2(ax, ay + ah), ImVec2(ax + aw * 0.5f, ay), c, 1.5f);  // Left stroke
    Line(dl, ImVec2(ax + aw * 0.5f, ay), ImVec2(ax + aw, ay + ah), c, 1.5f);  // Right stroke
    Line(dl, ImVec2(ax + aw * 0.2f, ay + ah * 0.6f), ImVec2(ax + aw * 0.8f, ay + ah * 0.6f), c, 1.2f);  // Crossbar

    // "文" simplified on right side (two crossing strokes)
    float wx = cx + s * 0.08f;
    float wy = cy - s * 0.02f;
    float wh = s * 0.24f;
    float ww = s * 0.18f;
    // Top horizontal
    Line(dl, ImVec2(wx, wy), ImVec2(wx + ww, wy), c, 1.5f);
    // Left diagonal
    Line(dl, ImVec2(wx + ww * 0.5f, wy + wh * 0.15f), ImVec2(wx, wy + wh), c, 1.5f);
    // Right diagonal
    Line(dl, ImVec2(wx + ww * 0.5f, wy + wh * 0.15f), ImVec2(wx + ww, wy + wh), c, 1.5f);
}

// Memory: RAM chip / memory icon
void DrawMemory(ImDrawList* dl, ImVec2 p, float s, ImU32 c) {
    float m = s * 0.12f;
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;

    // Main chip body
    RRect(dl, ImVec2(p.x + m + s * 0.08f, p.y + m + s * 0.1f),
          ImVec2(p.x + s - m - s * 0.08f, p.y + s - m - s * 0.1f), c, 2.0f, 1.8f);

    // Pins on left side
    for (int i = 0; i < 3; i++) {
        float py = p.y + m + s * 0.2f + i * s * 0.18f;
        Line(dl, ImVec2(p.x + m, py), ImVec2(p.x + m + s * 0.08f, py), c, 1.5f);
    }

    // Pins on right side
    for (int i = 0; i < 3; i++) {
        float py = p.y + m + s * 0.2f + i * s * 0.18f;
        Line(dl, ImVec2(p.x + s - m - s * 0.08f, py), ImVec2(p.x + s - m, py), c, 1.5f);
    }

    // Inner detail - small squares representing memory cells
    float cellSize = s * 0.08f;
    float startX = cx - cellSize * 1.2f;
    float startY = cy - cellSize * 0.6f;
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            RRectFilled(dl,
                ImVec2(startX + col * (cellSize + 2), startY + row * (cellSize + 2)),
                ImVec2(startX + col * (cellSize + 2) + cellSize, startY + row * (cellSize + 2) + cellSize),
                c, 1.0f);
        }
    }
}

}} // namespace sf::icons
