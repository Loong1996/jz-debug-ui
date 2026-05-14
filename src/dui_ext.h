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

// Register a display name for an entity/cell type value.
// The Inspector will show the name wherever it previously showed the raw number.
// Affects: type column, filter combo, group headers, detail pane, and search.
void RegisterEntityTypeName(uint8_t type, const char* name);
void RegisterCellTypeName  (uint8_t type, const char* name);

// Returns the registered name, or nullptr if none was registered.
const char* GetEntityTypeName(uint8_t type);
const char* GetCellTypeName  (uint8_t type);

// Called by the Inspector after rendering built-in fields.
void InvokeEntityDrawer(Entity& e);
void InvokeCellDrawer  (Cell& c);

} // namespace dui
