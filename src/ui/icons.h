#pragma once
#include "imgui.h"

namespace sf { namespace icons {

// All icons are drawn as vector graphics using ImDrawList
// Size is the bounding box dimension (square)
// Color is the icon fill color

void DrawStore(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawLibrary(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawFreeGames(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawTools(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawCards(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawAchievement(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawCloud(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawSettings(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawInfo(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawSearch(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawGrid(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawList(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawPlay(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawDownload(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawCheck(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawUser(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawNotification(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawNetwork(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawShield(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawChart(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawHome(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawBack(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawClose(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawMinimize(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawMaximize(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawTranslate(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);
void DrawMemory(ImDrawList* dl, ImVec2 pos, float size, ImU32 color);

// Icon drawing function type
using DrawFunc = void(*)(ImDrawList*, ImVec2, float, ImU32);

}} // namespace sf::icons
