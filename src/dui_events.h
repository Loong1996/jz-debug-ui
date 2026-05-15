#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdint>

namespace dui {

// Push a discrete event onto the timeline.
// category: short name used for row grouping and color assignment (up to 8 unique categories shown).
// color: IM_COL32 override; 0 = auto-assign from palette.
void PushEvent(const char* category, const char* text, uint32_t color = 0);

inline void PushEventFmt(const char* category, const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    PushEvent(category, buf);
}

inline void PushEventFmtColored(const char* category, uint32_t color, const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    PushEvent(category, buf, color);
}

void DrawEvents();  // ImGui panel — add to DrawAll or call manually

} // namespace dui
