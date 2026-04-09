#include "core/online_detector.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace sf {

OnlineDetector& OnlineDetector::Get() {
    static OnlineDetector instance;
    return instance;
}

// 已知联机游戏列表
static const std::vector<std::string> s_knownOnlineGames = {
    // 大型多人在线
    "WorldOfWarcraft.exe", "wow.exe", "WowClassic.exe",
    "ffxiv.exe", "ffxiv_dx11.exe",
    "GenshinImpact.exe", "YuanShen.exe",
    "LeagueOfLegends.exe", "League of Legends.exe",
    "VALORANT.exe", "VALORANT-Win64-Shipping.exe",
    "csgo.exe", "cs2.exe",
    "dota2.exe",
    "Overwatch.exe",
    "FortniteClient-Win64-Shipping.exe",
    "PUBG.exe", "TslGame.exe",
    "ApexLegends.exe", "r5apex.exe",
    "RainbowSix.exe", "RainbowSix_Vulkan.exe",
    "destiny2.exe",
    "EscapeFromTarkov.exe",
    "RustClient.exe",
    "DayZ.exe", "DayZ_x64.exe",
    "arma3.exe", "arma3_x64.exe",
    "GTA5.exe", "PlayGTAV.exe", // GTA Online
    "RDR2.exe", // Red Dead Online
    "FiveM.exe", "FiveM_GTAProcess.exe",
    "NewWorld.exe",
    "LostArk.exe", "LOSTARK.exe",
    "BlackDesert64.exe",
    "bns.exe", "BNSR.exe", // 剑灵
    "DNF.exe", // 地下城与勇士
    "crossfire.exe", "cfgame.exe",
    "NARAKA.exe", // 永劫无间
    "eldenring.exe", // 有联机功能

    // 竞技/对战游戏
    "starcraft2.exe", "SC2.exe", "SC2_x64.exe",
    "hearthstone.exe",
    "MTGA.exe", // Magic Arena
    "PokerStars.exe",

    // 合作游戏（有联机）
    "left4dead2.exe", "l4d2.exe",
    "Back4Blood.exe",
    "DeepRockGalactic.exe",
    "Phasmophobia.exe",
    "AmongUs.exe",
    "FallGuys_client_game.exe",

    // 反作弊保护的游戏
    "EasyAntiCheat.exe",
    "BEService.exe", // BattlEye
    "vgc.exe", // Vanguard
};

const std::vector<std::string>& OnlineDetector::GetKnownOnlineGames() {
    return s_knownOnlineGames;
}

// 反作弊模块
static const std::vector<std::string> s_antiCheatModules = {
    "EasyAntiCheat.dll",
    "BEClient.dll", "BEClient_x64.dll",
    "vgk.sys", "vgkbootstatus.dat",
    "nProtect", "GameGuard",
    "XignCode",
    "PunkBuster",
    "FairFight",
    "VAC", // Valve Anti-Cheat
};

// 网络相关模块（可能表示联机）
static const std::vector<std::string> s_networkModules = {
    "steam_api64.dll", // Steam联机
    "steamnetworkingsockets.dll",
    "EOS", // Epic Online Services
    "vivox", // 语音聊天
    "discord", // Discord集成
};

bool OnlineDetector::IsOnlineGame(DWORD processId) {
    // 获取进程名
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) return false;

    char processName[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    QueryFullProcessImageNameA(hProcess, 0, processName, &size);
    CloseHandle(hProcess);

    // 提取文件名
    std::string fullPath = processName;
    std::string name = fullPath;
    size_t pos = fullPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        name = fullPath.substr(pos + 1);
    }

    return IsOnlineGame(name);
}

bool OnlineDetector::IsOnlineGame(const std::string& processName) {
    detectionReason_.clear();

    // 检查白名单
    if (IsWhitelisted(processName)) {
        return false;
    }

    // 1. 检查已知联机游戏
    if (CheckKnownOnlineGame(processName)) {
        return true;
    }

    // 2. 检查反作弊
    // 注意：这需要进程ID，这里简化处理
    // 实际使用时应该传入PID

    return false;
}

bool OnlineDetector::CheckKnownOnlineGame(const std::string& processName) {
    // 转小写比较
    std::string nameLower = processName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& known : s_knownOnlineGames) {
        std::string knownLower = known;
        std::transform(knownLower.begin(), knownLower.end(), knownLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (nameLower == knownLower) {
            detectionReason_ = "已知联机游戏: " + processName;
            return true;
        }
    }

    return false;
}

bool OnlineDetector::CheckLoadedModules(DWORD processId) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32W me;
    me.dwSize = sizeof(me);

    bool hasAntiCheat = false;

    if (Module32FirstW(hSnapshot, &me)) {
        do {
            char moduleName[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, moduleName, MAX_PATH, nullptr, nullptr);

            std::string nameStr = moduleName;
            std::string nameLower = nameStr;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            // 检查反作弊模块
            for (const auto& ac : s_antiCheatModules) {
                std::string acLower = ac;
                std::transform(acLower.begin(), acLower.end(), acLower.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                if (nameLower.find(acLower) != std::string::npos) {
                    detectionReason_ = "检测到反作弊: " + nameStr;
                    hasAntiCheat = true;
                    break;
                }
            }

            if (hasAntiCheat) break;

        } while (Module32NextW(hSnapshot, &me));
    }

    CloseHandle(hSnapshot);
    return hasAntiCheat;
}

bool OnlineDetector::CheckAntiCheat(DWORD processId) {
    return CheckLoadedModules(processId);
}

bool OnlineDetector::CheckNetworkConnections(DWORD processId) {
    // 获取TCP连接表
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    if (size == 0) return false;

    std::vector<uint8_t> buffer(size);
    PMIB_TCPTABLE_OWNER_PID tcpTable = (PMIB_TCPTABLE_OWNER_PID)buffer.data();

    if (GetExtendedTcpTable(tcpTable, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        return false;
    }

    int connectionCount = 0;
    int gameServerPorts = 0;

    // 常见游戏服务器端口
    std::vector<DWORD> gamePorts = {
        27015, 27016, 27017, // Steam/Source
        7777, 7778, 7779,    // Unreal
        25565,               // Minecraft
        3074,                // Xbox Live
        3478, 3479, 3480,    // PlayStation
    };

    for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
        if (tcpTable->table[i].dwOwningPid == processId &&
            tcpTable->table[i].dwState == MIB_TCP_STATE_ESTAB) {

            connectionCount++;

            DWORD remotePort = ntohs((u_short)tcpTable->table[i].dwRemotePort);
            for (DWORD gp : gamePorts) {
                if (remotePort == gp) {
                    gameServerPorts++;
                    break;
                }
            }
        }
    }

    // 如果有多个活跃连接或连接到游戏端口，可能是联机
    if (gameServerPorts > 0) {
        detectionReason_ = "检测到游戏服务器连接";
        return true;
    }

    return false;
}

bool OnlineDetector::ForceTerminate(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
    if (!hProcess) {
        return false;
    }

    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);

    return result != FALSE;
}

void OnlineDetector::AddToWhitelist(const std::string& processName) {
    std::string nameLower = processName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    whitelist_.insert(nameLower);
}

void OnlineDetector::RemoveFromWhitelist(const std::string& processName) {
    std::string nameLower = processName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    whitelist_.erase(nameLower);
}

bool OnlineDetector::IsWhitelisted(const std::string& processName) {
    std::string nameLower = processName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return whitelist_.find(nameLower) != whitelist_.end();
}

} // namespace sf
