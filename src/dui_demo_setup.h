#pragma once
#include "dui_world.h"
#include "dui_metrics.h"

namespace dui {
namespace demo {

// One-time setup: register map/type names, drawers, commands, markers.
// world must outlive the registered command callbacks (typically stack in WinMain).
void SetupRegistrations(World& world);

// Per-frame simulation: tick mock world, push metrics, emit log/watch entries.
void PerFrameDemo(World& world, Metrics& metrics, float dt, int& tick_count);

} // namespace demo
} // namespace dui
