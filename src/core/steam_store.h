#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>

namespace sf {

struct SteamStoreData {
    bool loaded = false;
    bool failed = false;

    std::string name;
    std::string shortDesc;          // plain text
    std::string aboutGame;          // HTML stripped
    std::string developer;
    std::string publisher;
    std::string releaseDate;
    std::string headerImage;

    // Screenshots
    std::vector<std::string> screenshotUrls;    // full size URLs
    std::vector<std::string> screenshotThumbs;  // thumbnail URLs

    // Movies/Trailers
    struct Movie {
        std::string name;
        std::string thumbnailUrl;
        std::string mp4Url;        // direct mp4 (legacy) or empty
        std::string hlsUrl;        // HLS .m3u8 stream URL
        std::string dashUrl;       // DASH .mpd stream URL
        bool highlight = false;
    };
    std::vector<Movie> movies;

    // Classification
    std::vector<std::string> genres;
    std::vector<std::string> categories;  // e.g. Single-player, Steam Achievements

    // Reviews
    int totalReviews = 0;
    int totalPositive = 0;
    int reviewScore = 0;            // 0-100
    std::string reviewDesc;         // e.g. "好评如潮"

    // Price
    std::string priceFormatted;
    int discountPercent = 0;
    std::string originalPrice;
    bool isFree = false;            // true if game is free to play

    // System requirements (plain text, stripped HTML)
    std::string reqMinimum;
    std::string reqRecommended;

    // Supported languages
    std::string languages;

    // Platforms
    bool platformWindows = false;
    bool platformMac = false;
    bool platformLinux = false;

    // Additional info
    std::string website;        // official website URL
    int dlcCount = 0;
    std::string type;           // "game", "dlc", "demo", etc.

    // Region availability
    bool regionRestricted = false;  // true if not available in user's region (China)

    // Release status
    bool comingSoon = false;        // true if game has not been released yet

    // Disk space required (in bytes, from pc_requirements or estimated)
    long long diskSpaceBytes = 0;

    // DLC items
    struct DlcItem {
        std::string appId;
        std::string name;
        std::string headerImage;
        std::string price;
        std::string originalPrice;
        std::string shortDesc;
        std::string aboutGame;      // detailed description (stripped)
        int discountPercent = 0;
        bool loaded = false;
        bool owned = false;         // whether user owns this DLC
    };
    std::vector<DlcItem> dlcItems;

    // Rich content segment for rendering mixed text/image content
    struct RichSegment {
        enum Type { TEXT, IMAGE, HEADING, BULLET, VIDEO };
        Type type = TEXT;
        std::string text;       // text content, image URL, or video URL
        std::string videoUrl2;  // secondary video URL (e.g., mp4 fallback for webm)
        int headingLevel = 0;   // 1-4 for headings
    };

    // News / announcements (from ISteamNews API)
    struct NewsItem {
        std::string title;
        std::string contents;       // plain text fallback
        std::vector<RichSegment> richContent;  // parsed rich segments
        std::string thumbnailUrl;   // first image from content (for card display)
        std::string url;
        std::string feedLabel;      // e.g. "Community Announcements"
        long long date = 0;         // unix timestamp
    };
    std::vector<NewsItem> news;

    // Rich content for "About this game"
    std::vector<RichSegment> aboutRich;

    // ═══════════════════════════════════════════════════════════════
    //  ACHIEVEMENTS (from ISteamUserStats API)
    // ═══════════════════════════════════════════════════════════════
    struct Achievement {
        std::string apiName;        // internal name
        std::string displayName;    // localized display name
        std::string description;    // localized description
        std::string iconUrl;        // unlocked icon URL
        std::string iconGrayUrl;    // locked icon URL
        float globalPercent = 0.0f; // % of players who unlocked
        bool hidden = false;        // hidden achievement
    };
    std::vector<Achievement> achievements;
    int totalAchievements = 0;

    // ═══════════════════════════════════════════════════════════════
    //  COMMUNITY CONTENT (from Steam Community API)
    // ═══════════════════════════════════════════════════════════════
    struct CommunityItem {
        enum Type { SCREENSHOT, ARTWORK, VIDEO, GUIDE, WORKSHOP };
        Type type = SCREENSHOT;
        std::string id;
        std::string title;
        std::string previewUrl;     // thumbnail URL
        std::string authorName;
        std::string authorAvatarUrl;
        int upvotes = 0;
        int comments = 0;
        long long timestamp = 0;
    };
    std::vector<CommunityItem> communityItems;
    int communityScreenshots = 0;
    int communityArtwork = 0;
    int communityGuides = 0;
    int workshopItems = 0;

    // ═══════════════════════════════════════════════════════════════
    //  LIVE STREAMS (from Steam Broadcast API)
    // ═══════════════════════════════════════════════════════════════
    struct LiveStream {
        std::string streamId;
        std::string title;
        std::string streamerName;
        std::string streamerAvatarUrl;
        std::string thumbnailUrl;
        std::string watchUrl;
        int viewerCount = 0;
        bool isLive = true;
    };
    std::vector<LiveStream> liveStreams;
    int totalLiveStreams = 0;

    // ═══════════════════════════════════════════════════════════════
    //  PLAYER STATS (from ISteamUserStats API)
    // ═══════════════════════════════════════════════════════════════
    int currentPlayers = 0;         // current online players
    int peakPlayers24h = 0;         // 24h peak
    int allTimePeak = 0;            // all-time peak
};

// A game entry from the store listing APIs (featured, specials, new releases, etc.)
struct SteamListingGame {
    std::string name;
    std::string appId;
    std::string price;          // formatted final price, e.g. "¥268"
    std::string originalPrice;  // formatted original price before discount
    std::string discount;       // e.g. "-50%"
    std::string headerImage;    // URL to header image
    std::string tags;           // comma-separated genres/tags
    int discountPercent = 0;
};

// Container for all store listing categories
struct SteamStoreListings {
    bool loaded = false;
    bool failed = false;
    std::vector<SteamListingGame> featured;      // large capsules
    std::vector<SteamListingGame> specials;       // on sale
    std::vector<SteamListingGame> topSellers;     // top sellers
    std::vector<SteamListingGame> newReleases;    // new releases
    std::vector<SteamListingGame> comingSoon;     // coming soon
};

// Initialize / shutdown the background fetch system
void InitStoreData();
void ShutdownStoreData();

// Set Steam Web API key (enables richer data)
void SetStoreApiKey(const std::string& key);

// Request game details (async, non-blocking)
void RequestStoreData(const std::string& appId);

// Get cached data (returns nullptr if not yet loaded)
const SteamStoreData* GetStoreData(const std::string& appId);

// Check if data is being fetched
bool IsStoreDataLoading(const std::string& appId);

// Store listings (featured, specials, new releases, etc.)
void RequestStoreListings();
const SteamStoreListings* GetStoreListings();
bool IsStoreListingsLoading();

// Search results container
struct SteamSearchResults {
    bool loaded = false;
    bool failed = false;
    int total = 0;
    std::string query;          // the search term used
    std::vector<SteamListingGame> items;
};

// Search API (async, non-blocking)
void RequestStoreSearch(const std::string& term, int start = 0, int count = 50);
const SteamSearchResults* GetSearchResults();
bool IsSearchLoading();
void ClearSearchResults();

// Browse all games (paginated via IStoreService)
struct SteamBrowsePage {
    bool loaded = false;
    bool loading = false;
    bool hasMore = false;
    int lastAppId = 0;                          // cursor for next page
    std::vector<SteamListingGame> games;        // accumulated results
};

void RequestBrowseGames(int lastAppId = 0, int count = 100);
const SteamBrowsePage* GetBrowsePage();
bool IsBrowseLoading();

// ═══════════════════════════════════════════════════════════════
//  TRANSLATION API (Google Translate)
// ═══════════════════════════════════════════════════════════════
struct TranslationResult {
    bool loaded = false;
    bool failed = false;
    std::vector<std::string> translatedTexts;  // translated segments
};

// Request translation of multiple text segments to Chinese
void RequestTranslation(const std::vector<std::string>& texts, const std::string& requestId);
const TranslationResult* GetTranslationResult(const std::string& requestId);
bool IsTranslationLoading(const std::string& requestId);
void ClearTranslation(const std::string& requestId);

} // namespace sf
