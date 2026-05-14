#include "dui_ext.h"
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace {
    std::unordered_map<uint8_t, dui::EntityDrawer> g_entity_drawers;
    std::unordered_map<uint8_t, dui::CellDrawer>   g_cell_drawers;
    std::unordered_map<uint8_t, std::string>        g_entity_type_names;
    std::unordered_map<uint8_t, std::string>        g_cell_type_names;
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

} // namespace dui
