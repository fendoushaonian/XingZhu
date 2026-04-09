#include "core/auth.h"
#include "core/database.h"
#include "core/steam_library.h"
#include "utils/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <winhttp.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

#include <mysql.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <map>

namespace sf {

static bool g_loggedIn = false;
static UserInfo g_currentUser;

static std::string GenerateSalt() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::ostringstream ss;
    for (int i = 0; i < 16; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    return ss.str();
}

static std::string Sha256(const std::string& input) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    UCHAR hash[32];
    DWORD hashLen = 0, resultLen = 0;

    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &resultLen, 0);
    BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hHash, (PUCHAR)input.data(), (ULONG)input.size(), 0);
    BCryptFinishHash(hHash, hash, hashLen, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    std::ostringstream ss;
    for (DWORD i = 0; i < hashLen; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

static std::string HashPassword(const std::string& password, const std::string& salt) {
    return Sha256(salt + password + salt);
}

bool AuthRegister(const std::string& username, const std::string& password,
                  const std::string& nickname, const std::string& email,
                  std::string& outError) {
    MYSQL* db = GetDB();
    if (!db) { outError = u8"\u6570\u636E\u5E93\u672A\u8FDE\u63A5"; return false; } // 数据库未连接

    if (username.empty() || username.size() < 3) {
        outError = u8"\u7528\u6237\u540D\u81F3\u5C11 3 \u4E2A\u5B57\u7B26"; return false;
    }
    if (password.empty() || password.size() < 6) {
        outError = u8"\u5BC6\u7801\u81F3\u5C11 6 \u4F4D"; return false;
    }

    // Check if username exists
    bool exists = false;
    DbQueryParam("SELECT id FROM users WHERE username = ?", {username},
        [&](int, char** vals, char**) { if (vals[0]) exists = true; });

    if (exists) {
        outError = u8"\u7528\u6237\u540D\u5DF2\u5B58\u5728"; return false; // 用户名已存在
    }

    std::string salt = GenerateSalt();
    std::string hash = HashPassword(password, salt);
    std::string nick = nickname.empty() ? username : nickname;

    bool ok = DbExecParam(
        "INSERT INTO users (username, password, salt, nickname, email) VALUES (?, ?, ?, ?, ?)",
        {username, hash, salt, nick, email});

    if (!ok) {
        outError = u8"\u6CE8\u518C\u5931\u8D25"; return false; // 注册失败
    }

    Log(LogLevel::Info, "User registered: %s", username.c_str());
    return true;
}

bool AuthLogin(const std::string& username, const std::string& password,
               std::string& outError) {
    MYSQL* db = GetDB();
    if (!db) { outError = u8"\u6570\u636E\u5E93\u672A\u8FDE\u63A5"; return false; }

    if (username.empty() || password.empty()) {
        outError = u8"\u8BF7\u8F93\u5165\u7528\u6237\u540D\u548C\u5BC6\u7801"; return false;
    }

    struct Row {
        int id = 0; std::string user, hash, salt, nick, avatarUrl, steamId, apiKey, created, lastLogin;
        int vipLevel = 0; std::string vipExpire;
        int coins = 0;
        std::string lastCheckIn;
        int consecutiveDays = 0;
        int monthlyCheckIns = 0;
        bool found = false;
    } row;

    DbQueryParam(
        "SELECT id, username, password, salt, nickname, avatar_url, vip_level, vip_expire, "
        "steam_id, api_key, created_at, last_login, COALESCE(coins, 0), "
        "COALESCE(last_checkin, ''), COALESCE(consecutive_days, 0), COALESCE(monthly_checkins, 0) "
        "FROM users WHERE username = ? AND status = 1",
        {username},
        [&](int, char** v, char**) {
            row.found = true;
            row.id        = v[0] ? atoi(v[0]) : 0;
            row.user      = v[1] ? v[1] : "";
            row.hash      = v[2] ? v[2] : "";
            row.salt      = v[3] ? v[3] : "";
            row.nick      = v[4] ? v[4] : "";
            row.avatarUrl = v[5] ? v[5] : "";
            row.vipLevel  = v[6] ? atoi(v[6]) : 0;
            row.vipExpire = v[7] ? v[7] : "";
            row.steamId   = v[8] ? v[8] : "";
            row.apiKey    = v[9] ? v[9] : "";
            row.created   = v[10] ? v[10] : "";
            row.lastLogin = v[11] ? v[11] : "";
            row.coins     = v[12] ? atoi(v[12]) : 0;
            row.lastCheckIn = v[13] ? v[13] : "";
            row.consecutiveDays = v[14] ? atoi(v[14]) : 0;
            row.monthlyCheckIns = v[15] ? atoi(v[15]) : 0;
        });

    if (!row.found) {
        outError = u8"\u7528\u6237\u540D\u4E0D\u5B58\u5728"; return false; // 用户名不存在
    }

    std::string inputHash = HashPassword(password, row.salt);
    if (inputHash != row.hash) {
        outError = u8"\u5BC6\u7801\u9519\u8BEF"; return false; // 密码错误
    }

    g_currentUser.id        = row.id;
    g_currentUser.username  = row.user;
    g_currentUser.nickname  = row.nick;
    g_currentUser.avatarUrl = row.avatarUrl;
    g_currentUser.vipLevel  = row.vipLevel;
    g_currentUser.vipExpire = row.vipExpire;
    g_currentUser.steamId   = row.steamId;
    g_currentUser.apiKey    = row.apiKey;
    g_currentUser.createdAt = row.created;
    g_currentUser.lastLogin = row.lastLogin;
    g_currentUser.coins     = row.coins;
    g_currentUser.lastCheckIn = row.lastCheckIn;
    g_currentUser.consecutiveDays = row.consecutiveDays;
    g_currentUser.monthlyCheckIns = row.monthlyCheckIns;
    g_loggedIn = true;

    // Update last_login
    DbExecParam("UPDATE users SET last_login = NOW() WHERE id = ?",
                {std::to_string(row.id)});

    Log(LogLevel::Info, "User logged in: %s (id=%d)", username.c_str(), row.id);

    // Auto-load Steam library if Steam ID is already bound
    if (!g_currentUser.steamId.empty()) {
        Log(LogLevel::Info, "Auto-loading Steam library for steamId=%s", g_currentUser.steamId.c_str());
        RequestSteamLibrary(g_currentUser.steamId);
    }

    return true;
}

void AuthLogout() {
    Log(LogLevel::Info, "AuthLogout: starting...");

    // Set logged out state first
    g_loggedIn = false;

    // Stop OAuth server first (this is quick if not running)
    Log(LogLevel::Info, "AuthLogout: stopping OAuth server...");
    StopOAuthCallbackServer();

    // Clear library data (this waits for background thread)
    Log(LogLevel::Info, "AuthLogout: clearing Steam library...");
    ClearSteamLibrary();

    // Clear user info last
    Log(LogLevel::Info, "AuthLogout: clearing user info...");
    g_currentUser = UserInfo();

    Log(LogLevel::Info, "AuthLogout: done");
}

bool IsLoggedIn() { return g_loggedIn; }
const UserInfo& GetCurrentUser() { return g_currentUser; }

bool AuthUpdateSteamId(const std::string& steamId) {
    if (!g_loggedIn) return false;
    Log(LogLevel::Info, "AuthUpdateSteamId: saving '%s' for user %d", steamId.c_str(), g_currentUser.id);
    // Use DbExec with escaped values (avoids prepared statement issues)
    std::string sql = "UPDATE users SET steam_id = '" + DbEscape(steamId) +
                      "' WHERE id = " + std::to_string(g_currentUser.id);
    bool ok = DbExec(sql.c_str());
    if (ok) {
        g_currentUser.steamId = steamId;
        Log(LogLevel::Info, "AuthUpdateSteamId: success");
    } else {
        Log(LogLevel::Error, "AuthUpdateSteamId: failed");
    }
    return ok;
}

bool AuthUpdateApiKey(const std::string& apiKey) {
    if (!g_loggedIn) return false;
    bool ok = DbExecParam("UPDATE users SET api_key = ? WHERE id = ?",
                          {apiKey, std::to_string(g_currentUser.id)});
    if (ok) g_currentUser.apiKey = apiKey;
    return ok;
}

bool AuthUpdateNickname(const std::string& nickname) {
    if (!g_loggedIn) return false;
    bool ok = DbExecParam("UPDATE users SET nickname = ? WHERE id = ?",
                          {nickname, std::to_string(g_currentUser.id)});
    if (ok) g_currentUser.nickname = nickname;
    return ok;
}

bool AuthUpdateAvatar(const std::string& avatarUrl) {
    if (!g_loggedIn) return false;
    bool ok = DbExecParam("UPDATE users SET avatar_url = ? WHERE id = ?",
                          {avatarUrl, std::to_string(g_currentUser.id)});
    if (ok) g_currentUser.avatarUrl = avatarUrl;
    return ok;
}

bool AuthActivateVip(int level) {
    if (!g_loggedIn) return false;
    int days = (level == 2) ? 365 : 30;
    std::string sql = "UPDATE users SET vip_level = ?, vip_expire = DATE_ADD(NOW(), INTERVAL ? DAY) WHERE id = ?";
    bool ok = DbExecParam(sql.c_str(),
        {std::to_string(level), std::to_string(days), std::to_string(g_currentUser.id)});
    if (ok) {
        g_currentUser.vipLevel = level;
        // Refresh vip_expire from DB
        DbQueryParam("SELECT vip_expire FROM users WHERE id = ?",
            {std::to_string(g_currentUser.id)},
            [&](int, char** v, char**) {
                g_currentUser.vipExpire = v[0] ? v[0] : "";
            });
        Log(LogLevel::Info, "VIP activated: level=%d, days=%d", level, days);
    }
    return ok;
}

bool IsVipActive() {
    if (!g_loggedIn || g_currentUser.vipLevel == 0) return false;
    if (g_currentUser.vipExpire.empty()) return false;
    // Simple check: query DB for NOW() < vip_expire
    bool active = false;
    DbQueryParam("SELECT 1 FROM users WHERE id = ? AND vip_expire > NOW()",
        {std::to_string(g_currentUser.id)},
        [&](int, char** v, char**) { active = true; });
    return active;
}

// ============================================================
// 签到系统
// ============================================================

// 获取今天的日期字符串 (YYYY-MM-DD)
static std::string GetTodayDateStr() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
    return buf;
}

// 检查两个日期是否是连续的 (date2 是 date1 的下一天)
static bool IsConsecutiveDay(const std::string& date1, const std::string& date2) {
    if (date1.empty() || date2.empty()) return false;

    int y1, m1, d1, y2, m2, d2;
    if (sscanf(date1.c_str(), "%d-%d-%d", &y1, &m1, &d1) != 3) return false;
    if (sscanf(date2.c_str(), "%d-%d-%d", &y2, &m2, &d2) != 3) return false;

    // 简单计算：转换为天数比较
    std::tm tm1 = {};
    tm1.tm_year = y1 - 1900;
    tm1.tm_mon = m1 - 1;
    tm1.tm_mday = d1;
    std::time_t t1 = std::mktime(&tm1);

    std::tm tm2 = {};
    tm2.tm_year = y2 - 1900;
    tm2.tm_mon = m2 - 1;
    tm2.tm_mday = d2;
    std::time_t t2 = std::mktime(&tm2);

    // 相差一天 (86400秒)
    return (t2 - t1) == 86400;
}

bool HasCheckedInToday() {
    if (!g_loggedIn) return false;
    std::string today = GetTodayDateStr();
    return g_currentUser.lastCheckIn == today;
}

void RefreshCheckInStatus() {
    if (!g_loggedIn) return;

    // 从数据库刷新签到状态
    DbQueryParam(
        "SELECT last_checkin, consecutive_days, monthly_checkins FROM users WHERE id = ?",
        {std::to_string(g_currentUser.id)},
        [&](int, char** v, char**) {
            g_currentUser.lastCheckIn = v[0] ? v[0] : "";
            g_currentUser.consecutiveDays = v[1] ? atoi(v[1]) : 0;
            g_currentUser.monthlyCheckIns = v[2] ? atoi(v[2]) : 0;
        });
}

bool AuthCheckIn(int& outCoinsEarned, std::string& outError) {
    if (!g_loggedIn) {
        outError = u8"请先登录";
        return false;
    }

    std::string today = GetTodayDateStr();

    // 检查今天是否已签到
    if (g_currentUser.lastCheckIn == today) {
        outError = u8"今天已经签到过了";
        return false;
    }

    // 计算连续签到天数
    int newConsecutive = 1;
    if (IsConsecutiveDay(g_currentUser.lastCheckIn, today)) {
        newConsecutive = g_currentUser.consecutiveDays + 1;
    }

    // 计算获得的金币
    int coinsEarned = 10;  // 基础签到奖励

    // 连续签到奖励
    if (newConsecutive == 7) {
        coinsEarned += 100;  // 连续7天额外奖励
        Log(LogLevel::Info, "CheckIn: 7-day streak bonus! +100 coins");
    } else if (newConsecutive == 15) {
        coinsEarned += 200;  // 连续15天额外奖励
        Log(LogLevel::Info, "CheckIn: 15-day streak bonus! +200 coins");
    }

    // 更新数据库
    std::string sql =
        "UPDATE users SET "
        "last_checkin = '" + DbEscape(today) + "', "
        "consecutive_days = " + std::to_string(newConsecutive) + ", "
        "monthly_checkins = monthly_checkins + 1, "
        "coins = coins + " + std::to_string(coinsEarned) + " "
        "WHERE id = " + std::to_string(g_currentUser.id);

    if (!DbExec(sql.c_str())) {
        outError = u8"签到失败，请稍后重试";
        return false;
    }

    // 更新本地用户数据
    g_currentUser.lastCheckIn = today;
    g_currentUser.consecutiveDays = newConsecutive;
    g_currentUser.monthlyCheckIns++;
    g_currentUser.coins += coinsEarned;

    outCoinsEarned = coinsEarned;
    Log(LogLevel::Info, "CheckIn success: user=%d, consecutive=%d, coins_earned=%d, total_coins=%d",
        g_currentUser.id, newConsecutive, coinsEarned, g_currentUser.coins);

    return true;
}

bool DeductCoins(int amount, std::string& outError) {
    if (!g_loggedIn) {
        outError = u8"请先登录";
        return false;
    }

    if (amount <= 0) {
        outError = u8"无效的金额";
        return false;
    }

    if (g_currentUser.coins < amount) {
        outError = u8"星铸币不足";
        return false;
    }

    std::string sql = "UPDATE users SET coins = coins - " + std::to_string(amount) +
                      " WHERE id = " + std::to_string(g_currentUser.id) +
                      " AND coins >= " + std::to_string(amount);

    if (!DbExec(sql.c_str())) {
        outError = u8"扣除失败，请稍后重试";
        return false;
    }

    g_currentUser.coins -= amount;
    Log(LogLevel::Info, "DeductCoins: user=%d, amount=%d, remaining=%d",
        g_currentUser.id, amount, g_currentUser.coins);
    return true;
}

bool AddCoins(int amount, std::string& outError) {
    if (!g_loggedIn) {
        outError = u8"请先登录";
        return false;
    }

    if (amount <= 0) {
        outError = u8"无效的金额";
        return false;
    }

    std::string sql = "UPDATE users SET coins = coins + " + std::to_string(amount) +
                      " WHERE id = " + std::to_string(g_currentUser.id);

    if (!DbExec(sql.c_str())) {
        outError = u8"增加失败，请稍后重试";
        return false;
    }

    g_currentUser.coins += amount;
    Log(LogLevel::Info, "AddCoins: user=%d, amount=%d, total=%d",
        g_currentUser.id, amount, g_currentUser.coins);
    return true;
}

bool AddVipDays(int days, std::string& outError) {
    if (!g_loggedIn) {
        outError = u8"请先登录";
        return false;
    }

    if (days <= 0) {
        outError = u8"无效的天数";
        return false;
    }

    // 计算新的VIP到期时间
    std::string sql;
    if (g_currentUser.vipLevel == 0 || g_currentUser.vipExpire.empty()) {
        // 没有VIP，从今天开始计算
        sql = "UPDATE users SET vip_level = 1, vip_expire = DATE_ADD(NOW(), INTERVAL " +
              std::to_string(days) + " DAY) WHERE id = " + std::to_string(g_currentUser.id);
    } else {
        // 已有VIP，在现有基础上延长
        sql = "UPDATE users SET vip_expire = DATE_ADD(GREATEST(vip_expire, NOW()), INTERVAL " +
              std::to_string(days) + " DAY) WHERE id = " + std::to_string(g_currentUser.id);
    }

    if (!DbExec(sql.c_str())) {
        outError = u8"增加VIP天数失败";
        return false;
    }

    // 更新本地状态
    if (g_currentUser.vipLevel == 0) {
        g_currentUser.vipLevel = 1;
    }

    Log(LogLevel::Info, "AddVipDays: user=%d, days=%d", g_currentUser.id, days);
    return true;
}

// ============================================================
// OAuth Implementation
// ============================================================

// OAuth state
static std::atomic<bool> g_oauthServerRunning{false};
static std::atomic<bool> g_oauthPending{false};
static std::string g_oauthResult;
static std::string g_oauthType; // "steam" or "github"
static std::mutex g_oauthMtx;
static SOCKET g_oauthSocket = INVALID_SOCKET;
static bool g_wsaInitialized = false;
static int g_oauthActualPort = 0; // Actual port used (may differ from default)

// Your OAuth app credentials
static const char* GITHUB_CLIENT_ID = "Iv23liICmfKe8nDckS69";
static const char* GITHUB_CLIENT_SECRET = "101bb343cb2341529e6263e9b18687013ec49ed9";
static const int OAUTH_CALLBACK_PORT_START = 18880;  // Try ports starting from here
static const int OAUTH_CALLBACK_PORT_END = 18899;    // Up to this port

std::string GetSteamOAuthUrl() {
    // Steam OpenID 2.0 authentication URL
    int port = g_oauthActualPort > 0 ? g_oauthActualPort : OAUTH_CALLBACK_PORT_START;
    std::string returnUrl = "http://127.0.0.1:" + std::to_string(port) + "/steam/callback";
    std::string url = "https://steamcommunity.com/openid/login?"
        "openid.ns=http://specs.openid.net/auth/2.0&"
        "openid.mode=checkid_setup&"
        "openid.return_to=" + returnUrl + "&"
        "openid.realm=http://127.0.0.1:" + std::to_string(port) + "/&"
        "openid.identity=http://specs.openid.net/auth/2.0/identifier_select&"
        "openid.claimed_id=http://specs.openid.net/auth/2.0/identifier_select";
    return url;
}

std::string GetGitHubOAuthUrl() {
    // GitHub OAuth 2.0 authorization URL
    // Use 127.0.0.1 instead of localhost - GitHub allows any port with loopback IP
    int port = g_oauthActualPort > 0 ? g_oauthActualPort : OAUTH_CALLBACK_PORT_START;
    std::string redirectUri = "http://127.0.0.1:" + std::to_string(port) + "/github/callback";
    std::string url = "https://github.com/login/oauth/authorize?"
        "client_id=" + std::string(GITHUB_CLIENT_ID) + "&"
        "redirect_uri=" + redirectUri + "&"
        "scope=user:email";
    return url;
}

// Extract Steam ID from OpenID claimed_id
static std::string ExtractSteamId(const std::string& claimedId) {
    // Format: https://steamcommunity.com/openid/id/76561198xxxxxxxxx
    size_t pos = claimedId.rfind('/');
    if (pos != std::string::npos && pos + 1 < claimedId.size()) {
        return claimedId.substr(pos + 1);
    }
    return "";
}

// URL decode function
static std::string UrlDecode(const std::string& str) {
    std::string decoded;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int hex = 0;
            if (sscanf(str.c_str() + i + 1, "%2x", &hex) == 1) {
                decoded += (char)hex;
                i += 2;
            } else {
                decoded += str[i];
            }
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

// Parse URL query parameters
static std::map<std::string, std::string> ParseQueryParams(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;
    while (std::getline(iss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = UrlDecode(pair.substr(0, eq));
            std::string val = UrlDecode(pair.substr(eq + 1));
            params[key] = val;
        }
    }
    return params;
}

// Simple HTTP server to receive OAuth callback
static void OAuthServerThread() {
    // WSA is already initialized in StartOAuthCallbackServer

    g_oauthSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_oauthSocket == INVALID_SOCKET) {
        Log(LogLevel::Error, "OAuth: socket creation failed, error=%d", WSAGetLastError());
        return;
    }

    // Allow port reuse and set socket options for faster binding
    int opt = 1;
    setsockopt(g_oauthSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Disable Nagle's algorithm for faster response
    setsockopt(g_oauthSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&opt, sizeof(opt));

    // Set receive buffer size
    int rcvBufSize = 65536;
    setsockopt(g_oauthSocket, SOL_SOCKET, SO_RCVBUF, (char*)&rcvBufSize, sizeof(rcvBufSize));

    // Try multiple ports until we find one that works
    int boundPort = 0;
    for (int port = OAUTH_CALLBACK_PORT_START; port <= OAUTH_CALLBACK_PORT_END; port++) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Bind specifically to localhost
        addr.sin_port = htons(port);

        if (bind(g_oauthSocket, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
            boundPort = port;
            Log(LogLevel::Info, "OAuth: bound to 127.0.0.1:%d", port);
            break;
        }
        Log(LogLevel::Debug, "OAuth: port %d in use, trying next...", port);
    }

    if (boundPort == 0) {
        Log(LogLevel::Error, "OAuth: all ports %d-%d in use", OAUTH_CALLBACK_PORT_START, OAUTH_CALLBACK_PORT_END);
        closesocket(g_oauthSocket);
        g_oauthSocket = INVALID_SOCKET;
        return;
    }

    // Use larger backlog for reliability
    if (listen(g_oauthSocket, SOMAXCONN) == SOCKET_ERROR) {
        Log(LogLevel::Error, "OAuth: listen failed, error=%d", WSAGetLastError());
        closesocket(g_oauthSocket);
        g_oauthSocket = INVALID_SOCKET;
        g_oauthActualPort = 0;
        return;
    }

    Log(LogLevel::Info, "OAuth callback server listening on 127.0.0.1:%d", boundPort);

    // Set socket to non-blocking for clean shutdown AFTER listen succeeds
    u_long mode = 1;
    ioctlsocket(g_oauthSocket, FIONBIO, &mode);

    // Memory barrier to ensure all writes are visible before setting flags
    std::atomic_thread_fence(std::memory_order_release);

    // IMPORTANT: Set port and running flag AFTER everything is ready
    // This ensures the main thread knows the server is truly ready
    g_oauthActualPort = boundPort;
    g_oauthServerRunning = true;

    Log(LogLevel::Info, "OAuth: Server fully ready on port %d", boundPort);

    while (g_oauthServerRunning) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(g_oauthSocket, &readSet);
        timeval tv = {0, 50000}; // 50ms timeout for faster response

        int sel = select(0, &readSet, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        SOCKET client = accept(g_oauthSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        // IMPORTANT: Set client socket to blocking mode for reliable recv
        u_long blockingMode = 0;
        ioctlsocket(client, FIONBIO, &blockingMode);

        // Set client socket timeout
        DWORD timeout = 10000; // 10 seconds for slow connections
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

        // Read HTTP request - use larger buffer for Steam's long callback URL
        char buf[8192] = {};
        int totalLen = 0;
        int len;

        // Read until we get the full HTTP request (ends with \r\n\r\n)
        while (totalLen < (int)sizeof(buf) - 1) {
            len = recv(client, buf + totalLen, sizeof(buf) - 1 - totalLen, 0);
            if (len <= 0) break;
            totalLen += len;
            buf[totalLen] = '\0';
            // Check if we have the complete HTTP headers
            if (strstr(buf, "\r\n\r\n")) break;
        }

        if (totalLen > 0) {
            std::string request(buf, totalLen);
            Log(LogLevel::Info, "OAuth received request, length=%d", totalLen);
            Log(LogLevel::Debug, "OAuth request: %s", request.substr(0, 300).c_str());

            // Parse request line: GET /path?query HTTP/1.1
            std::string path, query;
            if (request.find("GET ") == 0) {
                size_t pathStart = 4;
                size_t pathEnd = request.find(' ', pathStart);
                if (pathEnd != std::string::npos) {
                    std::string fullPath = request.substr(pathStart, pathEnd - pathStart);
                    size_t qmark = fullPath.find('?');
                    if (qmark != std::string::npos) {
                        path = fullPath.substr(0, qmark);
                        query = fullPath.substr(qmark + 1);
                    } else {
                        path = fullPath;
                    }
                    Log(LogLevel::Info, "OAuth path: %s, query length: %d", path.c_str(), (int)query.length());
                }
            }

            std::string responseBody;
            bool success = false;

            if (path == "/steam/callback") {
                Log(LogLevel::Info, "Processing Steam callback...");
                auto params = ParseQueryParams(query);

                // Log all params for debugging
                Log(LogLevel::Debug, "Steam callback params count: %d", (int)params.size());
                for (const auto& p : params) {
                    Log(LogLevel::Debug, "  %s = %s", p.first.c_str(), p.second.substr(0, 50).c_str());
                }

                std::string claimedId = params["openid.claimed_id"];
                Log(LogLevel::Info, "Steam claimed_id: %s", claimedId.c_str());

                std::string steamId = ExtractSteamId(claimedId);
                Log(LogLevel::Info, "Extracted Steam ID: %s", steamId.c_str());

                if (!steamId.empty()) {
                    std::lock_guard<std::mutex> lock(g_oauthMtx);
                    g_oauthResult = steamId;
                    g_oauthType = "steam";
                    g_oauthPending = true;
                    success = true;
                    Log(LogLevel::Info, "Steam OAuth success! SteamID=%s", steamId.c_str());
                    responseBody = u8"<html><head><meta charset='utf-8'></head>"
                        u8"<body style='background:#1b2838;color:#fff;font-family:sans-serif;text-align:center;padding-top:100px;'>"
                        u8"<h1>\u2705 Steam \u767B\u5F55\u6210\u529F</h1>"
                        u8"<p>\u8BF7\u8FD4\u56DE SteamForge \u5E94\u7528</p>"
                        u8"<script>setTimeout(function(){window.close();},2000);</script></body></html>";
                } else {
                    Log(LogLevel::Error, "Steam OAuth failed: could not extract Steam ID");
                    responseBody = u8"<html><head><meta charset='utf-8'></head>"
                        u8"<body style='background:#1b2838;color:#fff;font-family:sans-serif;text-align:center;padding-top:100px;'>"
                        u8"<h1>\u274C Steam \u767B\u5F55\u5931\u8D25</h1>"
                        u8"<p>\u65E0\u6CD5\u83B7\u53D6 Steam ID</p></body></html>";
                }
            } else if (path == "/github/callback") {
                Log(LogLevel::Info, "Processing GitHub callback...");
                auto params = ParseQueryParams(query);
                std::string code = params["code"];
                if (!code.empty()) {
                    std::lock_guard<std::mutex> lock(g_oauthMtx);
                    g_oauthResult = code;
                    g_oauthType = "github";
                    g_oauthPending = true;
                    success = true;
                    Log(LogLevel::Info, "GitHub OAuth success! code=%s...", code.substr(0, 10).c_str());
                    responseBody = u8"<html><head><meta charset='utf-8'></head>"
                        u8"<body style='background:#24292e;color:#fff;font-family:sans-serif;text-align:center;padding-top:100px;'>"
                        u8"<h1>\u2705 GitHub \u767B\u5F55\u6210\u529F</h1>"
                        u8"<p>\u8BF7\u8FD4\u56DE SteamForge \u5E94\u7528</p>"
                        u8"<script>setTimeout(function(){window.close();},2000);</script></body></html>";
                } else {
                    Log(LogLevel::Error, "GitHub OAuth failed: no code in callback");
                    responseBody = u8"<html><head><meta charset='utf-8'></head>"
                        u8"<body style='background:#24292e;color:#fff;font-family:sans-serif;text-align:center;padding-top:100px;'>"
                        u8"<h1>\u274C GitHub \u767B\u5F55\u5931\u8D25</h1></body></html>";
                }
            } else {
                Log(LogLevel::Warn, "OAuth: unknown callback path: %s", path.c_str());
                responseBody = "<html><body>Invalid callback</body></html>";
            }

            // Send HTTP response
            std::string response = "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: " + std::to_string(responseBody.size()) + "\r\n"
                "Connection: close\r\n\r\n" + responseBody;
            send(client, response.c_str(), (int)response.size(), 0);

            if (success) {
                Log(LogLevel::Info, "OAuth callback received: type=%s", g_oauthType.c_str());
            }
        }
        closesocket(client);
    }

    // Socket may already be closed by StopOAuthCallbackServer, check before closing
    SOCKET sock = g_oauthSocket;
    g_oauthSocket = INVALID_SOCKET;
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
}

void StartOAuthCallbackServer() {
    Log(LogLevel::Info, "OAuth: Starting callback server...");

    // Stop any existing server first
    if (g_oauthServerRunning) {
        StopOAuthCallbackServer();
        // Wait a bit for port to be released
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    // Initialize Winsock once
    if (!g_wsaInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            Log(LogLevel::Error, "OAuth: WSAStartup failed");
            return;
        }
        g_wsaInitialized = true;
    }

    g_oauthPending = false;
    g_oauthActualPort = 0;  // Reset port
    {
        std::lock_guard<std::mutex> lock(g_oauthMtx);
        g_oauthResult.clear();
        g_oauthType.clear();
    }

    // Detach the thread so it runs independently
    std::thread serverThread(OAuthServerThread);
    serverThread.detach();

    // Wait for server to actually start and bind to a port (up to 5 seconds)
    for (int i = 0; i < 50; i++) {
        if (g_oauthServerRunning && g_oauthActualPort > 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (g_oauthServerRunning && g_oauthActualPort > 0) {
        // Give the server a moment to be fully ready for accept()
        // Don't do test connections as they can interfere with real requests
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        Log(LogLevel::Info, "OAuth: Server ready on port %d", g_oauthActualPort);
    } else {
        Log(LogLevel::Error, "OAuth: Server failed to start (running=%d, port=%d)",
            g_oauthServerRunning.load() ? 1 : 0, g_oauthActualPort);
    }
}

bool IsOAuthServerRunning() {
    return g_oauthServerRunning.load();
}

int GetOAuthServerPort() {
    return g_oauthActualPort;
}

void StopOAuthCallbackServer() {
    Log(LogLevel::Info, "StopOAuthCallbackServer: checking state...");

    // Check if server is running - use atomic load
    if (!g_oauthServerRunning.load()) {
        Log(LogLevel::Info, "StopOAuthCallbackServer: server not running, just resetting state");
        // Just reset state, no blocking
        g_oauthActualPort = 0;
        g_oauthPending = false;
        {
            std::lock_guard<std::mutex> lock(g_oauthMtx);
            g_oauthResult.clear();
            g_oauthType.clear();
        }
        return;
    }

    Log(LogLevel::Info, "StopOAuthCallbackServer: stopping server...");
    g_oauthServerRunning = false;

    // Close the socket to unblock accept() - do this atomically
    SOCKET sock = g_oauthSocket;
    g_oauthSocket = INVALID_SOCKET;
    if (sock != INVALID_SOCKET) {
        Log(LogLevel::Info, "StopOAuthCallbackServer: closing socket...");
        // Use linger to force immediate close
        struct linger lin = {1, 0};  // Hard close
        setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&lin, sizeof(lin));
        shutdown(sock, SD_BOTH);
        closesocket(sock);
    }

    // Brief wait for thread to notice
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Reset OAuth state
    Log(LogLevel::Info, "StopOAuthCallbackServer: resetting state...");
    g_oauthActualPort = 0;
    g_oauthPending = false;
    {
        std::lock_guard<std::mutex> lock(g_oauthMtx);
        g_oauthResult.clear();
        g_oauthType.clear();
    }

    Log(LogLevel::Info, "StopOAuthCallbackServer: done");
}

bool IsOAuthCallbackPending() {
    return g_oauthPending;
}

std::string GetOAuthResult() {
    std::lock_guard<std::mutex> lock(g_oauthMtx);
    return g_oauthResult;
}

bool AuthLoginWithSteam(const std::string& steamId, std::string& outError) {
    MYSQL* db = GetDB();
    if (!db) { outError = u8"\u6570\u636E\u5E93\u672A\u8FDE\u63A5"; return false; }

    if (steamId.empty()) {
        outError = u8"Steam ID \u65E0\u6548"; return false;
    }

    // Check if user with this Steam ID exists
    struct Row {
        int id = 0; std::string user, nick, avatarUrl, steamId, apiKey, created, lastLogin;
        int vipLevel = 0; std::string vipExpire;
        int coins = 0;
        bool found = false;
    } row;

    DbQueryParam(
        "SELECT id, username, nickname, avatar_url, vip_level, vip_expire, "
        "steam_id, api_key, created_at, last_login, COALESCE(coins, 0) FROM users WHERE steam_id = ? AND status = 1",
        {steamId},
        [&](int, char** v, char**) {
            row.found = true;
            row.id        = v[0] ? atoi(v[0]) : 0;
            row.user      = v[1] ? v[1] : "";
            row.nick      = v[2] ? v[2] : "";
            row.avatarUrl = v[3] ? v[3] : "";
            row.vipLevel  = v[4] ? atoi(v[4]) : 0;
            row.vipExpire = v[5] ? v[5] : "";
            row.steamId   = v[6] ? v[6] : "";
            row.apiKey    = v[7] ? v[7] : "";
            row.created   = v[8] ? v[8] : "";
            row.lastLogin = v[9] ? v[9] : "";
            row.coins     = v[10] ? atoi(v[10]) : 0;
        });

    if (!row.found) {
        // Auto-register new user with Steam ID
        std::string username = "steam_" + steamId;
        std::string nickname = username; // Use username as default nickname
        std::string salt = GenerateSalt();
        std::string hash = HashPassword(GenerateSalt(), salt); // Random password

        bool ok = DbExecParam(
            "INSERT INTO users (username, password, salt, nickname, steam_id) VALUES (?, ?, ?, ?, ?)",
            {username, hash, salt, nickname, steamId});

        if (!ok) {
            outError = u8"\u521B\u5EFA\u8D26\u6237\u5931\u8D25"; return false;
        }

        // Fetch the newly created user
        DbQueryParam(
            "SELECT id, username, nickname, avatar_url, vip_level, vip_expire, "
            "steam_id, api_key, created_at, last_login, COALESCE(coins, 0) FROM users WHERE steam_id = ?",
            {steamId},
            [&](int, char** v, char**) {
                row.found = true;
                row.id        = v[0] ? atoi(v[0]) : 0;
                row.user      = v[1] ? v[1] : "";
                row.nick      = v[2] ? v[2] : "";
                row.avatarUrl = v[3] ? v[3] : "";
                row.vipLevel  = v[4] ? atoi(v[4]) : 0;
                row.vipExpire = v[5] ? v[5] : "";
                row.steamId   = v[6] ? v[6] : "";
                row.apiKey    = v[7] ? v[7] : "";
                row.created   = v[8] ? v[8] : "";
                row.lastLogin = v[9] ? v[9] : "";
                row.coins     = v[10] ? atoi(v[10]) : 0;
            });

        Log(LogLevel::Info, "New user created via Steam OAuth: %s", username.c_str());
    }

    // Set current user
    g_currentUser.id        = row.id;
    g_currentUser.username  = row.user;
    g_currentUser.nickname  = row.nick;
    g_currentUser.avatarUrl = row.avatarUrl;
    g_currentUser.vipLevel  = row.vipLevel;
    g_currentUser.vipExpire = row.vipExpire;
    g_currentUser.steamId   = row.steamId;
    g_currentUser.apiKey    = row.apiKey;
    g_currentUser.createdAt = row.created;
    g_currentUser.lastLogin = row.lastLogin;
    g_currentUser.coins     = row.coins;
    g_loggedIn = true;

    // Update last_login
    DbExecParam("UPDATE users SET last_login = NOW() WHERE id = ?",
                {std::to_string(row.id)});

    Log(LogLevel::Info, "User logged in via Steam: %s (id=%d)", row.user.c_str(), row.id);

    // Auto-load Steam library
    if (!g_currentUser.steamId.empty()) {
        RequestSteamLibrary(g_currentUser.steamId);
    }

    // Clear OAuth state (thread-safe)
    g_oauthPending = false;
    {
        std::lock_guard<std::mutex> lock(g_oauthMtx);
        g_oauthResult.clear();
    }

    return true;
}

bool AuthLoginWithGitHub(const std::string& code, std::string& outError) {
    MYSQL* db = GetDB();
    if (!db) { outError = u8"\u6570\u636E\u5E93\u672A\u8FDE\u63A5"; return false; }

    if (code.empty()) {
        outError = u8"GitHub \u6388\u6743\u7801\u65E0\u6548"; return false;
    }

    // Step 1: Exchange code for access token
    HINTERNET hSession = WinHttpOpen(L"SteamForge/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        outError = u8"HTTP \u521D\u59CB\u5316\u5931\u8D25"; return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        outError = u8"\u8FDE\u63A5 GitHub \u5931\u8D25"; return false;
    }

    // POST to /login/oauth/access_token
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/login/oauth/access_token",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = u8"\u521B\u5EFA\u8BF7\u6C42\u5931\u8D25"; return false;
    }

    // Build POST body
    std::string postBody = "client_id=" + std::string(GITHUB_CLIENT_ID) +
        "&client_secret=" + std::string(GITHUB_CLIENT_SECRET) +
        "&code=" + code;

    // Set headers
    WinHttpAddRequestHeaders(hRequest, L"Accept: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/x-www-form-urlencoded", -1, WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)postBody.c_str(), (DWORD)postBody.size(), (DWORD)postBody.size(), 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = u8"\u53D1\u9001\u8BF7\u6C42\u5931\u8D25"; return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = u8"\u63A5\u6536\u54CD\u5E94\u5931\u8D25"; return false;
    }

    // Read response
    std::string tokenResponse;
    DWORD bytesRead = 0;
    char buffer[4096];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        tokenResponse += buffer;
    }
    WinHttpCloseHandle(hRequest);

    Log(LogLevel::Debug, "GitHub token response: %s", tokenResponse.c_str());

    // Parse access_token from JSON response
    std::string accessToken;
    size_t tokenPos = tokenResponse.find("\"access_token\":\"");
    if (tokenPos != std::string::npos) {
        tokenPos += 16;
        size_t tokenEnd = tokenResponse.find("\"", tokenPos);
        if (tokenEnd != std::string::npos) {
            accessToken = tokenResponse.substr(tokenPos, tokenEnd - tokenPos);
        }
    }

    if (accessToken.empty()) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = u8"\u83B7\u53D6 access_token \u5931\u8D25";
        g_oauthPending = false;
        {
            std::lock_guard<std::mutex> lock(g_oauthMtx);
            g_oauthResult.clear();
        }
        return false;
    }

    // Step 2: Get user info from GitHub API
    HINTERNET hApiConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hApiConnect) {
        WinHttpCloseHandle(hSession);
        outError = u8"\u8FDE\u63A5 GitHub API \u5931\u8D25"; return false;
    }

    HINTERNET hUserRequest = WinHttpOpenRequest(hApiConnect, L"GET", L"/user",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hUserRequest) {
        WinHttpCloseHandle(hApiConnect);
        WinHttpCloseHandle(hSession);
        outError = u8"\u521B\u5EFA\u7528\u6237\u8BF7\u6C42\u5931\u8D25"; return false;
    }

    // Set Authorization header
    std::wstring authHeader = L"Authorization: Bearer " + std::wstring(accessToken.begin(), accessToken.end());
    WinHttpAddRequestHeaders(hUserRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hUserRequest, L"User-Agent: SteamForge/1.0", -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(hUserRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) ||
        !WinHttpReceiveResponse(hUserRequest, nullptr)) {
        WinHttpCloseHandle(hUserRequest);
        WinHttpCloseHandle(hApiConnect);
        WinHttpCloseHandle(hSession);
        outError = u8"\u83B7\u53D6\u7528\u6237\u4FE1\u606F\u5931\u8D25"; return false;
    }

    std::string userResponse;
    while (WinHttpReadData(hUserRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        userResponse += buffer;
    }
    WinHttpCloseHandle(hUserRequest);

    Log(LogLevel::Debug, "GitHub user response: %s", userResponse.c_str());

    // Parse user info from JSON
    std::string githubId, githubLogin, githubName, githubEmail, githubAvatar;

    // Parse id (number)
    size_t idPos = userResponse.find("\"id\":");
    if (idPos != std::string::npos) {
        idPos += 5;
        size_t idEnd = userResponse.find_first_of(",}", idPos);
        if (idEnd != std::string::npos) {
            githubId = userResponse.substr(idPos, idEnd - idPos);
            // Trim whitespace
            while (!githubId.empty() && (githubId[0] == ' ' || githubId[0] == '\t')) githubId.erase(0, 1);
        }
    }

    // Parse login
    size_t loginPos = userResponse.find("\"login\":\"");
    if (loginPos != std::string::npos) {
        loginPos += 9;
        size_t loginEnd = userResponse.find("\"", loginPos);
        if (loginEnd != std::string::npos) {
            githubLogin = userResponse.substr(loginPos, loginEnd - loginPos);
        }
    }

    // Parse name
    size_t namePos = userResponse.find("\"name\":");
    if (namePos != std::string::npos) {
        namePos += 7;
        if (userResponse[namePos] == '\"') {
            namePos++;
            size_t nameEnd = userResponse.find("\"", namePos);
            if (nameEnd != std::string::npos) {
                githubName = userResponse.substr(namePos, nameEnd - namePos);
            }
        }
    }

    // Parse avatar_url
    size_t avatarPos = userResponse.find("\"avatar_url\":\"");
    if (avatarPos != std::string::npos) {
        avatarPos += 14;
        size_t avatarEnd = userResponse.find("\"", avatarPos);
        if (avatarEnd != std::string::npos) {
            githubAvatar = userResponse.substr(avatarPos, avatarEnd - avatarPos);
        }
    }

    // Step 3: Get user email (may need separate API call)
    HINTERNET hEmailRequest = WinHttpOpenRequest(hApiConnect, L"GET", L"/user/emails",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (hEmailRequest) {
        WinHttpAddRequestHeaders(hEmailRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(hEmailRequest, L"User-Agent: SteamForge/1.0", -1, WINHTTP_ADDREQ_FLAG_ADD);

        if (WinHttpSendRequest(hEmailRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) &&
            WinHttpReceiveResponse(hEmailRequest, nullptr)) {
            std::string emailResponse;
            while (WinHttpReadData(hEmailRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                emailResponse += buffer;
            }
            Log(LogLevel::Debug, "GitHub email response: %s", emailResponse.c_str());

            // Find primary email
            size_t primaryPos = emailResponse.find("\"primary\":true");
            if (primaryPos != std::string::npos) {
                // Search backwards for email
                size_t emailStart = emailResponse.rfind("\"email\":\"", primaryPos);
                if (emailStart != std::string::npos) {
                    emailStart += 9;
                    size_t emailEnd = emailResponse.find("\"", emailStart);
                    if (emailEnd != std::string::npos) {
                        githubEmail = emailResponse.substr(emailStart, emailEnd - emailStart);
                    }
                }
            }
        }
        WinHttpCloseHandle(hEmailRequest);
    }

    WinHttpCloseHandle(hApiConnect);
    WinHttpCloseHandle(hSession);

    if (githubId.empty()) {
        outError = u8"\u83B7\u53D6 GitHub ID \u5931\u8D25";
        g_oauthPending = false;
        {
            std::lock_guard<std::mutex> lock(g_oauthMtx);
            g_oauthResult.clear();
        }
        return false;
    }

    Log(LogLevel::Info, "GitHub user: id=%s, login=%s, name=%s, email=%s",
        githubId.c_str(), githubLogin.c_str(), githubName.c_str(), githubEmail.c_str());

    // Step 4: Check if user exists in database, or create new user
    struct Row {
        int id = 0; std::string user, nick, avatarUrl, steamId, apiKey, created, lastLogin;
        int vipLevel = 0; std::string vipExpire;
        int coins = 0;
        bool found = false;
    } row;

    DbQueryParam(
        "SELECT id, username, nickname, avatar_url, vip_level, vip_expire, "
        "steam_id, api_key, created_at, last_login, COALESCE(coins, 0) FROM users WHERE github_id = ? AND status = 1",
        {githubId},
        [&](int, char** v, char**) {
            row.found = true;
            row.id        = v[0] ? atoi(v[0]) : 0;
            row.user      = v[1] ? v[1] : "";
            row.nick      = v[2] ? v[2] : "";
            row.avatarUrl = v[3] ? v[3] : "";
            row.vipLevel  = v[4] ? atoi(v[4]) : 0;
            row.vipExpire = v[5] ? v[5] : "";
            row.steamId   = v[6] ? v[6] : "";
            row.apiKey    = v[7] ? v[7] : "";
            row.created   = v[8] ? v[8] : "";
            row.lastLogin = v[9] ? v[9] : "";
            row.coins     = v[10] ? atoi(v[10]) : 0;
        });

    if (!row.found) {
        // Auto-register new user with GitHub info
        std::string username = "github_" + githubLogin;
        std::string nickname = githubLogin; // Always use GitHub login as nickname
        std::string salt = GenerateSalt();
        std::string hash = HashPassword(GenerateSalt(), salt); // Random password

        bool ok = DbExecParam(
            "INSERT INTO users (username, password, salt, nickname, email, avatar_url, github_id) VALUES (?, ?, ?, ?, ?, ?, ?)",
            {username, hash, salt, nickname, githubEmail, githubAvatar, githubId});

        if (!ok) {
            outError = u8"\u521B\u5EFA\u8D26\u6237\u5931\u8D25";
            g_oauthPending = false;
            {
                std::lock_guard<std::mutex> lock(g_oauthMtx);
                g_oauthResult.clear();
            }
            return false;
        }

        // Fetch the newly created user
        DbQueryParam(
            "SELECT id, username, nickname, avatar_url, vip_level, vip_expire, "
            "steam_id, api_key, created_at, last_login, COALESCE(coins, 0) FROM users WHERE github_id = ?",
            {githubId},
            [&](int, char** v, char**) {
                row.found = true;
                row.id        = v[0] ? atoi(v[0]) : 0;
                row.user      = v[1] ? v[1] : "";
                row.nick      = v[2] ? v[2] : "";
                row.avatarUrl = v[3] ? v[3] : "";
                row.vipLevel  = v[4] ? atoi(v[4]) : 0;
                row.vipExpire = v[5] ? v[5] : "";
                row.steamId   = v[6] ? v[6] : "";
                row.apiKey    = v[7] ? v[7] : "";
                row.created   = v[8] ? v[8] : "";
                row.lastLogin = v[9] ? v[9] : "";
                row.coins     = v[10] ? atoi(v[10]) : 0;
            });

        Log(LogLevel::Info, "New user created via GitHub OAuth: %s", username.c_str());
    }

    // Set current user
    g_currentUser.id        = row.id;
    g_currentUser.username  = row.user;
    g_currentUser.nickname  = row.nick;
    g_currentUser.avatarUrl = row.avatarUrl.empty() ? githubAvatar : row.avatarUrl;
    g_currentUser.vipLevel  = row.vipLevel;
    g_currentUser.vipExpire = row.vipExpire;
    g_currentUser.steamId   = row.steamId;
    g_currentUser.apiKey    = row.apiKey;
    g_currentUser.createdAt = row.created;
    g_currentUser.lastLogin = row.lastLogin;
    g_currentUser.coins     = row.coins;
    g_loggedIn = true;

    // Update last_login and avatar if changed
    DbExecParam("UPDATE users SET last_login = NOW(), avatar_url = ? WHERE id = ?",
                {githubAvatar, std::to_string(row.id)});

    Log(LogLevel::Info, "User logged in via GitHub: %s (id=%d)", row.user.c_str(), row.id);

    // Clear OAuth state (thread-safe)
    g_oauthPending = false;
    {
        std::lock_guard<std::mutex> lock(g_oauthMtx);
        g_oauthResult.clear();
    }

    return true;
}

} // namespace sf
