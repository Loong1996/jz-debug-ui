#pragma once
#include "dui_world.h"
#include <functional>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
#include <imgui.h>

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

// Bulk registration overloads — equivalent to calling the single-item versions in order.
// Usage: RegisterEntityTypeNames({{0, u8"主角"}, {1, u8"战士"}, {2, u8"法师"}});
void RegisterEntityTypeNames(std::initializer_list<std::pair<uint8_t,  const char*>> entries);
void RegisterCellTypeNames  (std::initializer_list<std::pair<uint8_t,  const char*>> entries);
void RegisterMapNames       (std::initializer_list<std::pair<uint32_t, const char*>> entries);

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

// ---- Cell Overlay API ----
// Mirrors RegisterEntityOverlay: per cell-type callback drawn after cells are filled.

using CellOverlayFn = std::function<void(const Cell&, const CanvasOverlayCtx&)>;

void RegisterCellOverlay(uint8_t type, CellOverlayFn fn);

// ---- Cell Heatmap API ----
// Overlays a color gradient on every tile in the visible viewport,
// including tiles where no Cell object exists.
// fn receives tile world-coordinates (x, y); return a float scalar.

struct HeatmapOpts {
    float    min_value  = 0.f;
    float    max_value  = 1.f;
    uint32_t low_color  = RGBA(  0,  80, 200, 100);
    uint32_t high_color = RGBA(220,  40,  40, 180);
    bool     auto_range = false; // normalize against visible viewport min/max each frame
};

using CellValueFn = std::function<float(int x, int y)>;

void RegisterCellHeatmap  (const char* name, CellValueFn fn, HeatmapOpts opts = {});
void UnregisterCellHeatmap(const char* name);

// ---- Entity Links API ----
// Draw directed lines between entities (e.g. target, threat, leader→follower).
// Multiple named link registrations can coexist.

struct EntityLink {
    uint64_t target_id;
    uint32_t color     = RGBA(255, 200, 0, 180);
    float    thickness = 1.5f;
    bool     arrow     = true;
    bool     dashed    = false; // draw as dashed line instead of solid
};

using EntityLinksFn = std::function<std::vector<EntityLink>(const Entity&)>;

void RegisterEntityLinks  (const char* name, EntityLinksFn fn);
void UnregisterEntityLinks(const char* name);

// Internal — called by DrawCanvas.
void  SetCanvasViewState_(float fcx, float fcy, float th, ImVec2 center);
ImVec2 CanvasToScreen_(float wx, float wy);
float  CanvasTileHalf_();  // current tile half-width in screen pixels
void InvokeEntityOverlays_(const World& world, const Entity& e, ImDrawList* dl);
void InvokeGlobalOverlays_(const World& world, ImDrawList* dl);
void InvokeCellOverlays_  (const World& world, const Cell& c, ImDrawList* dl);
// ccx/ccy: camera center tile coords; view_half: tiles visible in each direction
void InvokeCellHeatmaps_  (const World& world, ImDrawList* dl,
                            int ccx, int ccy, int view_half);
void InvokeEntityLinks_   (const World& world, ImDrawList* dl);

// ---- User Panel API ----
// RegisterPanel lets users add ImGui windows into the existing dock layout.
// draw_fn is called every frame inside Begin/End — write only ImGui content.
// window_title is also used as the DockBuilder key; keep it stable.
// RegisterPanel must be called before the first App::Tick so the dock layout
// includes the panel. Panels registered later appear floating and can be dragged.

enum class PanelDock { Center, Left, Right, Bottom, Floating };

using PanelDrawFn = std::function<void()>;

void RegisterPanel  (const char* window_title, PanelDrawFn draw_fn,
                     PanelDock initial_dock = PanelDock::Bottom);
void UnregisterPanel(const char* window_title);

void InvokeUserPanels_();                                 // called by DrawAll
void DockUserPanels_(ImGuiID center, ImGuiID left,
                     ImGuiID right,  ImGuiID bottom);      // called by ApplyBuiltinLayout

// ---- Multi-selection helpers ----
// World::selected_ids is the multi-selection set; selected_id is the primary
// (used by the Inspector detail panel and follow-camera). Use these helpers to
// keep both fields consistent.

// Add id to selected_ids. If set_primary, also updates selected_id.
void SelectAdd   (World& w, uint64_t id, bool set_primary = true);
// Remove id. If it was primary, promotes another id (or -1 if empty).
void SelectRemove(World& w, uint64_t id);
void SelectToggle(World& w, uint64_t id);
void SelectClear (World& w);
bool IsSelected  (const World& w, uint64_t id);

} // namespace dui
