#pragma once
#include "imgui.h"
#include "ui/widgets/game_card.h"
#include <string>

namespace sf {

class GamePropertiesPanel {
public:
    void Render(float winW, float winH);
    void Open(const GameInfo& game);
    bool IsOpen() const { return open_; }

private:
    bool open_ = false;
    float anim_ = 0.0f;
    GameInfo game_;
    bool storeRequested_ = false;
    bool headerTexRequested_ = false;
};

} // namespace sf
