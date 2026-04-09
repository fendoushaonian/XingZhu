#pragma once
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace sf {

struct SteamStatus {
    bool installed = false;
    bool running = false;
    std::string steamPath;    // e.g. "C:\Program Files (x86)\Steam"
    std::string steamExe;     // e.g. "C:/Program Files (x86)/Steam/steam.exe"
};

// Detect Steam installation and running state (call once at startup, cache result)
inline SteamStatus& GetSteamStatus() {
    static SteamStatus s_status;
    return s_status;
}

inline void DetectSteam() {
    auto& st = GetSteamStatus();
    st.installed = false;
    st.running = false;
    st.steamPath.clear();
    st.steamExe.clear();

    // Check registry for Steam install path
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buf[512] = {};
        DWORD sz = sizeof(buf);
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
            st.steamPath = buf;
            // Normalize path separators
            for (auto& c : st.steamPath) if (c == '/') c = '\\';
        }

        sz = sizeof(buf);
        memset(buf, 0, sizeof(buf));
        if (RegQueryValueExA(hKey, "SteamExe", nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
            st.steamExe = buf;
        }

        RegCloseKey(hKey);
    }

    // Verify steam.exe actually exists
    if (!st.steamExe.empty()) {
        std::string exePath = st.steamExe;
        for (auto& c : exePath) if (c == '/') c = '\\';
        DWORD attr = GetFileAttributesA(exePath.c_str());
        st.installed = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
    }

    // Check if Steam is currently running
    if (st.installed) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = {};
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(snap, &pe)) {
                do {
                    if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) {
                        st.running = true;
                        break;
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }
    }
}

inline bool IsSteamInstalled() { return GetSteamStatus().installed; }
inline bool IsSteamRunning()   { return GetSteamStatus().running; }

} // namespace sf
