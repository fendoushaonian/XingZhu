#pragma once
#include <cstdio>
#include <cstdarg>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sf {

enum class LogLevel { Debug, Info, Warn, Error };

inline FILE* GetLogFile() {
    static FILE* f = nullptr;
    if (!f) {
        f = fopen("steamforge.log", "w");
    }
    return f;
}

inline void Log(LogLevel level, const char* fmt, ...) {
    const char* prefix = "";
    switch (level) {
        case LogLevel::Debug: prefix = "[DEBUG]"; break;
        case LogLevel::Info:  prefix = "[INFO] "; break;
        case LogLevel::Warn:  prefix = "[WARN] "; break;
        case LogLevel::Error: prefix = "[ERROR]"; break;
    }

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int n = snprintf(buf, sizeof(buf), "%s ", prefix);
    n += vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);
    if (n < (int)sizeof(buf) - 1) { buf[n] = '\n'; buf[n+1] = '\0'; }

    OutputDebugStringA(buf);

    FILE* f = GetLogFile();
    if (f) { fputs(buf, f); fflush(f); }
}

} // namespace sf
