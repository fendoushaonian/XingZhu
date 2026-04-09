#pragma once
#include "imgui.h"
#include "ui/icons.h"
#include <string>

namespace sf {

struct GameInfo {
    std::string name;
    std::string appId;
    std::string status;      // "installed", "not_installed", "playing", "updating"
    float       playtime;    // hours
    float       size;        // GB
    int         achievements;
    int         achievementsTotal;
    bool        hasTradingCards;
};

class GameCard {
public:
    // Render a modern game card with cover, gradient overlay, hover effects
    static bool Render(const GameInfo& game, float width, float height, bool isSelected);

    // Render a modern list row item
    static bool RenderListItem(const GameInfo& game, float width, bool isSelected);

private:
    // Generate a unique gradient color from appId
    static void GetGameColors(const std::string& appId, ImU32& primary, ImU32& secondary);
};

} // namespace sf
