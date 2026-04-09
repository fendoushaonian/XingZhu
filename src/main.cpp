//=============================================================================
// SteamForge — The Ultimate Steam Power Tool
// Built with C++17, Dear ImGui, DirectX 11
//=============================================================================

#include "app/application.h"
#include "app/config.h"
#include "utils/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <atomic>

// Global flag to indicate app is shutting down - detached threads should check this
std::atomic<bool> g_appShuttingDown{false};

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    // Don't show crash dialog during shutdown - it's expected that threads may fail
    if (g_appShuttingDown) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
        "SteamForge crashed!\n\n"
        "Exception code: 0x%08X\n"
        "Address: 0x%p\n\n"
        "Please report this to the developer.",
        (unsigned)ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress);
    MessageBoxA(NULL, buf, "SteamForge Crash Report", MB_OK | MB_ICONERROR);

    sf::Log(sf::LogLevel::Error, "CRASH: code=0x%08X addr=0x%p",
        (unsigned)ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    SetUnhandledExceptionFilter(CrashHandler);
    sf::Log(sf::LogLevel::Info, "SteamForge v%s starting...", sf::APP_VERSION);

    sf::Application app;

    if (!app.Init(hInstance)) {
        sf::Log(sf::LogLevel::Error, "Failed to initialize application");
        MessageBoxW(NULL, L"Failed to initialize SteamForge.\nPlease check DirectX 11 support.",
                    L"SteamForge Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    sf::Log(sf::LogLevel::Info, "Application running");
    int exitCode = app.Run();

    sf::Log(sf::LogLevel::Info, "Application shutting down (exit code: %d)", exitCode);

    // Signal all detached threads to stop
    g_appShuttingDown = true;

    return exitCode;
}
