#pragma once
#include "imgui.h"
#include "ui/widgets/game_card.h"
#include "ui/panels/checkin.h"
#include <vector>

namespace sf {

struct StoreItem {
    GameInfo info;
    std::string price;
    std::string discount;
    bool isFree;
};

// 商店分类
enum class StoreCategory {
    Official,    // 正版游戏
    Offline,     // 离线游戏
    Cracked,     // 盗版破解
    FreePlay     // 畅玩
};

class StorePanel {
public:
    StorePanel();
    void Render(float x, float y, float width, float height);

    // 签到面板
    CheckInPanel& GetCheckInPanel() { return checkInPanel_; }

private:
    void RenderSidebar(float x, float y, float width, float height);
    void RenderContent(float x, float y, float width, float height);
    void RenderFeatured(float width);
    void RenderSpecials(float width);
    void RenderNewReleases(float width);

    std::vector<StoreItem> featured_;
    std::vector<StoreItem> specials_;
    std::vector<StoreItem> newReleases_;

    // 侧边栏状态
    StoreCategory currentCategory_ = StoreCategory::Official;
    int onlineUsers_ = 0;
    int totalAccounts_ = 0;
    int idleAccounts_ = 0;
    float statsUpdateTimer_ = 0.0f;

    // 签到面板
    CheckInPanel checkInPanel_;
};

class FreeGamesPanel {
public:
    FreeGamesPanel();
    void Render(float x, float y, float width, float height);

private:
    struct FreeGame {
        std::string name;
        std::string appId;
        std::string type;  // "game", "dlc", "software"
        std::string source; // "Steam", "Reddit", "PICS"
        bool claimed;
    };
    std::vector<FreeGame> freeGames_;
};

} // namespace sf
