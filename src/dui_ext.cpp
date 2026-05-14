#include "dui_ext.h"
#include <imgui.h>
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

    // Thread-local canvas view state (set each frame by DrawCanvas)
    struct CanvasViewState { float fcx, fcy, th; ImVec2 center; };
    thread_local CanvasViewState s_cv_view = {};
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
const char* GetMapName(uint32_t map_id) {
    auto it = g_map_names.find(map_id);
    return it != g_map_names.end() ? it->second.c_str() : nullptr;
}
void SwitchActiveMap(World& w, uint32_t new_map_id) {
    w.active_map_id  = new_map_id;
    w.sel_cell_valid = false;
    w.selected_id    = -1;
    for (const auto& e : w.entities) {
        if (e.map_id == new_map_id) {
            w.selected_id = static_cast<int>(e.id);
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

} // namespace dui
