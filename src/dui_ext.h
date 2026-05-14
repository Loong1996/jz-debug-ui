#pragma once
#include "dui_world.h"
#include <functional>
#include <cstdint>
#include <string>

struct ImDrawList;
struct ImVec2;

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

// Mark a entity type as "player" — Canvas draws a yellow triangle above all matching entities.
void SetPlayerEntityType(uint8_t type);
bool IsPlayerEntityType (uint8_t type);

// Per-entity marker: draw a downward triangle above a specific entity instance.
// color is ImU32 (IM_COL32). Call ClearEntityMarker to remove.
// Per-entity marker takes precedence over the player-type marker if both apply.
void        SetEntityMarker  (uint64_t entity_id, uint32_t color);
void        ClearEntityMarker(uint64_t entity_id);
// Returns nullptr if no per-entity marker is registered.
const uint32_t* GetEntityMarker(uint64_t entity_id);

// Register a function that returns the short text shown above an entity on the Canvas.
// Returning an empty string hides the label. Unregistered types fall back to e.label.
using EntityLabelFn = std::function<std::string(const Entity&)>;
void        RegisterEntityLabelFn(uint8_t type, EntityLabelFn fn);
std::string InvokeEntityLabel    (const Entity& e);

// ---- Canvas Overlay API ----
// Callbacks are invoked during DrawCanvas while the clip rect is active.
// ToScreen converts world-space coordinates to screen pixels.

struct CanvasOverlayCtx {
    ImVec2 (*ToScreen)(float wx, float wy);  // reads thread_local view state set by DrawCanvas
    ImDrawList* dl;
};

using EntityOverlayFn = std::function<void(const Entity&, const CanvasOverlayCtx&)>;
using GlobalOverlayFn = std::function<void(const World&,  const CanvasOverlayCtx&)>;

// Per-type overlay: called once per entity of that type each frame.
void RegisterEntityOverlay(uint8_t type, EntityOverlayFn fn);

// World-level overlay: called once per frame regardless of entity type.
// name is used for deduplication (registering again with same name overwrites).
void RegisterGlobalOverlay  (const char* name, GlobalOverlayFn fn);
void UnregisterGlobalOverlay(const char* name);

// Internal — called by DrawCanvas.
void SetCanvasViewState_(float fcx, float fcy, float th, ImVec2 center);
ImVec2 CanvasToScreen_(float wx, float wy);
void InvokeEntityOverlays_(const World& world, const Entity& e, ImDrawList* dl);
void InvokeGlobalOverlays_(const World& world, ImDrawList* dl);

} // namespace dui
