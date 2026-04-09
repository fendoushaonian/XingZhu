#include "core/steam_library.h"
#include "utils/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace sf {

// ---- WinHTTP GET ----
static bool WinHttpGet(const std::string& url, std::string& response) {
    std::string u = url;
    bool isHttps = true;

    if (u.find("https://") == 0) u = u.substr(8);
    else if (u.find("http://") == 0) { u = u.substr(7); isHttps = false; }

    size_t slashPos = u.find('/');
    std::string host, path;
    if (slashPos == std::string::npos) { host = u; path = "/"; }
    else { host = u.substr(0, slashPos); path = u.substr(slashPos); }

    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hSession = WinHttpOpen(L"SteamForge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 15000);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    DWORD decompFlags = WINHTTP_DECOMPRESSION_FLAG_ALL;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decompFlags, sizeof(decompFlags));

    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    ok = WinHttpReceiveResponse(hRequest, NULL);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    response.clear();
    DWORD dwSize = 0, dwDownloaded = 0;
    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;
        char* buf = (char*)malloc(dwSize + 1);
        if (!buf) break;
        memset(buf, 0, dwSize + 1);
        WinHttpReadData(hRequest, buf, dwSize, &dwDownloaded);
        response.append(buf, dwDownloaded);
        free(buf);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// ---- Pure C JSON parser ----
struct RawGameEntry {
    int appId;
    char name[256];
    char iconHash[128];
    int playtimeForever;
    int playtimeRecent;
};

static int ExtractJsonString(const char* start, int regionLen, const char* key, char* out, int outMax) {
    char needle[64];
    int nLen = snprintf(needle, sizeof(needle), "\"%s\":", key);
    if (nLen <= 0 || nLen >= (int)sizeof(needle)) { out[0] = '\0'; return 0; }

    const char* found = NULL;
    for (const char* p = start; p < start + regionLen - nLen; p++) {
        if (memcmp(p, needle, nLen) == 0) { found = p; break; }
    }
    if (!found) { out[0] = '\0'; return 0; }

    const char* p = found + nLen;
    const char* end = start + regionLen;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (p >= end || *p != '"') { out[0] = '\0'; return 0; }
    p++;

    int i = 0;
    while (p < end && i < outMax - 1) {
        if (*p == '\\' && p + 1 < end) {
            // Handle escape sequences
            p++;
            char escaped = *p;
            switch (escaped) {
                case '"': out[i++] = '"'; break;
                case '\\': out[i++] = '\\'; break;
                case '/': out[i++] = '/'; break;
                case 'n': out[i++] = '\n'; break;
                case 'r': out[i++] = '\r'; break;
                case 't': out[i++] = '\t'; break;
                case 'u':
                    // Skip unicode escape \uXXXX (just copy as-is for now)
                    out[i++] = '\\';
                    out[i++] = 'u';
                    break;
                default: out[i++] = escaped; break;
            }
            p++;
        } else if (*p == '"') {
            // End of string
            break;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = '\0';
    return i;
}

static int ExtractJsonInt(const char* start, int regionLen, const char* key) {
    char needle[64];
    int nLen = snprintf(needle, sizeof(needle), "\"%s\":", key);
    if (nLen <= 0 || nLen >= (int)sizeof(needle)) return 0;

    const char* found = NULL;
    for (const char* p = start; p < start + regionLen - nLen; p++) {
        if (memcmp(p, needle, nLen) == 0) { found = p; break; }
    }
    if (!found) return 0;

    const char* p = found + nLen;
    const char* end = start + regionLen;
    while (p < end && (*p == ' ' || *p == '\t')) p++;

    int val = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    return val;
}

static int ParseGamesC(const char* jsonData, int jsonLen, RawGameEntry* outGames, int maxGames) {
    int count = 0;

    // First, try to find game_count to know expected number
    int expectedCount = ExtractJsonInt(jsonData, jsonLen, "game_count");
    Log(LogLevel::Info, "ParseGamesC: expected game_count = %d", expectedCount);

    const char* gamesKey = NULL;
    for (const char* p = jsonData; p < jsonData + jsonLen - 8; p++) {
        if (memcmp(p, "\"games\":", 8) == 0) { gamesKey = p; break; }
    }
    if (!gamesKey) {
        Log(LogLevel::Error, "ParseGamesC: 'games' key not found in JSON");
        return 0;
    }

    const char* arrStart = NULL;
    for (const char* p = gamesKey + 8; p < jsonData + jsonLen; p++) {
        if (*p == '[') { arrStart = p; break; }
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') break;
    }
    if (!arrStart) {
        Log(LogLevel::Error, "ParseGamesC: games array '[' not found");
        return 0;
    }

    Log(LogLevel::Debug, "ParseGamesC: found games array at offset %d", (int)(arrStart - jsonData));

    const char* p = arrStart + 1;
    const char* dataEnd = jsonData + jsonLen;

    while (p < dataEnd && count < maxGames) {
        while (p < dataEnd && *p != '{' && *p != ']') p++;
        if (p >= dataEnd || *p == ']') break;

        const char* objStart = p;
        int depth = 1;
        p++;
        bool inString = false;
        while (p < dataEnd && depth > 0) {
            if (*p == '\\' && inString && p + 1 < dataEnd) {
                // Skip escaped character
                p += 2;
                continue;
            }
            if (*p == '"') {
                inString = !inString;
            } else if (!inString) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
            }
            p++;
        }
        if (depth != 0) {
            Log(LogLevel::Warn, "ParseGamesC: unbalanced braces at offset %d, depth=%d", (int)(p - jsonData), depth);
            break;
        }

        const char* objEnd = p;
        int objLen = (int)(objEnd - objStart);

        int appid = ExtractJsonInt(objStart, objLen, "appid");
        if (appid > 0 && count < maxGames) {
            RawGameEntry* g = &outGames[count];
            g->appId = appid;
            ExtractJsonString(objStart, objLen, "name", g->name, sizeof(g->name));
            ExtractJsonString(objStart, objLen, "img_icon_url", g->iconHash, sizeof(g->iconHash));
            g->playtimeForever = ExtractJsonInt(objStart, objLen, "playtime_forever");
            g->playtimeRecent = ExtractJsonInt(objStart, objLen, "playtime_2weeks");
            count++;
            if (count <= 5 || count % 10 == 0) {
                Log(LogLevel::Debug, "ParseGamesC: game %d: appid=%d name='%s'", count, appid, g->name);
            }
        } else if (appid <= 0) {
            Log(LogLevel::Warn, "ParseGamesC: skipped object with invalid appid at offset %d", (int)(objStart - jsonData));
        }
    }

    Log(LogLevel::Info, "ParseGamesC: total parsed = %d (expected %d)", count, expectedCount);
    return count;
}

// ---- State ----
static const char* API_KEY = "281B554C78FFC10E43A506B29CB19FEE";

// Raw data from background thread (C-style, no C++ objects)
static char* g_rawJsonData = nullptr;
static int g_rawJsonLen = 0;
static char g_rawSteamId[32] = {};
static std::atomic<bool> g_rawDataReady{false};
static std::atomic<bool> g_rawDataFailed{false};

// Parsed data (only accessed from main thread)
static SteamLibraryData g_libData;
static std::string g_resolvedSteamId;
static std::string g_bindStatusMsg;

// Thread control
static std::mutex g_rawMutex;  // Only protects raw data handoff
static std::atomic<bool> g_fetchRunning{false};
static std::atomic<bool> g_abortFetch{false};

// ---- Background fetch thread (only downloads, no C++ object creation) ----
static void FetchThreadFunc(const char* steamId) {
    Log(LogLevel::Info, "FetchThread: starting for steamId=%s", steamId);

    if (g_abortFetch) {
        g_fetchRunning = false;
        Log(LogLevel::Info, "FetchThread: aborted before start");
        return;
    }

    // Build URL using C-style
    // include_played_free_games=1: include free games that have been played
    // skip_unvetted_apps=0: include all apps (not just vetted ones)
    char url[512];
    snprintf(url, sizeof(url),
        "https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/"
        "?key=%s&steamid=%s&include_appinfo=1&include_played_free_games=1&skip_unvetted_apps=0&format=json",
        API_KEY, steamId);

    Log(LogLevel::Info, "FetchThread: requesting URL (steamId=%s)", steamId);

    // Download using std::string (temporary, will be copied to C buffer)
    std::string response;
    bool httpOk = WinHttpGet(url, response);

    if (g_abortFetch) {
        g_fetchRunning = false;
        Log(LogLevel::Info, "FetchThread: aborted after HTTP");
        return;
    }

    Log(LogLevel::Info, "FetchThread: HTTP %s, %d bytes", httpOk ? "OK" : "FAILED", (int)response.size());

    if (!httpOk || response.empty()) {
        g_rawDataFailed = true;
        g_fetchRunning = false;
        return;
    }

    // Copy to C buffer for main thread
    int len = (int)response.size();
    char* buf = (char*)malloc(len + 1);
    if (!buf) {
        Log(LogLevel::Error, "FetchThread: malloc failed");
        g_rawDataFailed = true;
        g_fetchRunning = false;
        return;
    }
    memcpy(buf, response.c_str(), len);
    buf[len] = '\0';

    // Clear std::string immediately
    response.clear();
    response.shrink_to_fit();

    if (g_abortFetch) {
        free(buf);
        g_fetchRunning = false;
        Log(LogLevel::Info, "FetchThread: aborted before handoff");
        return;
    }

    // Hand off to main thread
    {
        std::lock_guard<std::mutex> lk(g_rawMutex);
        if (g_rawJsonData) free(g_rawJsonData);
        g_rawJsonData = buf;
        g_rawJsonLen = len;
        strncpy(g_rawSteamId, steamId, sizeof(g_rawSteamId) - 1);
        g_rawSteamId[sizeof(g_rawSteamId) - 1] = '\0';
    }

    g_rawDataReady = true;
    g_fetchRunning = false;
    Log(LogLevel::Info, "FetchThread: raw data ready for main thread");
}

// ---- Public API ----

void RequestSteamBind(const std::string& input) {
    if (input.empty()) return;
    if (g_fetchRunning) return;

    Log(LogLevel::Info, "RequestSteamBind: '%s'", input.c_str());

    g_abortFetch = false;
    g_rawDataReady = false;
    g_rawDataFailed = false;

    g_libData.loaded = false;
    g_libData.loading = true;
    g_libData.failed = false;
    g_libData.games.clear();
    g_libData.steamId.clear();

    g_resolvedSteamId.clear();
    g_bindStatusMsg = u8"\u6B63\u5728\u8FDE\u63A5 Steam...";

    // Copy steamId to static buffer for thread
    static char s_steamIdBuf[64];
    strncpy(s_steamIdBuf, input.c_str(), sizeof(s_steamIdBuf) - 1);
    s_steamIdBuf[sizeof(s_steamIdBuf) - 1] = '\0';

    g_fetchRunning = true;
    std::thread t(FetchThreadFunc, s_steamIdBuf);
    t.detach();
}

void RequestSteamLibrary(const std::string& steamId) { RequestSteamBind(steamId); }

void PollSteamLibrary() {
    // Check for failed fetch
    if (g_rawDataFailed.exchange(false)) {
        g_libData.failed = true;
        g_libData.loading = false;
        g_libData.error = "HTTP request failed";
        return;
    }

    // Check for raw data ready
    if (!g_rawDataReady.exchange(false)) return;

    Log(LogLevel::Info, "PollSteamLibrary: processing raw data on main thread");

    // Get raw data (protected by mutex)
    char* jsonBuf = nullptr;
    int jsonLen = 0;
    char steamId[32] = {};
    {
        std::lock_guard<std::mutex> lk(g_rawMutex);
        jsonBuf = g_rawJsonData;
        jsonLen = g_rawJsonLen;
        strncpy(steamId, g_rawSteamId, sizeof(steamId) - 1);
        g_rawJsonData = nullptr;
        g_rawJsonLen = 0;
    }

    if (!jsonBuf || jsonLen == 0) {
        g_libData.failed = true;
        g_libData.loading = false;
        return;
    }

    // Parse on main thread using C parser
    // Use heap allocation to avoid stack overflow (RawGameEntry is ~400 bytes each)
    RawGameEntry* rawGames = (RawGameEntry*)malloc(sizeof(RawGameEntry) * 1000);
    if (!rawGames) {
        Log(LogLevel::Error, "PollSteamLibrary: failed to allocate rawGames buffer");
        free(jsonBuf);
        g_libData.failed = true;
        g_libData.loading = false;
        return;
    }
    memset(rawGames, 0, sizeof(RawGameEntry) * 1000);
    int gameCount = ParseGamesC(jsonBuf, jsonLen, rawGames, 1000);

    Log(LogLevel::Info, "PollSteamLibrary: parsed %d games", gameCount);

    // Free raw buffer
    free(jsonBuf);

    // Create C++ objects on main thread (safe!)
    g_libData.games.clear();
    g_libData.games.reserve(gameCount);

    for (int i = 0; i < gameCount; i++) {
        SteamOwnedGame game;
        game.appId = std::to_string(rawGames[i].appId);
        game.name = rawGames[i].name;
        game.iconHash = rawGames[i].iconHash;
        game.playtimeForever = rawGames[i].playtimeForever;
        game.playtimeRecent = rawGames[i].playtimeRecent;
        g_libData.games.push_back(std::move(game));
    }

    // Free rawGames buffer after use
    free(rawGames);
    rawGames = nullptr;

    int totalCount = (int)g_libData.games.size();
    Log(LogLevel::Info, "PollSteamLibrary: loaded %d games", totalCount);

    // Sort by name
    std::sort(g_libData.games.begin(), g_libData.games.end(),
        [](const SteamOwnedGame& a, const SteamOwnedGame& b) {
            return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
        });

    g_libData.steamId = steamId;
    g_libData.loaded = true;
    g_libData.loading = false;
    g_libData.failed = false;

    g_resolvedSteamId = steamId;

    char msg[128];
    snprintf(msg, sizeof(msg), u8"\u7ED1\u5B9A\u6210\u529F\uFF0C\u5171 %d \u6B3E\u6E38\u620F", totalCount);
    g_bindStatusMsg = msg;

    Log(LogLevel::Info, "PollSteamLibrary: done, %d games loaded", totalCount);
}

bool GetSteamLibraryCopy(SteamLibraryData& out) {
    // No mutex needed - only accessed from main thread now
    if (!g_libData.loaded && !g_libData.loading && !g_libData.failed) return false;
    out = g_libData;
    return true;
}

bool IsSteamLibraryLoading() { return g_fetchRunning || g_libData.loading; }

std::string GetResolvedSteamId() {
    return g_resolvedSteamId;
}

std::string GetBindStatusMessage() {
    return g_bindStatusMsg;
}

void ClearSteamLibrary() {
    Log(LogLevel::Info, "ClearSteamLibrary: starting...");

    // Signal abort first
    g_abortFetch = true;

    // Wait for fetch to complete (shorter timeout, non-blocking approach)
    int waitCount = 0;
    while (g_fetchRunning.load() && waitCount < 50) {  // Max 500ms
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waitCount++;
    }

    if (g_fetchRunning.load()) {
        Log(LogLevel::Warn, "ClearSteamLibrary: fetch thread still running after timeout");
    }

    Log(LogLevel::Info, "ClearSteamLibrary: clearing raw data...");

    // Clear raw data
    {
        std::lock_guard<std::mutex> lk(g_rawMutex);
        if (g_rawJsonData) {
            free(g_rawJsonData);
            g_rawJsonData = nullptr;
        }
        g_rawJsonLen = 0;
        g_rawSteamId[0] = '\0';
    }
    g_rawDataReady = false;
    g_rawDataFailed = false;

    Log(LogLevel::Info, "ClearSteamLibrary: clearing parsed data...");

    // Clear parsed data - do this carefully with logging
    Log(LogLevel::Info, "ClearSteamLibrary: setting flags...");
    g_libData.loaded = false;
    g_libData.loading = false;
    g_libData.failed = false;
    g_libData.rawReady = false;

    Log(LogLevel::Info, "ClearSteamLibrary: clearing error string...");
    g_libData.error.clear();

    Log(LogLevel::Info, "ClearSteamLibrary: clearing steamId string...");
    g_libData.steamId.clear();

    Log(LogLevel::Info, "ClearSteamLibrary: clearing rawJson string...");
    g_libData.rawJson.clear();

    Log(LogLevel::Info, "ClearSteamLibrary: clearing games vector (%d games)...", (int)g_libData.games.size());
    g_libData.games.clear();

    Log(LogLevel::Info, "ClearSteamLibrary: clearing resolvedSteamId...");
    g_resolvedSteamId.clear();

    Log(LogLevel::Info, "ClearSteamLibrary: clearing bindStatusMsg...");
    g_bindStatusMsg.clear();

    g_abortFetch = false;
    Log(LogLevel::Info, "ClearSteamLibrary: done");
}

} // namespace sf
