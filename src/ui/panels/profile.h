#pragma once
#include "imgui.h"
#include <string>

namespace sf {

class ProfilePanel {
public:
    void Render(float winW, float winH);
    void Open();
    bool IsOpen() const { return open_; }

private:
    bool open_ = false;
    float anim_ = 0.0f;

    // Editable fields (loaded from DB on open)
    char editNick_[64] = {};
    bool dirty_ = false;
    std::string saveMsg_;
    float saveMsgTimer_ = 0.0f;

    // Steam ID
    char editSteamId_[64] = {};
    bool steamIdDirty_ = false;
    std::string steamIdMsg_;
    float steamIdMsgTimer_ = 0.0f;

    // Avatar
    std::string avatarPath_; // local file path
    ImTextureID avatarTex_ = (ImTextureID)0;
    bool avatarLoaded_ = false;
};

} // namespace sf
