#include "dui_inspector.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>

namespace dui {

void DrawInspector(World& world) {
    ImGui::Begin(u8"检视器");

    ImGui::Text(u8"实体数量: %d", static_cast<int>(world.entities.size()));

    // --- Filter controls ---
    static char search[64]  = {};
    static int  type_filter = -1;

    // Collect unique entity types present this frame
    int  types[32];
    int  ntypes = 0;
    for (const auto& e : world.entities) {
        bool dup = false;
        for (int k = 0; k < ntypes; ++k) if (types[k] == static_cast<int>(e.type)) { dup = true; break; }
        if (!dup && ntypes < 32) types[ntypes++] = static_cast<int>(e.type);
    }
    for (int i = 0; i < ntypes; ++i)
        for (int j = i + 1; j < ntypes; ++j)
            if (types[j] < types[i]) { int t = types[i]; types[i] = types[j]; types[j] = t; }

    static const char* ALL_LABEL = u8"全部";
    const char* combo_items[33];
    combo_items[0] = ALL_LABEL;
    char type_bufs[32][8];
    for (int i = 0; i < ntypes; ++i) {
        std::snprintf(type_bufs[i], sizeof(type_bufs[i]), "%d", types[i]);
        combo_items[i + 1] = type_bufs[i];
    }

    int combo_idx = 0;
    if (type_filter != -1) {
        for (int i = 0; i < ntypes; ++i)
            if (types[i] == type_filter) { combo_idx = i + 1; break; }
    }

    ImGui::SetNextItemWidth(140.f);
    ImGui::InputText(u8"##搜索", search, sizeof(search));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.f);
    if (ImGui::Combo(u8"类型", &combo_idx, combo_items, ntypes + 1))
        type_filter = (combo_idx == 0) ? -1 : types[combo_idx - 1];

    auto pass_filter = [&](const Entity& e) {
        if (type_filter != -1 && static_cast<int>(e.type) != type_filter) return false;
        if (search[0] != '\0' && std::strstr(e.label, search) == nullptr) return false;
        return true;
    };

    ImGui::Separator();

    // --- Entity list ---
    if (ImGui::TreeNodeEx(u8"实体列表", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& e : world.entities) {
            if (!pass_filter(e)) continue;
            bool selected = (static_cast<int>(e.id) == world.selected_id);
            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                (selected ? ImGuiTreeNodeFlags_Selected : 0);
            ImGui::PushID(static_cast<int>(e.id));
            ImGui::TreeNodeEx(e.label, flags);
            if (ImGui::IsItemClicked())
                world.selected_id = selected ? -1 : static_cast<int>(e.id);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    // --- Selected entity details ---
    if (world.selected_id != -1) {
        ImGui::Separator();
        for (auto& e : world.entities) {
            if (static_cast<int>(e.id) != world.selected_id) continue;
            ImGui::PushID(static_cast<int>(e.id));
            ImGui::Text(u8"已选中: %s  [type %d]", e.label, e.type);
            if (ImGui::InputInt(u8"x 坐标", &e.x, 1, 5)) e.fx = static_cast<float>(e.x);
            if (ImGui::InputInt(u8"y 坐标", &e.y, 1, 5)) e.fy = static_cast<float>(e.y);
            ImGui::SliderFloat(u8"半径",  &e.radius, 0.1f, 1.f, "%.2f");
            ImGui::PopID();
            break;
        }
    }

    // --- Selected cell details ---
    if (world.sel_cell_valid) {
        ImGui::Separator();
        ImGui::Text(u8"选中格子: (%d, %d)", world.sel_cell_x, world.sel_cell_y);
        for (const auto& c : world.cells) {
            if (c.x != world.sel_cell_x || c.y != world.sel_cell_y) continue;
            ImGui::Text(u8"类型: %d  名称: %s", c.type, c.label);
            break;
        }
    }

    // --- Cell list ---
    ImGui::Separator();
    if (ImGui::TreeNodeEx(u8"地图格子", 0)) {
        int cell_idx = 0;
        for (const auto& c : world.cells) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "(%d,%d) %s", c.x, c.y, c.label);
            bool sel = world.sel_cell_valid &&
                       world.sel_cell_x == c.x &&
                       world.sel_cell_y == c.y;
            ImGui::PushID(cell_idx++);
            if (ImGui::Selectable(buf, sel)) {
                world.sel_cell_valid = true;
                world.sel_cell_x     = c.x;
                world.sel_cell_y     = c.y;
                world.selected_id    = -1;
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

} // namespace dui
