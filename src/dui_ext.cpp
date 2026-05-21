#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "dui_ext.h"
#include "dui_trails.h"
#include "dui_entity_log.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    std::unordered_map<uint8_t,  dui::EntityDrawer>    g_entity_drawers;
    std::unordered_map<uint8_t,  dui::CellDrawer>      g_cell_drawers;
    std::unordered_map<uint8_t,  std::string>           g_entity_type_names;
    std::unordered_map<uint8_t,  uint32_t>              g_entity_type_colors;
    std::unordered_map<uint8_t,  std::string>           g_cell_type_names;
    std::unordered_map<uint32_t, std::string>           g_map_names;
    std::unordered_map<uint8_t,  dui::EntityLabelFn>    g_label_fns;
    std::unordered_map<uint64_t, dui::EntityMarkerData>  g_entity_markers;
    uint8_t g_player_entity_type = 255;
    bool    g_player_type_set    = false;

    // Overlay registries
    std::unordered_map<uint8_t, dui::EntityOverlayFn> g_entity_overlays;
    std::unordered_map<uint8_t, bool>                  g_entity_overlay_enabled;
    struct GlobalOverlay { std::string name; dui::GlobalOverlayFn fn; bool enabled = true; };
    std::vector<GlobalOverlay> g_global_overlays;

    // Cell overlay registry
    std::unordered_map<uint8_t, dui::CellOverlayFn> g_cell_overlays;
    std::unordered_map<uint8_t, bool>                g_cell_overlay_enabled;

    // Heatmap registry
    struct HeatmapEntry { std::string name; dui::CellValueFn fn; dui::HeatmapOpts opts; bool enabled = true; };
    std::vector<HeatmapEntry> g_heatmaps;

    // Entity links registry
    struct EntityLinksEntry { std::string name; dui::EntityLinksFn fn; bool enabled = true; };
    std::vector<EntityLinksEntry> g_entity_links;

    // Static (id-pair) entity links — drawn every frame until removed
    struct StaticEntityLink {
        uint64_t from_id, to_id;
        uint32_t color;
        float    thickness;
        bool     dashed, arrow;
    };
    std::vector<StaticEntityLink> g_static_entity_links;
    bool g_static_links_enabled = true;
    static const char* kStaticLinksLayerName = u8"静态连线";

    // Cell links registry
    struct CellLinksEntry { std::string name; dui::CellLinksFn fn; bool enabled = true; };
    std::vector<CellLinksEntry> g_cell_links;

    // Context menu registries
    std::unordered_map<uint8_t, dui::EntityContextMenuFn> g_entity_ctx_menus;
    std::unordered_map<uint8_t, dui::CellContextMenuFn>   g_cell_ctx_menus;
    dui::BgContextMenuFn g_bg_ctx_menu;

    // User panel registry
    struct PanelEntry { std::string title; dui::PanelDrawFn fn; dui::PanelDock dock; };
    std::vector<PanelEntry> g_panels;

    // Thread-local canvas view state (set each frame by DrawCanvas)
    struct CanvasViewState { float fcx, fcy, th; ImVec2 center; };
    thread_local CanvasViewState s_cv_view = {};

    // Internal helper: draw a solid or dashed line from→to on dl.
    static void DrawLine_(ImDrawList* dl, ImVec2 from, ImVec2 to,
                          uint32_t color, float thickness, bool dashed) {
        if (!dashed) {
            dl->AddLine(from, to, color, thickness);
            return;
        }
        float dx = to.x - from.x, dy = to.y - from.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1.f) return;
        float nx = dx / len, ny = dy / len;
        const float dash = 6.f, gap = 4.f, step = dash + gap;
        float t = 0.f;
        while (t < len) {
            float t1 = t + dash < len ? t + dash : len;
            dl->AddLine(
                ImVec2(from.x + nx * t,  from.y + ny * t),
                ImVec2(from.x + nx * t1, from.y + ny * t1),
                color, thickness);
            t += step;
        }
    }

    // Internal helper: draw an arrow (solid or dashed) from→to on dl.
    static void DrawArrow_(ImDrawList* dl, ImVec2 from, ImVec2 to,
                           uint32_t color, float thickness, bool dashed) {
        DrawLine_(dl, from, to, color, thickness, dashed);
        float dx = to.x - from.x, dy = to.y - from.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 8.f) {
            float nx = dx / len, ny = dy / len;
            float px = -ny * 4.f, py = nx * 4.f;
            dl->AddTriangleFilled(
                to,
                ImVec2(to.x - nx * 9.f + px, to.y - ny * 9.f + py),
                ImVec2(to.x - nx * 9.f - px, to.y - ny * 9.f - py),
                color);
        }
    }

    // Lerp a single byte channel
    static uint8_t LerpU8(uint8_t a, uint8_t b, float t) {
        return static_cast<uint8_t>(a + (b - a) * t);
    }
    static uint32_t LerpColor(uint32_t lo, uint32_t hi, float t) {
        return IM_COL32(
            LerpU8(lo & 0xFF,        hi & 0xFF,        t),
            LerpU8((lo >> 8)  & 0xFF, (hi >> 8)  & 0xFF, t),
            LerpU8((lo >> 16) & 0xFF, (hi >> 16) & 0xFF, t),
            LerpU8((lo >> 24) & 0xFF, (hi >> 24) & 0xFF, t));
    }
}

namespace dui {

void RegisterEntityDrawer  (uint8_t type, EntityDrawer drawer) { g_entity_drawers[type] = std::move(drawer); }
void UnregisterEntityDrawer(uint8_t type)                      { g_entity_drawers.erase(type); }
void ClearEntityDrawers   ()                                   { g_entity_drawers.clear(); }
void RegisterCellDrawer    (uint8_t type, CellDrawer   drawer) { g_cell_drawers[type]   = std::move(drawer); }
void UnregisterCellDrawer  (uint8_t type)                      { g_cell_drawers.erase(type); }
void ClearCellDrawers      ()                                  { g_cell_drawers.clear(); }

void RegisterEntityTypeName  (uint8_t type, const char* name) { g_entity_type_names[type] = name ? name : ""; }
void UnregisterEntityTypeName(uint8_t type)                   { g_entity_type_names.erase(type); }
void ClearEntityTypeNames    ()                               { g_entity_type_names.clear(); }

void RegisterEntityTypeColor  (uint8_t type, uint32_t color) { g_entity_type_colors[type] = color; }
void UnregisterEntityTypeColor(uint8_t type)                 { g_entity_type_colors.erase(type); }
void ClearEntityTypeColors    ()                             { g_entity_type_colors.clear(); }

uint32_t GetEntityTypeColor(uint8_t type, uint32_t fallback) {
    auto it = g_entity_type_colors.find(type);
    return it != g_entity_type_colors.end() ? it->second : fallback;
}

uint32_t ResolveEntityColor(const Entity& e) {
    static const uint32_t kDefault = RGBA(180, 180, 180); // matches Entity::color default
    if (e.color != kDefault) return e.color;              // user explicitly set a color
    auto it = g_entity_type_colors.find(e.type);
    return it != g_entity_type_colors.end() ? it->second : e.color;
}
void RegisterCellTypeName  (uint8_t type, const char* name) { g_cell_type_names[type] = name ? name : ""; }
void UnregisterCellTypeName(uint8_t type)                   { g_cell_type_names.erase(type); }
void ClearCellTypeNames    ()                               { g_cell_type_names.clear(); }
const char* GetEntityTypeName(uint8_t type) {
    auto it = g_entity_type_names.find(type);
    return it != g_entity_type_names.end() ? it->second.c_str() : nullptr;
}
const char* GetCellTypeName(uint8_t type) {
    auto it = g_cell_type_names.find(type);
    return it != g_cell_type_names.end() ? it->second.c_str() : nullptr;
}

void RegisterMapName  (uint32_t map_id, const char* name) { g_map_names[map_id] = name ? name : ""; }
void UnregisterMapName(uint32_t map_id)                   { g_map_names.erase(map_id); }
void ClearMapNames    ()                                  { g_map_names.clear(); }

void RegisterEntityTypeNames(std::initializer_list<std::pair<uint8_t, const char*>> entries) {
    for (const auto& p : entries) RegisterEntityTypeName(p.first, p.second);
}
void RegisterCellTypeNames(std::initializer_list<std::pair<uint8_t, const char*>> entries) {
    for (const auto& p : entries) RegisterCellTypeName(p.first, p.second);
}
void RegisterMapNames(std::initializer_list<std::pair<uint32_t, const char*>> entries) {
    for (const auto& p : entries) RegisterMapName(p.first, p.second);
}
const char* GetMapName(uint32_t map_id) {
    auto it = g_map_names.find(map_id);
    return it != g_map_names.end() ? it->second.c_str() : nullptr;
}
void SwitchActiveMap(World& w, uint32_t new_map_id) {
    w.active_map_id  = new_map_id;
    w.sel_cell_valid = false;
    w.selected_id    = 0;
    w.selected_ids.clear();
    for (const auto& e : w.entities) {
        if (e.map_id == new_map_id) {
            w.selected_id = e.id;
            w.selected_ids.push_back(e.id);
            break;
        }
    }
}

void InvokeEntityDrawer(Entity& e) {
    auto it = g_entity_drawers.find(e.type);
    if (it == g_entity_drawers.end()) return;
    ImGui::Separator();
    it->second(e);
}

void InvokeCellDrawer(Cell& c) {
    auto it = g_cell_drawers.find(c.type);
    if (it == g_cell_drawers.end()) return;
    ImGui::Separator();
    it->second(c);
}

void SetPlayerEntityType  (uint8_t type) { g_player_entity_type = type; g_player_type_set = true; }
void ClearPlayerEntityType()            { g_player_type_set = false; }
bool IsPlayerEntityType   (uint8_t type) { return g_player_type_set && type == g_player_entity_type; }

void RegisterEntityLabelFn  (uint8_t type, EntityLabelFn fn) { g_label_fns[type] = std::move(fn); }
void UnregisterEntityLabelFn(uint8_t type)                   { g_label_fns.erase(type); }
void ClearEntityLabelFns   ()                                { g_label_fns.clear(); }

std::string InvokeEntityLabel(const Entity& e) {
    auto it = g_label_fns.find(e.type);
    if (it != g_label_fns.end()) return it->second(e);
    return e.label;
}

void SetEntityMarker(uint64_t entity_id, uint32_t color, MarkerShape shape) {
    g_entity_markers[entity_id] = { color, shape };
}
void ClearEntityMarker   (uint64_t entity_id) { g_entity_markers.erase(entity_id); }
void ClearAllEntityMarkers()                  { g_entity_markers.clear(); }
const EntityMarkerData* GetEntityMarker(uint64_t entity_id) {
    auto it = g_entity_markers.find(entity_id);
    return it != g_entity_markers.end() ? &it->second : nullptr;
}

void RegisterEntityOverlay  (uint8_t type, EntityOverlayFn fn) { g_entity_overlays[type] = std::move(fn); }
void UnregisterEntityOverlay(uint8_t type)                     { g_entity_overlays.erase(type); g_entity_overlay_enabled.erase(type); }
void ClearEntityOverlays   ()                                  { g_entity_overlays.clear(); g_entity_overlay_enabled.clear(); }

void RegisterGlobalOverlay(const char* name, GlobalOverlayFn fn) {
    if (!name) return;
    for (auto& g : g_global_overlays) {
        if (g.name == name) { g.fn = std::move(fn); return; }
    }
    g_global_overlays.push_back({ std::string(name), std::move(fn) });
}

void ClearGlobalOverlays() { g_global_overlays.clear(); }
void UnregisterGlobalOverlay(const char* name) {
    if (!name) return;
    for (auto it = g_global_overlays.begin(); it != g_global_overlays.end(); ++it) {
        if (it->name == name) { g_global_overlays.erase(it); return; }
    }
}

void SetCanvasViewState_(float fcx, float fcy, float th, ImVec2 center) {
    s_cv_view = { fcx, fcy, th, center };
}

ImVec2 CanvasToScreen_(float wx, float wy) {
    float dx = wx - s_cv_view.fcx, dy = wy - s_cv_view.fcy;
    return ImVec2(s_cv_view.center.x + (dx - dy) * s_cv_view.th,
                  s_cv_view.center.y + (dx + dy) * s_cv_view.th);
}

float CanvasTileHalf_() { return s_cv_view.th; }

void InvokeEntityOverlays_(const World& world, const Entity& e, ImDrawList* dl) {
    auto it = g_entity_overlays.find(e.type);
    if (it == g_entity_overlays.end()) return;
    auto ei = g_entity_overlay_enabled.find(e.type);
    if (ei != g_entity_overlay_enabled.end() && !ei->second) return;
    CanvasOverlayCtx ctx{ CanvasToScreen_, dl, s_cv_view.th };
    it->second(e, ctx);
}

void InvokeGlobalOverlays_(const World& world, ImDrawList* dl) {
    CanvasOverlayCtx ctx{ CanvasToScreen_, dl, s_cv_view.th };
    for (const auto& g : g_global_overlays)
        if (g.fn && g.enabled) g.fn(world, ctx);
}

// ---- Cell overlay ----

void RegisterCellOverlay  (uint8_t type, CellOverlayFn fn) { g_cell_overlays[type] = std::move(fn); }
void UnregisterCellOverlay(uint8_t type)                    { g_cell_overlays.erase(type); g_cell_overlay_enabled.erase(type); }
void ClearCellOverlays   ()                                 { g_cell_overlays.clear(); g_cell_overlay_enabled.clear(); }

void InvokeCellOverlays_(const World& world, const Cell& c, ImDrawList* dl) {
    auto it = g_cell_overlays.find(c.type);
    if (it == g_cell_overlays.end()) return;
    auto ei = g_cell_overlay_enabled.find(c.type);
    if (ei != g_cell_overlay_enabled.end() && !ei->second) return;
    CanvasOverlayCtx ctx{ CanvasToScreen_, dl, s_cv_view.th };
    it->second(c, ctx);
}

// ---- Heatmap ----

void RegisterCellHeatmap(const char* name, CellValueFn fn, HeatmapOpts opts) {
    if (!name) return;
    for (auto& h : g_heatmaps) {
        if (h.name == name) { h.fn = std::move(fn); h.opts = opts; return; }
    }
    g_heatmaps.push_back({ std::string(name), std::move(fn), opts });
}

void ClearCellHeatmaps() { g_heatmaps.clear(); }
void UnregisterCellHeatmap(const char* name) {
    if (!name) return;
    for (auto it = g_heatmaps.begin(); it != g_heatmaps.end(); ++it) {
        if (it->name == name) { g_heatmaps.erase(it); return; }
    }
}

void InvokeCellHeatmaps_(const World& world, ImDrawList* dl,
                         int ccx, int ccy, int view_half) {
    if (g_heatmaps.empty()) return;
    float th = CanvasTileHalf_();

    for (const auto& h : g_heatmaps) {
        if (!h.fn || !h.enabled) continue;
        float lo = h.opts.min_value, hi = h.opts.max_value;
        if (h.opts.auto_range) {
            lo =  1e30f; hi = -1e30f;
            for (int iy = ccy - view_half; iy <= ccy + view_half; ++iy)
                for (int ix = ccx - view_half; ix <= ccx + view_half; ++ix) {
                    float v = h.fn(ix, iy);
                    if (v < lo) lo = v;
                    if (v > hi) hi = v;
                }
            if (hi <= lo) { lo = h.opts.min_value; hi = h.opts.max_value; }
        }
        float range = hi - lo;
        if (range < 1e-6f) range = 1e-6f;

        for (int iy = ccy - view_half; iy <= ccy + view_half; ++iy) {
            for (int ix = ccx - view_half; ix <= ccx + view_half; ++ix) {
                float t = (h.fn(ix, iy) - lo) / range;
                if (t < 0.f) t = 0.f; if (t > 1.f) t = 1.f;
                uint32_t col = LerpColor(h.opts.low_color, h.opts.high_color, t);
                ImVec2 cen = CanvasToScreen_(static_cast<float>(ix), static_cast<float>(iy));
                dl->AddQuadFilled(
                    ImVec2(cen.x,      cen.y - th),
                    ImVec2(cen.x + th, cen.y),
                    ImVec2(cen.x,      cen.y + th),
                    ImVec2(cen.x - th, cen.y),
                    col);
            }
        }
    }
}

// ---- Entity links ----

void RegisterEntityLinks(const char* name, EntityLinksFn fn) {
    if (!name) return;
    for (auto& l : g_entity_links) {
        if (l.name == name) { l.fn = std::move(fn); return; }
    }
    g_entity_links.push_back({ std::string(name), std::move(fn) });
}

void ClearEntityLinks() { g_entity_links.clear(); }
void UnregisterEntityLinks(const char* name) {
    if (!name) return;
    for (auto it = g_entity_links.begin(); it != g_entity_links.end(); ++it) {
        if (it->name == name) { g_entity_links.erase(it); return; }
    }
}

void InvokeEntityLinks_(const World& world, ImDrawList* dl) {
    if (g_entity_links.empty() && g_static_entity_links.empty()) return;

    // Build id→entity index for the active map (shared by both link types)
    std::unordered_map<uint64_t, const Entity*> idx;
    idx.reserve(world.entities.size());
    for (const auto& e : world.entities)
        if (e.map_id == world.active_map_id) idx[e.id] = &e;

    // Callback-based links
    for (const auto& e : world.entities) {
        if (e.map_id != world.active_map_id) continue;
        for (const auto& entry : g_entity_links) {
            if (!entry.fn || !entry.enabled) continue;
            std::vector<EntityLink> links = entry.fn(e);
            for (const auto& lk : links) {
                auto tit = idx.find(lk.target_id);
                if (tit == idx.end()) continue;
                ImVec2 from = CanvasToScreen_(e.fx, e.fy);
                ImVec2 to   = CanvasToScreen_(tit->second->fx, tit->second->fy);
                if (lk.arrow) DrawArrow_(dl, from, to, lk.color, lk.thickness, lk.dashed);
                else          DrawLine_ (dl, from, to, lk.color, lk.thickness, lk.dashed);
            }
        }
    }

    // Static (id-pair) links
    if (!g_static_links_enabled) return;
    for (const auto& sl : g_static_entity_links) {
        auto fit = idx.find(sl.from_id);
        auto tit = idx.find(sl.to_id);
        if (fit == idx.end() || tit == idx.end()) continue;
        ImVec2 from = CanvasToScreen_(fit->second->fx, fit->second->fy);
        ImVec2 to   = CanvasToScreen_(tit->second->fx, tit->second->fy);
        if (sl.arrow) DrawArrow_(dl, from, to, sl.color, sl.thickness, sl.dashed);
        else          DrawLine_ (dl, from, to, sl.color, sl.thickness, sl.dashed);
    }
}

// ---- User Panels ----

void RegisterPanel(const char* window_title, PanelDrawFn draw_fn, PanelDock dock) {
    if (!window_title) return;
    for (auto& p : g_panels) {
        if (p.title == window_title) { p.fn = std::move(draw_fn); p.dock = dock; return; }
    }
    g_panels.push_back({ std::string(window_title), std::move(draw_fn), dock });
}

void ClearPanels() { g_panels.clear(); }
void UnregisterPanel(const char* window_title) {
    if (!window_title) return;
    for (auto it = g_panels.begin(); it != g_panels.end(); ++it) {
        if (it->title == window_title) { g_panels.erase(it); return; }
    }
}

void InvokeUserPanels_() {
    for (const auto& p : g_panels) {
        if (!p.fn) continue;
        ImGui::Begin(p.title.c_str());
        p.fn();
        ImGui::End();
    }
}

void DockUserPanels_(ImGuiID center, ImGuiID left, ImGuiID right, ImGuiID bottom) {
    for (const auto& p : g_panels) {
        ImGuiID slot = 0;
        switch (p.dock) {
            case PanelDock::Center:   slot = center; break;
            case PanelDock::Left:     slot = left;   break;
            case PanelDock::Right:    slot = right;  break;
            case PanelDock::Bottom:   slot = bottom; break;
            case PanelDock::Floating: slot = 0;      break;
        }
        if (slot) ImGui::DockBuilderDockWindow(p.title.c_str(), slot);
    }
}

// ---- Multi-selection ----

bool IsSelected(const World& w, uint64_t id) {
    for (auto eid : w.selected_ids)
        if (eid == id) return true;
    return false;
}

void SelectAdd(World& w, uint64_t id, bool set_primary) {
    if (!IsSelected(w, id)) w.selected_ids.push_back(id);
    if (set_primary) w.selected_id = id;
}

void SelectRemove(World& w, uint64_t id) {
    for (auto it = w.selected_ids.begin(); it != w.selected_ids.end(); ++it) {
        if (*it != id) continue;
        w.selected_ids.erase(it);
        if (id == w.selected_id)
            w.selected_id = w.selected_ids.empty() ? 0 : w.selected_ids.back();
        return;
    }
}

void SelectToggle(World& w, uint64_t id) {
    if (IsSelected(w, id)) SelectRemove(w, id);
    else                   SelectAdd(w, id);
}

void SelectClear(World& w) {
    w.selected_ids.clear();
    w.selected_id = 0;
}

void ForEachSelected(World& w, std::function<void(Entity&)> fn) {
    for (uint64_t id : w.selected_ids)
        for (auto& e : w.entities)
            if (e.id == id && e.map_id == w.active_map_id) { fn(e); break; }
}

void ForEachSelected(const World& w, std::function<void(const Entity&)> fn) {
    for (uint64_t id : w.selected_ids)
        for (const auto& e : w.entities)
            if (e.id == id && e.map_id == w.active_map_id) { fn(e); break; }
}

void ForEachSelectedCell(World& w, std::function<void(Cell&)> fn) {
    if (!w.sel_cell_valid) return;
    for (auto& c : w.cells)
        if (c.map_id == w.active_map_id && c.x == w.sel_cell_x && c.y == w.sel_cell_y)
            fn(c);
}

void ForEachSelectedCell(const World& w, std::function<void(const Cell&)> fn) {
    if (!w.sel_cell_valid) return;
    for (const auto& c : w.cells)
        if (c.map_id == w.active_map_id && c.x == w.sel_cell_x && c.y == w.sel_cell_y)
            fn(c);
}

// ---- Cell Links ----

void RegisterCellLinks(const char* name, CellLinksFn fn) {
    if (!name) return;
    for (auto& l : g_cell_links) {
        if (l.name == name) { l.fn = std::move(fn); return; }
    }
    g_cell_links.push_back({ std::string(name), std::move(fn) });
}

void ClearCellLinks() { g_cell_links.clear(); }
void UnregisterCellLinks(const char* name) {
    if (!name) return;
    for (auto it = g_cell_links.begin(); it != g_cell_links.end(); ++it) {
        if (it->name == name) { g_cell_links.erase(it); return; }
    }
}

void InvokeCellLinks_(const World& world, ImDrawList* dl) {
    if (g_cell_links.empty()) return;
    for (const auto& c : world.cells) {
        if (c.map_id != world.active_map_id) continue;
        for (const auto& entry : g_cell_links) {
            if (!entry.fn || !entry.enabled) continue;
            std::vector<CellLink> links = entry.fn(c);
            for (const auto& lk : links) {
                ImVec2 from = CanvasToScreen_(static_cast<float>(c.x), static_cast<float>(c.y));
                ImVec2 to   = CanvasToScreen_(static_cast<float>(lk.target_x), static_cast<float>(lk.target_y));
                if (lk.arrow) DrawArrow_(dl, from, to, lk.color, lk.thickness, lk.dashed);
                else          DrawLine_ (dl, from, to, lk.color, lk.thickness, lk.dashed);
            }
        }
    }
}

// ---- Layer visibility ----

void ListLayers(std::vector<LayerInfo>& out) {
    out.clear();
    for (const auto& g : g_global_overlays)
        out.push_back({ "GlobalOverlay", g.name.c_str(), g.enabled });
    for (const auto& h : g_heatmaps)
        out.push_back({ "Heatmap", h.name.c_str(), h.enabled });
    for (const auto& l : g_entity_links)
        out.push_back({ "EntityLinks", l.name.c_str(), l.enabled });
    if (!g_static_entity_links.empty())
        out.push_back({ "EntityLinks", kStaticLinksLayerName, g_static_links_enabled });
    for (const auto& l : g_cell_links)
        out.push_back({ "CellLinks", l.name.c_str(), l.enabled });
    for (const auto& kv : g_entity_overlays) {
        bool en = true;
        auto ei = g_entity_overlay_enabled.find(kv.first);
        if (ei != g_entity_overlay_enabled.end()) en = ei->second;
        const char* tn = GetEntityTypeName(kv.first);
        char nbuf[24];
        if (!tn) { std::snprintf(nbuf, sizeof(nbuf), "type %u", (unsigned)kv.first); tn = nbuf; }
        // LayerInfo.name must outlive the call — use a persistent string via static storage
        // We store the entity type index as key; name is ephemeral so callers should copy.
        out.push_back({ "EntityOverlay", tn, en });
    }
    for (const auto& kv : g_cell_overlays) {
        bool en = true;
        auto ei = g_cell_overlay_enabled.find(kv.first);
        if (ei != g_cell_overlay_enabled.end()) en = ei->second;
        const char* tn = GetCellTypeName(kv.first);
        char nbuf[24];
        if (!tn) { std::snprintf(nbuf, sizeof(nbuf), "type %u", (unsigned)kv.first); tn = nbuf; }
        out.push_back({ "CellOverlay", tn, en });
    }
}

void SetLayerEnabled(const char* kind, const char* name, bool on) {
    if (!kind || !name) return;
    if (std::strcmp(kind, "GlobalOverlay") == 0) {
        for (auto& g : g_global_overlays)
            if (g.name == name) { g.enabled = on; return; }
    } else if (std::strcmp(kind, "Heatmap") == 0) {
        for (auto& h : g_heatmaps)
            if (h.name == name) { h.enabled = on; return; }
    } else if (std::strcmp(kind, "EntityLinks") == 0) {
        if (std::strcmp(name, kStaticLinksLayerName) == 0) { g_static_links_enabled = on; return; }
        for (auto& l : g_entity_links)
            if (l.name == name) { l.enabled = on; return; }
    } else if (std::strcmp(kind, "CellLinks") == 0) {
        for (auto& l : g_cell_links)
            if (l.name == name) { l.enabled = on; return; }
    } else if (std::strcmp(kind, "EntityOverlay") == 0) {
        for (const auto& kv : g_entity_overlays) {
            const char* tn = GetEntityTypeName(kv.first);
            char nbuf[24];
            if (!tn) { std::snprintf(nbuf, sizeof(nbuf), "type %u", (unsigned)kv.first); tn = nbuf; }
            if (std::strcmp(tn, name) == 0) { g_entity_overlay_enabled[kv.first] = on; return; }
        }
    } else if (std::strcmp(kind, "CellOverlay") == 0) {
        for (const auto& kv : g_cell_overlays) {
            const char* tn = GetCellTypeName(kv.first);
            char nbuf[24];
            if (!tn) { std::snprintf(nbuf, sizeof(nbuf), "type %u", (unsigned)kv.first); tn = nbuf; }
            if (std::strcmp(tn, name) == 0) { g_cell_overlay_enabled[kv.first] = on; return; }
        }
    }
}

// ---- Context Menus ----

void RegisterEntityContextMenu(uint8_t type, EntityContextMenuFn fn) {
    g_entity_ctx_menus[type] = std::move(fn);
}
void RegisterCellContextMenu(uint8_t type, CellContextMenuFn fn) {
    g_cell_ctx_menus[type] = std::move(fn);
}
void RegisterCanvasBackgroundContextMenu(BgContextMenuFn fn) {
    g_bg_ctx_menu = std::move(fn);
}
void UnregisterEntityContextMenu(uint8_t type) { g_entity_ctx_menus.erase(type); }
void ClearEntityContextMenus   ()              { g_entity_ctx_menus.clear(); }
void UnregisterCellContextMenu  (uint8_t type) { g_cell_ctx_menus.erase(type); }
void ClearCellContextMenus     ()              { g_cell_ctx_menus.clear(); }
void UnregisterCanvasBackgroundContextMenu()   { g_bg_ctx_menu = nullptr; }

bool HasEntityContextMenu_(uint8_t type) { return g_entity_ctx_menus.count(type) > 0; }
bool HasCellContextMenu_  (uint8_t type) { return g_cell_ctx_menus.count(type) > 0; }
bool HasBgContextMenu_    ()             { return g_bg_ctx_menu != nullptr; }

bool InvokeEntityContextMenu_(Entity& e, const CanvasContextCtx& ctx) {
    auto it = g_entity_ctx_menus.find(e.type);
    if (it == g_entity_ctx_menus.end()) return false;
    it->second(e, ctx);
    return true;
}
bool InvokeCellContextMenu_(Cell& c, const CanvasContextCtx& ctx) {
    auto it = g_cell_ctx_menus.find(c.type);
    if (it == g_cell_ctx_menus.end()) return false;
    it->second(c, ctx);
    return true;
}
bool InvokeBgContextMenu_(const CanvasContextCtx& ctx) {
    if (!g_bg_ctx_menu) return false;
    g_bg_ctx_menu(ctx.wx, ctx.wy);
    return true;
}

// ---- World helpers ----

bool DespawnEntity(World& w, uint64_t id) {
    for (auto it = w.entities.begin(); it != w.entities.end(); ++it) {
        if (it->id != id) continue;
        w.entities.erase(it);
        SelectRemove(w, id);
        ClearEntityMarker(id);
        ClearTrailsFor(id);
        OnEntityDespawned_(id);
        return true;
    }
    return false;
}

Entity& SpawnEntityAt(World& w, float fx, float fy, uint8_t type, const char* label) {
    uint64_t new_id = 1;
    for (const auto& e : w.entities)
        if (e.id >= new_id) new_id = e.id + 1;
    Entity& e = w.SpawnEntity(new_id);
    e.SetPos(fx, fy).SetType(type).SetMapId(w.active_map_id);
    auto tc = g_entity_type_colors.find(type);
    if (tc != g_entity_type_colors.end()) e.color = tc->second;
    if (label) e.SetLabel("%s", label);
    return e;
}

// ---- Cell label functions ----

namespace { std::unordered_map<uint8_t, CellLabelFn> g_cell_label_fns; }

void RegisterCellLabelFn(uint8_t type, CellLabelFn fn) {
    g_cell_label_fns[type] = std::move(fn);
}
std::string InvokeCellLabel(const Cell& c) {
    auto it = g_cell_label_fns.find(c.type);
    if (it != g_cell_label_fns.end()) return it->second(c);
    return c.label[0] ? std::string(c.label) : std::string();
}

// ---- Cell spawn / despawn ----

bool DespawnCell(World& w, uint32_t map_id, int x, int y) {
    for (auto it = w.cells.begin(); it != w.cells.end(); ++it) {
        if (it->map_id != map_id || it->x != x || it->y != y) continue;
        w.cells.erase(it);
        if (w.sel_cell_valid && w.sel_cell_x == x && w.sel_cell_y == y)
            w.sel_cell_valid = false;
        return true;
    }
    return false;
}

Cell& SpawnCellAt(World& w, uint32_t map_id, int x, int y, uint8_t type, const char* label) {
    Cell& c = w.SpawnCell(x, y);
    c.SetMapId(map_id).SetType(type);
    if (label) c.SetLabel("%s", label);
    return c;
}

// ---- GBK → UTF-8 ----

bool GbkToUtf8(const char* gbk, char* utf8_out, int out_size) {
    if (!gbk || !utf8_out || out_size <= 0) return false;
#ifdef _WIN32
    wchar_t wbuf[256];
    int wlen = MultiByteToWideChar(936, 0, gbk, -1, wbuf, 256);
    if (wlen <= 0) { utf8_out[0] = '\0'; return false; }
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, utf8_out, out_size, nullptr, nullptr);
    if (ulen <= 0) { utf8_out[0] = '\0'; return false; }
    return true;
#else
    int i = 0;
    while (gbk[i] && i < out_size - 1) { utf8_out[i] = gbk[i]; ++i; }
    utf8_out[i] = '\0';
    return false;
#endif
}

Entity& Entity::SetName4GBK(const char* gbk) {
    GbkToUtf8(gbk, label, sizeof(label));
    return *this;
}

Cell& Cell::SetName4GBK(const char* gbk) {
    GbkToUtf8(gbk, label, sizeof(label));
    return *this;
}

// ---- Static entity links (id-pair) ----

void AddEntityLinkById(uint64_t from_id, uint64_t to_id,
                       uint32_t color, float thickness, bool dashed, bool arrow) {
    for (auto& sl : g_static_entity_links) {
        if (sl.from_id == from_id && sl.to_id == to_id) {
            sl.color = color; sl.thickness = thickness;
            sl.dashed = dashed; sl.arrow = arrow;
            return;
        }
    }
    g_static_entity_links.push_back({ from_id, to_id, color, thickness, dashed, arrow });
}

void RemoveEntityLinkById(uint64_t from_id, uint64_t to_id) {
    auto& v = g_static_entity_links;
    v.erase(std::remove_if(v.begin(), v.end(), [&](const StaticEntityLink& sl) {
        return sl.from_id == from_id && sl.to_id == to_id;
    }), v.end());
}

void ClearEntityLinksById() {
    g_static_entity_links.clear();
}

// ---- Simple entity link (single-target shorthand) ----

void RegisterEntityLink(const char* name, EntityTargetFn fn,
                        uint32_t color, float thickness, bool dashed, bool arrow) {
    RegisterEntityLinks(name, [fn, color, thickness, dashed, arrow](const Entity& e) -> std::vector<EntityLink> {
        uint64_t tid = fn(e);
        if (!tid) return {};
        EntityLink lnk;
        lnk.target_id = tid;
        lnk.color     = color;
        lnk.thickness = thickness;
        lnk.dashed    = dashed;
        lnk.arrow     = arrow;
        return { lnk };
    });
}

} // namespace dui
