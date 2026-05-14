#pragma once
#include <cstdint>

namespace dui {

// Push a discrete event onto the timeline.
// category: short name used for row grouping and color assignment (up to 8 unique categories shown).
// color: IM_COL32 override; 0 = auto-assign from palette.
void PushEvent(const char* category, const char* text, uint32_t color = 0);

void DrawEvents();  // ImGui panel — add to DrawAll or call manually

} // namespace dui
