#pragma once
#include "dui_world.h"
#include "dui_metrics.h"

namespace dui {

// Draw all built-in panels in a single call.
// Equivalent to calling DrawInspector + DrawCanvas + DrawMetrics +
// DrawLog + DrawWatch + DrawCommands + DrawEntityDetail individually.
// Use the individual Draw* functions instead when you want to omit panels
// or control draw order.
void DrawAll(World& world, Metrics& metrics);

} // namespace dui
