#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdint>

namespace dui {

enum class LogLevel : uint8_t { Info = 0, Warn = 1, Error = 2 };

void Log     (const char* fmt, ...);
void LogWarn (const char* fmt, ...);
void LogError(const char* fmt, ...);

void Watch(const char* name, int         v);
void Watch(const char* name, float       v);
void Watch(const char* name, bool        v);
void Watch(const char* name, const char* v);
void Watch(const char* name, float x, float y);  // stored as "(x.xx, y.yy)"

inline void WatchFmt(const char* name, const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Watch(name, static_cast<const char*>(buf));
}

// Returns true and advances accumulator by -interval once accumulator >= interval.
// Typical use: static float acc = 0.f; if (EveryNSeconds(1.0f, dt, acc)) { ... }
inline bool EveryNSeconds(float interval, float dt, float& accumulator) {
    accumulator += dt;
    if (accumulator >= interval) { accumulator -= interval; return true; }
    return false;
}

void RemoveWatch(const char* name);
void ClearWatch();

void DrawLog();
void DrawWatch();

} // namespace dui
