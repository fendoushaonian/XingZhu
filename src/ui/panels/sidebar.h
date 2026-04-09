#pragma once
#include "imgui.h"
#include "ui/widgets/game_card.h"
#include <vector>
#include <string>

namespace sf {

// Steam-style left sidebar: game list with search and filter
class GameListSidebar {
public:
    GameListSidebar();
    void Render(float x, float y, float width, float height);

    int GetSelectedGameIndex() const { return selectedGame_; }
    const GameInfo* GetSelectedGame() const;
    const std::vector<GameInfo>& GetGames() const { return games_; }

    // Reload games from Steam library API (call after binding Steam ID)
    void RefreshFromSteamLibrary();

    // 强制刷新游戏列表（用户库变化时调用）
    void ForceRefresh();

    // 添加游戏到列表（用于免费游戏入库）
    void AddGameToList(const std::string& appId, const std::string& name);

    // 更新单个游戏的安装状态
    void UpdateGameInstallStatus(const std::string& appId, bool installed);

    // 选中指定 appId 的游戏
    void SelectGameByAppId(const std::string& appId);

    // Clear all data (call on logout)
    void Clear();

private:
    std::vector<GameInfo> games_;
    char searchBuf_[128] = {};
    int selectedGame_ = -1;
    int contextMenuGame_ = -1;
    int filterMode_ = 0;  // 0=all, 1=installed, 2=recent
    float hoverAnim_[64] = {};
    bool steamLibRequested_ = false;
    std::string loadedSteamId_;
};

} // namespace sf
