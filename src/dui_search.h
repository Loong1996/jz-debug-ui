#pragma once
#include "dui_world.h"

namespace dui {

// Draw the global search overlay. Call every frame from DrawAll.
// Pressing Ctrl+P (when no text input is captured) opens the floating search window.
void DrawGlobalSearch(World& world);

} // namespace dui
