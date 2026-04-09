#pragma once
#include <string>

namespace sf {

struct UserInfo {
    int id = 0;
    std::string username;
    std::string nickname;
    std::string avatarUrl;
    int vipLevel = 0;          // 0=normal, 1=monthly, 2=yearly
    std::string vipExpire;     // datetime string
    std::string steamId;
    std::string apiKey;
    std::string createdAt;
    std::string lastLogin;
    int coins = 0;             // 星铸币余额

    // 签到相关
    std::string lastCheckIn;   // 上次签到日期 (YYYY-MM-DD)
    int consecutiveDays = 0;   // 连续签到天数
    int monthlyCheckIns = 0;   // 本月签到次数
};

// Auth system
bool AuthRegister(const std::string& username, const std::string& password,
                  const std::string& nickname, const std::string& email,
                  std::string& outError);

bool AuthLogin(const std::string& username, const std::string& password,
               std::string& outError);

void AuthLogout();

bool IsLoggedIn();
const UserInfo& GetCurrentUser();

// Update user fields
bool AuthUpdateSteamId(const std::string& steamId);
bool AuthUpdateApiKey(const std::string& apiKey);
bool AuthUpdateNickname(const std::string& nickname);
bool AuthUpdateAvatar(const std::string& avatarUrl);

// VIP
bool AuthActivateVip(int level); // 1=monthly(30d), 2=yearly(365d)
bool IsVipActive();

// 签到系统
bool AuthCheckIn(int& outCoinsEarned, std::string& outError);  // 执行签到，返回获得的金币数
bool HasCheckedInToday();  // 今天是否已签到
void RefreshCheckInStatus();  // 刷新签到状态

// 星铸币操作
bool DeductCoins(int amount, std::string& outError);  // 扣除星铸币
bool AddCoins(int amount, std::string& outError);     // 增加星铸币

// VIP天数操作
bool AddVipDays(int days, std::string& outError);     // 增加VIP天数

// OAuth login
// Returns the OAuth authorization URL to open in browser
std::string GetSteamOAuthUrl();
std::string GetGitHubOAuthUrl();

// Process OAuth callback (called after user authorizes in browser)
// steamId: Steam 64-bit ID from OpenID callback
bool AuthLoginWithSteam(const std::string& steamId, std::string& outError);
// code: GitHub OAuth authorization code
bool AuthLoginWithGitHub(const std::string& code, std::string& outError);

// Start local HTTP server to receive OAuth callback
void StartOAuthCallbackServer();
void StopOAuthCallbackServer();
bool IsOAuthCallbackPending();
bool IsOAuthServerRunning();  // Check if server is actually running
int GetOAuthServerPort();     // Get the actual port being used
std::string GetOAuthResult(); // Returns steamId or github code, empty if not ready

} // namespace sf
