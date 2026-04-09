#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

namespace sf {

using json = nlohmann::json;

struct SteamPlayerSummary {
    std::string steamId;
    std::string personaName;
    std::string avatarUrl;
    int personaState;  // 0=offline, 1=online, 2=busy, 3=away, 4=snooze, 5=trade, 6=play
    std::string gameExtraInfo; // currently playing game name
};

struct SteamOwnedGame {
    int appId;
    std::string name;
    int playtimeForever;  // minutes
    std::string imgIconUrl;
    bool hasCommunityStats;
};

struct SteamAchievement {
    std::string apiName;
    std::string displayName;
    std::string description;
    bool achieved;
    int unlockTime;
};

struct SteamFreePackage {
    int appId;
    std::string name;
    std::string type; // "game", "dlc", "software"
};

class SteamAPI {
public:
    SteamAPI();
    ~SteamAPI();

    void SetApiKey(const std::string& key) { apiKey_ = key; }
    void SetSteamId(const std::string& id) { steamId_ = id; }

    bool IsConfigured() const { return !apiKey_.empty() && !steamId_.empty(); }

    // Player info
    bool GetPlayerSummary(SteamPlayerSummary& out);

    // Game library
    bool GetOwnedGames(std::vector<SteamOwnedGame>& out);

    // Achievements
    bool GetPlayerAchievements(int appId, std::vector<SteamAchievement>& out);

    // Free games (via Steam store API — no key needed)
    bool GetFreeGames(std::vector<SteamFreePackage>& out);

    // Store featured
    bool GetStoreFeatured(json& out);

    // Error info
    const std::string& GetLastError() const { return lastError_; }

private:
    std::string apiKey_;
    std::string steamId_;
    std::string lastError_;

    // Internal HTTP GET
    bool HttpGet(const std::string& url, std::string& response);
};

} // namespace sf
