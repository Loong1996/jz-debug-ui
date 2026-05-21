#pragma once
#include "dui_world.h"
#include "dui_metrics.h"

namespace dui {

// Draw all built-in panels in a single call.
//
// Two overloads:
//   DrawAll(world)           — skip the built-in tick_ms / entity_count line
//                              plots. The metrics panel still shows the user
//                              metric curves (ConfigureMetric / Push) and any
//                              Tunable controls.
//   DrawAll(world, metrics)  — same, plus draw the built-in plots from
//                              `metrics`.
//
// For custom panel layouts call the individual Draw* functions instead.
void DrawAll(World& world);
void DrawAll(World& world, Metrics& metrics);

} // namespace dui
