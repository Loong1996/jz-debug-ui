#include "dui_inspector.h"
#include <imgui.h>

namespace dui {

void DrawInspector(World& world) {
    ImGui::Begin(u8"检视器");

    ImGui::Text(u8"实体数量: %d", static_cast<int>(world.entities.size()));
    ImGui::Separator();

    if (ImGui::TreeNodeEx(u8"实体列表", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& e : world.entities) {
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

    if (world.selected_id != -1) {
        ImGui::Separator();
        for (auto& e : world.entities) {
            if (static_cast<int>(e.id) != world.selected_id) continue;
            ImGui::PushID(static_cast<int>(e.id));
            ImGui::Text(u8"已选中: %s", e.label);
            if (ImGui::InputInt(u8"x 坐标", &e.x, 1, 5)) e.fx = static_cast<float>(e.x);
            if (ImGui::InputInt(u8"y 坐标", &e.y, 1, 5)) e.fy = static_cast<float>(e.y);
            ImGui::SliderFloat(u8"半径",  &e.radius, 0.1f, 1.f, "%.2f");
            ImGui::PopID();
            break;
        }
    }

    ImGui::End();
}

} // namespace dui
