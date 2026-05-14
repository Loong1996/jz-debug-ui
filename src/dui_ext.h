#pragma once
#include "dui_world.h"
#include <functional>
#include <cstdint>
#include <string>

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

// Register a display name for a map id value.
void RegisterMapName(uint32_t map_id, const char* name);
const char* GetMapName(uint32_t map_id);   // nullptr if not registered

// Switch the active map: updates active_map_id, clears cell selection,
// and sets selected_id to the first entity on the new map (or -1 if none).
// Camera reset is handled by DrawCanvas on the next frame.
void SwitchActiveMap(World& w, uint32_t new_map_id);

// Called by the Inspector after rendering built-in fields.
void InvokeEntityDrawer(Entity& e);
void InvokeCellDrawer  (Cell& c);

// Register a function that returns the short text shown above an entity on the Canvas.
// Returning an empty string hides the label. Unregistered types fall back to e.label.
using EntityLabelFn = std::function<std::string(const Entity&)>;
void        RegisterEntityLabelFn(uint8_t type, EntityLabelFn fn);
std::string InvokeEntityLabel    (const Entity& e);

} // namespace dui
