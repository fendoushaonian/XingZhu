#pragma once
#include "imgui.h"
#include <string>

namespace sf {

class VipPanel {
public:
    void Render(float winW, float winH);
    void Open();
    bool IsOpen() const { return open_; }

private:
    bool open_ = false;
    float anim_ = 0.0f;
    int selectedPlan_ = 0; // 0=monthly, 1=yearly
    std::string statusMsg_;
    float msgTimer_ = 0.0f;
};

} // namespace sf
