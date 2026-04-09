#include "core/steam_api.h"
#include "utils/logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace sf {

SteamAPI::SteamAPI() {}
SteamAPI::~SteamAPI() {}

bool SteamAPI::HttpGet(const std::string& url, std::string& response) {
    // Parse URL — expects https://host/path?query
    // Simple parser for Steam API URLs
    std::string host, path;
    bool isHttps = true;

    std::string u = url;
    if (u.find("https://") == 0) {
        u = u.substr(8);
    } else if (u.find("http://") == 0) {
        u = u.substr(7);
        isHttps = false;
    }

    size_t slashPos = u.find('/');
    if (slashPos == std::string::npos) {
        host = u;
        path = "/";
    } else {
        host = u.substr(0, slashPos);
        path = u.substr(slashPos);
    }

    // Convert to wide strings
    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(
        L"SteamForge/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        lastError_ = "WinHttpOpen failed";
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        wHost.c_str(),
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
        0
    );

    if (!hConnect) {
        lastError_ = "WinHttpConnect failed";
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        wPath.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isHttps ? WINHTTP_FLAG_SECURE : 0
    );

    if (!hRequest) {
        lastError_ = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    BOOL bResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (!bResult) {
        lastError_ = "WinHttpSendRequest failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bResult = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResult) {
        lastError_ = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read response body
    response.clear();
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;

    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;

        std::vector<char> buf(dwSize + 1, 0);
        WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded);
        response.append(buf.data(), dwDownloaded);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return true;
}

bool SteamAPI::GetPlayerSummary(SteamPlayerSummary& out) {
    if (!IsConfigured()) {
        lastError_ = "API key or Steam ID not configured";
        return false;
    }

    std::string url = "https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v0002/"
                      "?key=" + apiKey_ + "&steamids=" + steamId_;

    std::string resp;
    if (!HttpGet(url, resp)) return false;

    try {
        auto j = json::parse(resp);
        auto& players = j["response"]["players"];
        if (players.empty()) {
            lastError_ = "Player not found";
            return false;
        }
        auto& p = players[0];
        out.steamId     = p.value("steamid", "");
        out.personaName = p.value("personaname", "");
        out.avatarUrl   = p.value("avatarfull", "");
        out.personaState = p.value("personastate", 0);
        out.gameExtraInfo = p.value("gameextrainfo", "");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool SteamAPI::GetOwnedGames(std::vector<SteamOwnedGame>& out) {
    if (!IsConfigured()) {
        lastError_ = "API key or Steam ID not configured";
        return false;
    }

    std::string url = "https://api.steampowered.com/IPlayerService/GetOwnedGames/v0001/"
                      "?key=" + apiKey_ + "&steamid=" + steamId_ +
                      "&format=json&include_appinfo=1&include_played_free_games=1";

    std::string resp;
    if (!HttpGet(url, resp)) return false;

    try {
        auto j = json::parse(resp);
        auto& games = j["response"]["games"];
        out.clear();
        for (auto& g : games) {
            SteamOwnedGame game;
            game.appId            = g.value("appid", 0);
            game.name             = g.value("name", "");
            game.playtimeForever  = g.value("playtime_forever", 0);
            game.imgIconUrl       = g.value("img_icon_url", "");
            game.hasCommunityStats = g.value("has_community_visible_stats", false);
            out.push_back(game);
        }
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool SteamAPI::GetPlayerAchievements(int appId, std::vector<SteamAchievement>& out) {
    if (!IsConfigured()) {
        lastError_ = "API key or Steam ID not configured";
        return false;
    }

    std::string url = "https://api.steampowered.com/ISteamUserStats/GetPlayerAchievements/v0001/"
                      "?appid=" + std::to_string(appId) +
                      "&key=" + apiKey_ + "&steamid=" + steamId_;

    std::string resp;
    if (!HttpGet(url, resp)) return false;

    try {
        auto j = json::parse(resp);
        auto& stats = j["playerstats"];
        if (!stats.contains("achievements")) {
            lastError_ = "No achievements for this game";
            return false;
        }
        out.clear();
        for (auto& a : stats["achievements"]) {
            SteamAchievement ach;
            ach.apiName     = a.value("apiname", "");
            ach.displayName = a.value("name", ach.apiName);
            ach.description = a.value("description", "");
            ach.achieved    = a.value("achieved", 0) != 0;
            ach.unlockTime  = a.value("unlocktime", 0);
            out.push_back(ach);
        }
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool SteamAPI::GetFreeGames(std::vector<SteamFreePackage>& out) {
    // Use Steam store API (no key needed)
    std::string url = "https://store.steampowered.com/api/featuredcategories/?cc=us&l=english";

    std::string resp;
    if (!HttpGet(url, resp)) return false;

    try {
        auto j = json::parse(resp);

        // Check "0" category which is specials/free
        out.clear();
        if (j.contains("specials") && j["specials"].contains("items")) {
            for (auto& item : j["specials"]["items"]) {
                if (item.value("final_price", 1) == 0) {
                    SteamFreePackage pkg;
                    pkg.appId = item.value("id", 0);
                    pkg.name  = item.value("name", "");
                    pkg.type  = "game";
                    out.push_back(pkg);
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool SteamAPI::GetStoreFeatured(json& out) {
    std::string url = "https://store.steampowered.com/api/featured/?cc=us&l=english";

    std::string resp;
    if (!HttpGet(url, resp)) return false;

    try {
        out = json::parse(resp);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

} // namespace sf
