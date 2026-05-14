#pragma once
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

void RemoveWatch(const char* name);
void ClearWatch();

void DrawLog();
void DrawWatch();

} // namespace dui
