#include "dui_inspector.h"
#include <imgui.h>

namespace dui {

void DrawInspector(World& world) {
    ImGui::Begin("Inspector");

    ImGui::Text("Entities: %d", static_cast<int>(world.entities.size()));
    ImGui::Separator();

    if (ImGui::TreeNodeEx("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
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
            ImGui::Text("Selected: %s", e.label);
            ImGui::InputFloat("x",       &e.x,      0.1f, 1.f, "%.3f");
            ImGui::InputFloat("y",       &e.y,      0.1f, 1.f, "%.3f");
            ImGui::SliderFloat("radius", &e.radius, 0.1f, 3.f, "%.2f");
            ImGui::PopID();
            break;
        }
    }

    ImGui::End();
}

} // namespace dui
