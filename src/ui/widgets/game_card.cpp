#include "ui/widgets/game_card.h"
#include "app/config.h"
#include "ui/widgets/modern.h"
#include <cstdio>
#include <algorithm>

namespace sf {

void GameCard::GetGameColors(const std::string& appId, ImU32& primary, ImU32& secondary) {
    unsigned hash = 0;
    for (char c : appId) hash = hash * 31 + c;

    // Generate pleasant dark game-themed colors
    int hue = hash % 360;
    float h = hue / 60.0f;
    int hi = (int)h % 6;
    float f = h - (int)h;

    // Create two related colors (darker primary, slightly lighter secondary)
    int r1, g1, b1, r2, g2, b2;
    int v = 120 + (hash >> 8) % 40;
    int s_val = 60 + (hash >> 16) % 30;
    int p = v * (100 - s_val) / 100;
    int q = v * (100 - (int)(s_val * f)) / 100;
    int t = v * (100 - (int)(s_val * (1 - f))) / 100;

    switch (hi) {
        case 0: r1 = v; g1 = t; b1 = p; break;
        case 1: r1 = q; g1 = v; b1 = p; break;
        case 2: r1 = p; g1 = v; b1 = t; break;
        case 3: r1 = p; g1 = q; b1 = v; break;
        case 4: r1 = t; g1 = p; b1 = v; break;
        default: r1 = v; g1 = p; b1 = q; break;
    }

    r2 = std::min(255, r1 + 30);
    g2 = std::min(255, g1 + 30);
    b2 = std::min(255, b1 + 30);

    primary   = IM_COL32(r1, g1, b1, 255);
    secondary = IM_COL32(r2, g2, b2, 255);
}

bool GameCard::Render(const GameInfo& game, float width, float height, bool isSelected) {
    bool clicked = false;

    ImGui::PushID(game.appId.c_str());

    ImGuiID id = ImGui::GetID("##card");
    float& anim = ui::GetAnimState(id);

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 size(width, height);

    ImGui::InvisibleButton("##card", size);
    clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    float dt = ImGui::GetIO().DeltaTime;
    anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 10.0f;
    anim = std::clamp(anim, 0.0f, 1.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Card shadow (on hover)
    if (anim > 0.01f) {
        dl->AddRectFilled(
            ImVec2(cursor.x + 2, cursor.y + 3),
            ImVec2(cursor.x + width + 2, cursor.y + height + 3),
            IM_COL32(0, 0, 0, (int)(40 * anim)), 10.0f
        );
    }

    // Card background
    ImU32 cardBg = isSelected ? IM_COL32(27, 40, 56, 240) : IM_COL32(22, 32, 45, 220);
    dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height), cardBg, 10.0f);

    // ---- Cover art area ----
    float coverH = height * 0.58f;
    ImU32 col1, col2;
    GetGameColors(game.appId, col1, col2);

    // Cover gradient background
    dl->AddRectFilledMultiColor(
        ImVec2(cursor.x, cursor.y),
        ImVec2(cursor.x + width, cursor.y + coverH),
        col1, col2, col2, col1
    );
    // Round top corners by drawing over
    dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + coverH), IM_COL32(0,0,0,0), 10.0f);

    // Bottom gradient fade into card bg
    dl->AddRectFilledMultiColor(
        ImVec2(cursor.x, cursor.y + coverH - 40),
        ImVec2(cursor.x + width, cursor.y + coverH),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(22, 32, 45, 220), IM_COL32(22, 32, 45, 220)
    );

    // Game name on cover (with text shadow)
    float nameY = cursor.y + coverH - 28;
    dl->AddText(ImVec2(cursor.x + 13, nameY + 1), IM_COL32(0, 0, 0, 150), game.name.c_str());
    dl->AddText(ImVec2(cursor.x + 12, nameY), IM_COL32(255, 255, 255, 240), game.name.c_str());

    // ---- Info area below cover ----
    float infoY = cursor.y + coverH + 6;

    // Status badge (pill shape)
    ImU32 statusBg, statusFg;
    const char* statusText;
    if (game.status == "installed") {
        statusBg = IM_COL32(76, 107, 34, 200);
        statusFg = IM_COL32(190, 230, 80, 255);
        statusText = "READY";
    } else if (game.status == "playing") {
        statusBg = IM_COL32(26, 130, 210, 200);
        statusFg = IM_COL32(140, 210, 255, 255);
        statusText = "PLAYING";
    } else if (game.status == "updating") {
        statusBg = IM_COL32(180, 130, 20, 200);
        statusFg = IM_COL32(255, 210, 80, 255);
        statusText = "UPDATING";
    } else {
        statusBg = IM_COL32(60, 60, 60, 180);
        statusFg = IM_COL32(140, 140, 140, 255);
        statusText = "NOT INSTALLED";
    }

    ImVec2 badgeTextSize = ImGui::CalcTextSize(statusText);
    float badgePadX = 8.0f, badgePadY = 3.0f;
    float badgeW = badgeTextSize.x + badgePadX * 2;
    float badgeH = badgeTextSize.y + badgePadY * 2;
    dl->AddRectFilled(
        ImVec2(cursor.x + 12, infoY),
        ImVec2(cursor.x + 12 + badgeW, infoY + badgeH),
        statusBg, badgeH * 0.5f
    );
    dl->AddText(ImVec2(cursor.x + 12 + badgePadX, infoY + badgePadY), statusFg, statusText);

    // Playtime
    char timeBuf[32];
    if (game.playtime >= 1000) {
        snprintf(timeBuf, sizeof(timeBuf), "%.1fk hrs", game.playtime / 1000.0f);
    } else {
        snprintf(timeBuf, sizeof(timeBuf), "%.0f hrs", game.playtime);
    }
    dl->AddText(ImVec2(cursor.x + 12, infoY + badgeH + 6),
                IM_COL32(143, 152, 160, 200), timeBuf);

    // Trading card indicator
    if (game.hasTradingCards) {
        float cardIconX = cursor.x + width - 28;
        icons::DrawCards(dl, ImVec2(cardIconX, infoY), 16, IM_COL32(180, 160, 80, 180));
    }

    // Achievement mini progress bar
    if (game.achievementsTotal > 0) {
        float progress = (float)game.achievements / (float)game.achievementsTotal;
        float barX = cursor.x + 12;
        float barY = cursor.y + height - 16;
        float barW = width - 24;
        float barH = 3.0f;

        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                          IM_COL32(30, 36, 44, 255), 2.0f);

        ImU32 barCol = (progress >= 1.0f) ? IM_COL32(255, 215, 0, 255) : IM_COL32(26, 159, 255, 255);
        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * progress, barY + barH),
                          barCol, 2.0f);
    }

    // Hover border glow
    if (anim > 0.01f) {
        dl->AddRect(cursor, ImVec2(cursor.x + width, cursor.y + height),
                    IM_COL32(26, 159, 255, (int)(100 * anim)), 10.0f, 0, 1.5f);
    }

    // Selected border
    if (isSelected) {
        dl->AddRect(cursor, ImVec2(cursor.x + width, cursor.y + height),
                    IM_COL32(26, 159, 255, 200), 10.0f, 0, 2.0f);
    }

    // Hover play icon overlay
    if (anim > 0.3f && game.status == "installed") {
        float playSize = 32.0f;
        float playCx = cursor.x + width * 0.5f - playSize * 0.5f;
        float playCy = cursor.y + coverH * 0.4f - playSize * 0.5f;

        dl->AddCircleFilled(
            ImVec2(playCx + playSize * 0.5f, playCy + playSize * 0.5f),
            playSize * 0.6f,
            IM_COL32(0, 0, 0, (int)(140 * anim))
        );
        icons::DrawPlay(dl, ImVec2(playCx, playCy), playSize,
                        IM_COL32(255, 255, 255, (int)(220 * anim)));
    }

    ImGui::PopID();
    return clicked;
}

bool GameCard::RenderListItem(const GameInfo& game, float width, bool isSelected) {
    bool clicked = false;

    ImGui::PushID(game.appId.c_str());

    ImGuiID id = ImGui::GetID("##listrow");
    float& anim = ui::GetAnimState(id);

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float height = 52.0f;

    ImGui::InvisibleButton("##listrow", ImVec2(width, height));
    clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    float dt = ImGui::GetIO().DeltaTime;
    anim += ((hovered ? 1.0f : 0.0f) - anim) * dt * 12.0f;
    anim = std::clamp(anim, 0.0f, 1.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Row background
    if (isSelected) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                          IM_COL32(26, 159, 255, 25), 6.0f);
    } else if (anim > 0.01f) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                          IM_COL32(255, 255, 255, (int)(10 * anim)), 6.0f);
    }

    // Active indicator
    if (isSelected) {
        dl->AddRectFilled(cursor, ImVec2(cursor.x + 3, cursor.y + height),
                          IM_COL32(26, 159, 255, 255), 1.5f);
    }

    // Color swatch (game icon placeholder)
    ImU32 col1, col2;
    GetGameColors(game.appId, col1, col2);
    dl->AddRectFilledMultiColor(
        ImVec2(cursor.x + 14, cursor.y + 8),
        ImVec2(cursor.x + 48, cursor.y + 44),
        col1, col2, col2, col1
    );
    dl->AddRect(ImVec2(cursor.x + 14, cursor.y + 8), ImVec2(cursor.x + 48, cursor.y + 44),
                IM_COL32(255, 255, 255, 20), 4.0f);

    // Game name
    ImU32 nameCol = isSelected ? IM_COL32(255, 255, 255, 245) : IM_COL32(199, 213, 224, 230);
    dl->AddText(ImVec2(cursor.x + 58, cursor.y + 8), nameCol, game.name.c_str());

    // Playtime
    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f hrs", game.playtime);
    dl->AddText(ImVec2(cursor.x + 58, cursor.y + 28), IM_COL32(143, 152, 160, 180), buf);

    // Trading cards indicator
    if (game.hasTradingCards) {
        icons::DrawCards(dl, ImVec2(cursor.x + width - 90, cursor.y + 18), 14,
                         IM_COL32(180, 160, 80, 150));
    }

    // Status on right
    const char* statusText;
    ImU32 statusCol;
    if (game.status == "installed") {
        statusText = "Ready";
        statusCol = IM_COL32(87, 203, 100, 230);
    } else if (game.status == "playing") {
        statusText = "Playing";
        statusCol = IM_COL32(26, 159, 255, 230);
    } else if (game.status == "updating") {
        statusText = "Updating";
        statusCol = IM_COL32(230, 180, 50, 230);
    } else {
        statusText = "---";
        statusCol = IM_COL32(100, 100, 100, 180);
    }
    ImVec2 statusSize = ImGui::CalcTextSize(statusText);
    dl->AddText(ImVec2(cursor.x + width - statusSize.x - 16, cursor.y + 18), statusCol, statusText);

    // Subtle bottom separator
    dl->AddRectFilledMultiColor(
        ImVec2(cursor.x + 14, cursor.y + height - 1),
        ImVec2(cursor.x + width - 14, cursor.y + height),
        IM_COL32(42, 71, 94, 0), IM_COL32(42, 71, 94, 60),
        IM_COL32(42, 71, 94, 60), IM_COL32(42, 71, 94, 0)
    );

    ImGui::PopID();
    return clicked;
}

} // namespace sf
