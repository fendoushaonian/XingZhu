#pragma once
#include "imgui.h"
#include <string>

namespace sf {

class SettingsPanel {
public:
    void Render(float x, float y, float width, float height);

private:
    // Steam API settings
    char apiKey_[128]  = "";
    char steamId_[64]  = "";

    // General
    bool autoStart_       = false;
    bool minimizeToTray_  = true;
    bool checkUpdates_    = true;

    // Card farmer
    int  farmMethod_      = 2;  // 0=normal, 1=fast, 2=smart
    bool farmOnStartup_   = false;

    // Network
    bool useProxy_        = false;
    char proxyHost_[128]  = "";
    int  proxyPort_       = 7890;

    // Appearance
    int  uiScale_         = 1;  // 0=small, 1=medium, 2=large
    bool showAnimations_  = true;
};

class AboutPanel {
public:
    void Render(float x, float y, float width, float height);
};

} // namespace sf
