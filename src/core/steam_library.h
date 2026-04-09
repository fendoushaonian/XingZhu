#pragma once
#include <string>
#include <vector>

namespace sf {

struct SteamOwnedGame {
    std::string appId;
    std::string name;
    std::string iconHash;       // icon hash for CDN URL
    int playtimeForever = 0;    // total playtime in minutes
    int playtimeRecent = 0;     // last 2 weeks playtime in minutes
};

struct SteamLibraryData {
    bool loaded = false;
    bool loading = false;
    bool failed = false;
    bool rawReady = false;      // raw JSON ready for main-thread parsing
    std::string error;
    std::string steamId;        // the steamId used for this fetch
    std::string rawJson;        // raw API response (parsed on main thread)
    std::vector<SteamOwnedGame> games;
};

// Request to bind a Steam ID (resolves vanity URL if needed) and fetch game library.
// Everything runs async in a background thread. Input can be a 64-bit Steam ID or vanity name.
void RequestSteamBind(const std::string& input);

// Fetch owned games for a known numeric Steam ID (async, non-blocking).
void RequestSteamLibrary(const std::string& steamId);

// Get a COPY of current library data (thread-safe).
// Returns false if no data available yet.
bool GetSteamLibraryCopy(SteamLibraryData& out);

// Check if currently loading
bool IsSteamLibraryLoading();

// Get the resolved Steam ID after a successful bind (empty if not resolved yet)
std::string GetResolvedSteamId();

// Call from main thread each frame — parses raw JSON if available
void PollSteamLibrary();

// Get bind status message (for UI feedback)
std::string GetBindStatusMessage();

// Clear all library data (call on logout or account switch)
void ClearSteamLibrary();

} // namespace sf
