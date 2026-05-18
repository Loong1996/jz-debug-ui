#include "dui_ext.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    std::unordered_map<uint8_t,  dui::EntityDrawer>    g_entity_drawers;
    std::unordered_map<uint8_t,  dui::CellDrawer>      g_cell_drawers;
    std::unordered_map<uint8_t,  std::string>           g_entity_type_names;
    std::unordered_map<uint8_t,  std::string>           g_cell_type_names;
    std::unordered_map<uint32_t, std::string>           g_map_names;
    std::unordered_map<uint8_t,  dui::EntityLabelFn>    g_label_fns;
    std::unordered_map<uint64_t, uint32_t>               g_entity_markers;
    uint8_t g_player_entity_type = 255;
    bool    g_player_type_set    = false;

    // Overlay registries
    std::unordered_map<uint8_t, dui::EntityOverlayFn> g_entity_overlays;
    struct GlobalOverlay { std::string name; dui::GlobalOverlayFn fn; };
    std::vector<GlobalOverlay> g_global_overlays;

    // Cell overlay registry
    std::unordered_map<uint8_t, dui::CellOverlayFn> g_cell_overlays;

    // Heatmap registry
    struct HeatmapEntry { std::string name; dui::CellValueFn fn; dui::HeatmapOpts opts; };
    std::vector<HeatmapEntry> g_heatmaps;

    // Entity links registry
    struct EntityLinksEntry { std::string name; dui::EntityLinksFn fn; };
    std::vector<EntityLinksEntry> g_entity_links;

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

void RegisterEntityDrawer(uint8_t type, EntityDrawer drawer) {
    g_entity_drawers[type] = std::move(drawer);
}

void RegisterCellDrawer(uint8_t type, CellDrawer drawer) {
    g_cell_drawers[type] = std::move(drawer);
}

void RegisterEntityTypeName(uint8_t type, const char* name) {
    g_entity_type_names[type] = name ? name : "";
}
void RegisterCellTypeName(uint8_t type, const char* name) {
    g_cell_type_names[type] = name ? name : "";
}
const char* GetEntityTypeName(uint8_t type) {
    auto it = g_entity_type_names.find(type);
    return it != g_entity_type_names.end() ? it->second.c_str() : nullptr;
}
const char* GetCellTypeName(uint8_t type) {
    auto it = g_cell_type_names.find(type);
    return it != g_cell_type_names.end() ? it->second.c_str() : nullptr;
}

void RegisterMapName(uint32_t map_id, const char* name) {
    g_map_names[map_id] = name ? name : "";
}

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
    w.selected_id    = -1;
    w.selected_ids.clear();
    for (const auto& e : w.entities) {
        if (e.map_id == new_map_id) {
            w.selected_id = static_cast<int>(e.id);
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

void SetPlayerEntityType(uint8_t type) { g_player_entity_type = type; g_player_type_set = true; }
bool IsPlayerEntityType (uint8_t type) { return g_player_type_set && type == g_player_entity_type; }

void RegisterEntityLabelFn(uint8_t type, EntityLabelFn fn) {
    g_label_fns[type] = std::move(fn);
}

std::string InvokeEntityLabel(const Entity& e) {
    auto it = g_label_fns.find(e.type);
    if (it != g_label_fns.end()) return it->second(e);
    return e.label;
}

void SetEntityMarker(uint64_t entity_id, uint32_t color) {
    g_entity_markers[entity_id] = color;
}
void ClearEntityMarker(uint64_t entity_id) {
    g_entity_markers.erase(entity_id);
}
const uint32_t* GetEntityMarker(uint64_t entity_id) {
    auto it = g_entity_markers.find(entity_id);
    return it != g_entity_markers.end() ? &it->second : nullptr;
}

void RegisterEntityOverlay(uint8_t type, EntityOverlayFn fn) {
    g_entity_overlays[type] = std::move(fn);
}

void RegisterGlobalOverlay(const char* name, GlobalOverlayFn fn) {
    if (!name) return;
    for (auto& g : g_global_overlays) {
        if (g.name == name) { g.fn = std::move(fn); return; }
    }
    g_global_overlays.push_back({ std::string(name), std::move(fn) });
}

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
    CanvasOverlayCtx ctx{ CanvasToScreen_, dl };
    it->second(e, ctx);
}

void InvokeGlobalOverlays_(const World& world, ImDrawList* dl) {
    CanvasOverlayCtx ctx{ CanvasToScreen_, dl };
    for (const auto& g : g_global_overlays)
        if (g.fn) g.fn(world, ctx);
}

// ---- Cell overlay ----

void RegisterCellOverlay(uint8_t type, CellOverlayFn fn) {
    g_cell_overlays[type] = std::move(fn);
}

void InvokeCellOverlays_(const World& world, const Cell& c, ImDrawList* dl) {
    auto it = g_cell_overlays.find(c.type);
    if (it == g_cell_overlays.end()) return;
    CanvasOverlayCtx ctx{ CanvasToScreen_, dl };
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
        if (!h.fn) continue;
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

void UnregisterEntityLinks(const char* name) {
    if (!name) return;
    for (auto it = g_entity_links.begin(); it != g_entity_links.end(); ++it) {
        if (it->name == name) { g_entity_links.erase(it); return; }
    }
}

void InvokeEntityLinks_(const World& world, ImDrawList* dl) {
    if (g_entity_links.empty()) return;

    // Build id→entity index for the active map
    std::unordered_map<uint64_t, const Entity*> idx;
    idx.reserve(world.entities.size());
    for (const auto& e : world.entities)
        if (e.map_id == world.active_map_id) idx[e.id] = &e;

    for (const auto& e : world.entities) {
        if (e.map_id != world.active_map_id) continue;
        for (const auto& entry : g_entity_links) {
            if (!entry.fn) continue;
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
}

// ---- User Panels ----

void RegisterPanel(const char* window_title, PanelDrawFn draw_fn, PanelDock dock) {
    if (!window_title) return;
    for (auto& p : g_panels) {
        if (p.title == window_title) { p.fn = std::move(draw_fn); p.dock = dock; return; }
    }
    g_panels.push_back({ std::string(window_title), std::move(draw_fn), dock });
}

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
    if (set_primary) w.selected_id = static_cast<int>(id);
}

void SelectRemove(World& w, uint64_t id) {
    for (auto it = w.selected_ids.begin(); it != w.selected_ids.end(); ++it) {
        if (*it != id) continue;
        w.selected_ids.erase(it);
        if (static_cast<int>(id) == w.selected_id)
            w.selected_id = w.selected_ids.empty() ? -1
                          : static_cast<int>(w.selected_ids.back());
        return;
    }
}

void SelectToggle(World& w, uint64_t id) {
    if (IsSelected(w, id)) SelectRemove(w, id);
    else                   SelectAdd(w, id);
}

void SelectClear(World& w) {
    w.selected_ids.clear();
    w.selected_id = -1;
}

} // namespace dui
