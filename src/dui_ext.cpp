#include "dui_ext.h"
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace {
    std::unordered_map<uint8_t,  dui::EntityDrawer>    g_entity_drawers;
    std::unordered_map<uint8_t,  dui::CellDrawer>      g_cell_drawers;
    std::unordered_map<uint8_t,  std::string>           g_entity_type_names;
    std::unordered_map<uint8_t,  std::string>           g_cell_type_names;
    std::unordered_map<uint32_t, std::string>           g_map_names;
    std::unordered_map<uint8_t,  dui::EntityLabelFn>    g_label_fns;
    uint8_t g_player_entity_type = 255;
    bool    g_player_type_set    = false;
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

} // namespace dui
