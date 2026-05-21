#pragma once
#include "dui_world.h"
#include <functional>
#include <cmath>
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
    ImVec2 (*ToScreen)(float wx, float wy); // reads thread_local view state set by DrawCanvas
    ImDrawList* dl;
    float tile_half; // screen pixels per world unit; set each frame by DrawCanvas

    // World-space drawing helpers. Coordinates and radii are in world units.
    void DrawCircle(float cx, float cy, float r_world, uint32_t col, float thickness = 1.5f) const {
        dl->AddCircle(ToScreen(cx, cy), r_world * tile_half, col, 24, thickness);
    }
    void DrawCircleFilled(float cx, float cy, float r_world, uint32_t col) const {
        dl->AddCircleFilled(ToScreen(cx, cy), r_world * tile_half, col, 24);
    }
    void DrawLine(float x0, float y0, float x1, float y1, uint32_t col, float thickness = 1.5f) const {
        dl->AddLine(ToScreen(x0, y0), ToScreen(x1, y1), col, thickness);
    }
    void DrawRect(float x0, float y0, float x1, float y1, uint32_t col, float thickness = 1.5f) const {
        ImVec2 p00 = ToScreen(x0, y0), p10 = ToScreen(x1, y0);
        ImVec2 p11 = ToScreen(x1, y1), p01 = ToScreen(x0, y1);
        dl->AddQuad(p00, p10, p11, p01, col, thickness);
    }
    void DrawRectFilled(float x0, float y0, float x1, float y1, uint32_t col) const {
        ImVec2 p00 = ToScreen(x0, y0), p10 = ToScreen(x1, y0);
        ImVec2 p11 = ToScreen(x1, y1), p01 = ToScreen(x0, y1);
        dl->AddQuadFilled(p00, p10, p11, p01, col);
    }
    void DrawArrow(float x0, float y0, float x1, float y1, uint32_t col,
                   float thickness = 1.5f, float head_px = 9.f) const {
        ImVec2 from = ToScreen(x0, y0), to = ToScreen(x1, y1);
        dl->AddLine(from, to, col, thickness);
        float dx = to.x - from.x, dy = to.y - from.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > head_px) {
            float nx = dx / len, ny = dy / len, hf = head_px * 0.45f;
            float px = -ny * hf, py = nx * hf;
            dl->AddTriangleFilled(to,
                ImVec2(to.x - nx * head_px + px, to.y - ny * head_px + py),
                ImVec2(to.x - nx * head_px - px, to.y - ny * head_px - py), col);
        }
    }
    // DrawCone: filled sector. (cx,cy) = apex, (dx,dy) = direction (any magnitude),
    // r_world = radius, half_angle_rad = half-aperture.
    void DrawCone(float cx, float cy, float dx, float dy,
                  float r_world, float half_angle_rad, uint32_t col) const {
        ImVec2 cen = ToScreen(cx, cy);
        ImVec2 tip = ToScreen(cx + dx * r_world, cy + dy * r_world);
        float sdx = tip.x - cen.x, sdy = tip.y - cen.y;
        float slen = sqrtf(sdx * sdx + sdy * sdy);
        if (slen < 0.1f) return;
        float ang = atan2f(sdy, sdx);
        float rpx = r_world * tile_half;
        const int N = 16;
        ImVec2 pts[N + 2];
        pts[0] = cen;
        for (int i = 0; i <= N; ++i) {
            float a = ang - half_angle_rad + half_angle_rad * 2.f * i / N;
            pts[1 + i] = ImVec2(cen.x + cosf(a) * rpx, cen.y + sinf(a) * rpx);
        }
        dl->AddConvexPolyFilled(pts, N + 2, col);
    }
    void DrawTextWorld(float wx, float wy, uint32_t col, const char* text) const {
        if (text) dl->AddText(ToScreen(wx, wy), col, text);
    }
    // world_pts: array of world-space ImVec2 coordinates
    void DrawPolyline(const ImVec2* world_pts, int n, uint32_t col,
                      float thickness = 1.5f, bool closed = false) const {
        for (int i = 1; i < n; ++i)
            dl->AddLine(ToScreen(world_pts[i-1].x, world_pts[i-1].y),
                        ToScreen(world_pts[i].x,   world_pts[i].y), col, thickness);
        if (closed && n >= 3)
            dl->AddLine(ToScreen(world_pts[n-1].x, world_pts[n-1].y),
                        ToScreen(world_pts[0].x,   world_pts[0].y), col, thickness);
    }
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

// ---- Cell Links API ----
// Symmetric to RegisterEntityLinks: draw directed connectors between cells.
struct CellLink {
    int      target_x, target_y;
    uint32_t color     = RGBA(100, 200, 100, 180);
    float    thickness = 1.5f;
    bool     arrow     = true;
    bool     dashed    = false;
};
using CellLinksFn = std::function<std::vector<CellLink>(const Cell&)>;
void RegisterCellLinks  (const char* name, CellLinksFn fn);
void UnregisterCellLinks(const char* name);

// ---- Canvas Context Menu API ----
// Right-click on an entity/cell/background opens a registered context menu.
// Inside the callback: call ImGui::MenuItem / ImGui::Separator etc. freely.
struct CanvasContextCtx {
    World*   world;
    ImVec2   click_screen; // screen coords of right-click
    float    wx, wy;       // world tile coords at click position
    uint32_t map_id;
};
using EntityContextMenuFn = std::function<void(Entity&, const CanvasContextCtx&)>;
using CellContextMenuFn   = std::function<void(Cell&,   const CanvasContextCtx&)>;
using BgContextMenuFn     = std::function<void(float wx, float wy)>;

void RegisterEntityContextMenu(uint8_t type, EntityContextMenuFn fn);
void RegisterCellContextMenu  (uint8_t type, CellContextMenuFn   fn);
void RegisterCanvasBackgroundContextMenu(BgContextMenuFn fn);
void UnregisterEntityContextMenu(uint8_t type);
void UnregisterCellContextMenu  (uint8_t type);
void UnregisterCanvasBackgroundContextMenu();

// ---- World modification helpers ----
// DespawnEntity removes the entity, cleans up selection, trails, and per-entity marker.
// SpawnEntityAt creates a new entity at (fx, fy) on the current active map.
bool    DespawnEntity(World& w, uint64_t id);
Entity& SpawnEntityAt(World& w, float fx, float fy, uint8_t type,
                      const char* label = nullptr);

// Cell equivalents — DespawnCell also clears the selected-cell highlight.
bool  DespawnCell(World& w, uint32_t map_id, int x, int y);
Cell& SpawnCellAt(World& w, uint32_t map_id, int x, int y, uint8_t type,
                  const char* label = nullptr);

// Cell label function — return value overrides c.label; empty string hides label.
// Falls back to c.label when no function is registered for the type.
using CellLabelFn = std::function<std::string(const Cell&)>;
void        RegisterCellLabelFn(uint8_t type, CellLabelFn fn);
std::string InvokeCellLabel    (const Cell& c);

// Internal — called by DrawCanvas.
void  SetCanvasViewState_(float fcx, float fcy, float th, ImVec2 center);
ImVec2 CanvasToScreen_(float wx, float wy);
float  CanvasTileHalf_();
void InvokeEntityOverlays_(const World& world, const Entity& e, ImDrawList* dl);
void InvokeGlobalOverlays_(const World& world, ImDrawList* dl);
void InvokeCellOverlays_  (const World& world, const Cell& c, ImDrawList* dl);
// ccx/ccy: camera center tile coords; view_half: tiles visible in each direction
void InvokeCellHeatmaps_  (const World& world, ImDrawList* dl,
                            int ccx, int ccy, int view_half);
void InvokeEntityLinks_   (const World& world, ImDrawList* dl);
void InvokeCellLinks_     (const World& world, ImDrawList* dl);
// Returns true if any items were added to the open popup.
bool InvokeEntityContextMenu_(Entity& e, const CanvasContextCtx& ctx);
bool InvokeCellContextMenu_  (Cell& c,   const CanvasContextCtx& ctx);
bool InvokeBgContextMenu_    (const CanvasContextCtx& ctx);
bool HasEntityContextMenu_   (uint8_t type);
bool HasCellContextMenu_     (uint8_t type);
bool HasBgContextMenu_       ();

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

// Iterate over every selected entity on the active map and call fn.
// Handles the id→entity lookup internally — no double loop needed.
void ForEachSelected     (World& w,       std::function<void(Entity&)>       fn);
void ForEachSelected     (const World& w, std::function<void(const Entity&)> fn);

// Iterate over every cell at the selected cell position (sel_cell_valid must be true).
// No-op if no cell is selected.
void ForEachSelectedCell (World& w,       std::function<void(Cell&)>         fn);
void ForEachSelectedCell (const World& w, std::function<void(const Cell&)>   fn);

// ---- Layer visibility ----
// Enumerate all registered overlays/heatmaps/links with their current enabled state.
// kind is one of: "GlobalOverlay", "Heatmap", "EntityLinks", "CellLinks",
//                 "EntityOverlay", "CellOverlay"
struct LayerInfo { const char* kind; const char* name; bool enabled; };
void ListLayers     (std::vector<LayerInfo>& out);
void SetLayerEnabled(const char* kind, const char* name, bool on);

// ---- Simple entity link (single-target shorthand) ----
// fn returns target entity id; return 0 to draw no link from this entity.
// Wraps RegisterEntityLinks internally — no vector allocation in your code.
using EntityTargetFn = std::function<uint64_t(const Entity&)>;
void RegisterEntityLink(const char* name, EntityTargetFn fn,
                        uint32_t color     = RGBA(255, 200, 0, 180),
                        float    thickness = 1.5f,
                        bool     dashed    = true,
                        bool     arrow     = true);

// ---- Static entity links (direct id-pair) ----
// Draw a line from entity from_id to entity to_id. Persists until removed.
// Calling Add again with the same (from_id, to_id) updates the style in-place.
// ClearEntityLinksById() removes all static links — call at frame start if
// you rebuild the set every tick.
void AddEntityLinkById   (uint64_t from_id, uint64_t to_id,
                          uint32_t color     = RGBA(255, 200, 0, 180),
                          float    thickness = 1.5f,
                          bool     dashed    = true,
                          bool     arrow     = true);
void RemoveEntityLinkById(uint64_t from_id, uint64_t to_id);
void ClearEntityLinksById();

// ---- GBK / UTF-8 encoding utility ----
// Converts a GBK (CP936) string to UTF-8, writes at most out_size bytes.
// Returns true on success. On non-Windows platforms copies bytes as-is.
bool GbkToUtf8(const char* gbk, char* utf8_out, int out_size);

} // namespace dui
