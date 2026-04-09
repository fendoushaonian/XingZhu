#pragma once
#include <string>

namespace sf {

struct NavAction {
    enum Type {
        None = 0,
        LaunchGame,
        StopGame,       // 停止游戏
        InstallGame,
        UninstallGame,
        ValidateGame,
        ViewStorePage,
        ViewProperties,
        BrowseLocalFiles,
        ViewLibraryDownload,  // Switch to Library tab and show download panel
        ViewLibraryGame,      // Switch to Library tab and select a specific game
    };

    Type type = None;
    std::string appId;
    std::string gameName;  // Optional: game name for install dialog
    float gameSize = 0.0f; // Optional: game size in bytes
};

// Shared state accessor (header-only, no .cpp needed)
inline NavAction& PendingNavAction() {
    static NavAction s_action;
    return s_action;
}

inline void Navigate(NavAction::Type type, const std::string& appId,
                     const std::string& gameName = "", float gameSize = 0.0f) {
    PendingNavAction().type = type;
    PendingNavAction().appId = appId;
    PendingNavAction().gameName = gameName;
    PendingNavAction().gameSize = gameSize;
}

inline void Navigate(const NavAction& action) {
    PendingNavAction() = action;
}

inline NavAction ConsumeNavAction() {
    NavAction action = PendingNavAction();
    PendingNavAction().type = NavAction::None;
    PendingNavAction().appId.clear();
    PendingNavAction().gameName.clear();
    PendingNavAction().gameSize = 0.0f;
    return action;
}

} // namespace sf
