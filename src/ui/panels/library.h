#pragma once
#include "imgui.h"
#include "ui/widgets/game_card.h"
#include <vector>
#include <string>

namespace sf {

// Steam-style Library main content area
class LibraryContent {
public:
    LibraryContent();
    void Render(float x, float y, float width, float height, const std::vector<GameInfo>& games, const GameInfo* selectedGame);

private:
    void RenderNewsSection(ImDrawList* dl, float x, float y, float width, float& outHeight, const std::vector<GameInfo>& games);
    void RenderRecentGames(ImDrawList* dl, float x, float y, float width, float& outHeight, const std::vector<GameInfo>& games);
    void RenderAllGames(float x, float y, float width, float& outHeight, const std::vector<GameInfo>& games);
    void UpdateNewsFromGames(const std::vector<GameInfo>& games);
    void RenderGameDetail(ImDrawList* dl, float x, float y, float width, float height, const GameInfo* game);

    struct NewsItem {
        std::string title;
        std::string game;
        std::string gameAppId;
        std::string date;
        std::string thumbnailUrl;
        ImU32 coverColor;
    };
    std::vector<NewsItem> news_;
    int newsScroll_ = 0;
    bool newsRequested_ = false;
    std::vector<std::string> requestedAppIds_;  // 已请求数据的游戏

    // Game detail state
    std::string lastSelectedAppId_;
    bool detailTexturesRequested_ = false;

    // Media gallery state (screenshots + videos)
    int selectedMediaIdx_ = 0;      // 0-based index into combined media list
    bool isPlayingVideo_ = false;   // true if currently playing a video in-app
    int playingVideoIdx_ = -1;      // index of currently playing video
    float autoSlideTimer_ = 0.0f;   // Timer for auto-slideshow (only when no videos)
    float lastFrameTime_ = 0.0f;    // For delta time calculation

    // Achievements popup state
    bool showAchievementsPopup_ = false;
    std::string achievementsPopupAppId_;
    float achievementsScrollY_ = 0.0f;
};

// Store / marketplace page
class StorePage {
public:
    StorePage();
    void Render(float x, float y, float width, float height);
    void SelectGame(const std::string& appId);

private:
    void RenderFeaturedBanner(ImDrawList* dl, float x, float y, float width);
    void RenderSpecialOffers(ImDrawList* dl, float x, float y, float width);
    void RenderCategories(ImDrawList* dl, float x, float y, float width);
    void RenderNewReleases(ImDrawList* dl, float x, float y, float width);
    void RenderTrending(ImDrawList* dl, float x, float y, float width);
    void RenderComingSoon(ImDrawList* dl, float x, float y, float width);
    void RenderGameDetail(ImDrawList* dl, float x, float y, float width, float height);

    struct StoreGame {
        std::string name;
        std::string appId;
        std::string price;
        std::string originalPrice;
        std::string discount;
        std::string tags;
        std::string desc;
        std::string developer;
        std::string publisher;
        std::string releaseDate;
        int reviewPct = 0;          // positive review percentage (0 = unknown)
        std::string reviewLabel;    // e.g. 好评如潮
    };

    const StoreGame* FindGameByAppId(const std::string& appId) const;

    std::vector<StoreGame> featured_;
    std::vector<StoreGame> specials_;
    std::vector<StoreGame> newReleases_;
    std::vector<StoreGame> trending_;
    std::vector<StoreGame> comingSoon_;
    int featuredIdx_ = 0;
    float featuredTimer_ = 0;

    // Dynamic loading from Steam API
    bool storeListingsRequested_ = false;
    bool storeListingsLoaded_ = false;
    void UpdateFromSteamListings();

    // Detail page state
    std::string selectedAppId_;
    bool showDetail_ = false;
    bool playingVideo_ = false;
    int playingMovieIdx_ = -1;
    int selectedSSIdx_ = 0;
    bool detailTexturesRequested_ = false;

    // In-app popup state
    bool showNewsPopup_ = false;
    int newsPopupIdx_ = -1;
    bool showDlcPopup_ = false;
    bool showDlcDetailPopup_ = false;
    int dlcDetailIdx_ = -1;

    // Translation state for news popup
    bool newsTranslating_ = false;
    bool newsTranslated_ = false;
    std::string translatedTitle_;
    std::vector<std::string> translatedContent_;  // Translated rich content segments

    // News video playback state
    bool newsVideoPlaying_ = false;
    int newsVideoIdx_ = -1;  // index of video segment being played

    // Search state
    char searchBuf_[256] = {};
    char searchPrev_[256] = {};  // previous frame's text (for live search)
    float searchDebounce_ = 0;  // timer for debounce
    bool showSearchResults_ = false;
    std::string activeSearchQuery_;
    std::vector<StoreGame> searchResults_;
    bool searchResultsLoaded_ = false;
    int searchTotal_ = 0;

    void RenderSearchBar(ImDrawList* dl, float x, float y, float width);
    void RenderSearchResults(ImDrawList* dl, float x, float y, float width, float height);
    void DoSearch(const std::string& term);

    // Browse all games state
    bool browseRequested_ = false;
    std::vector<StoreGame> browseGames_;
    int browseLastAppId_ = 0;
    bool browseHasMore_ = true;
    int browseLoadedCount_ = 0; // track how many we've consumed from API

    // Category carousel state
    int categoryPage_ = 0;  // current page (0, 1, 2...)

    // Category detail view state
    bool showCategoryDetail_ = false;
    std::string categoryDetailLabel_;  // Display name (e.g., "动作")
    std::string categoryDetailTerm_;   // Search term (e.g., "action")
    std::vector<StoreGame> categoryDetailGames_;
    bool categoryDetailLoaded_ = false;
    float categoryDetailScrollY_ = 0;

    void RenderBrowseAll(ImDrawList* dl, float x, float& cy, float width);
    void RenderCategoryDetail(ImDrawList* dl, float x, float y, float width, float height);

    // 侧边栏相关
    void RenderSidebar(float x, float y, float width, float height);
    void RenderMainContent(float x, float y, float width, float height);

    // 商店分类
    enum class StoreCategory { FreePlay, Official, Offline, Pirated };
    StoreCategory currentCategory_ = StoreCategory::Official;

    // 统计数据
    int onlineUsers_ = 0;
    int totalAccounts_ = 0;
    int idleAccounts_ = 0;
    float statsUpdateTimer_ = 0.0f;

    // 畅玩游戏页面状态
    int freeplayCarouselIdx_ = 0;
    float freeplayCarouselTimer_ = 0.0f;
    char freeplaySearchBuf_[128] = {};
};

// Community page placeholder
class CommunityPage {
public:
    void Render(float x, float y, float width, float height);
};

} // namespace sf
