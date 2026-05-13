#pragma once
#include "dui_world.h"
#include <functional>
#include <cstdint>

namespace dui {

// Callback types for rendering custom fields in the Inspector detail pane.
// The drawer runs inside an active ImGui window — use any ImGui widgets freely.
using EntityDrawer = std::function<void(Entity&)>;
using CellDrawer   = std::function<void(Cell&)>;

// Register a drawer for a specific entity/cell type (overwrites any previous one).
void RegisterEntityDrawer(uint8_t type, EntityDrawer drawer);
void RegisterCellDrawer  (uint8_t type, CellDrawer   drawer);

// Called by the Inspector after rendering built-in fields.
void InvokeEntityDrawer(Entity& e);
void InvokeCellDrawer  (Cell& c);

} // namespace dui
