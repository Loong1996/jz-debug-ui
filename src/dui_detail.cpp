#include "dui_detail.h"
#include <imgui.h>
#include <unordered_map>

namespace {
    std::unordered_map<uint8_t, dui::EntityDetailTextFn> g_text_fns;
}

namespace dui {

void RegisterEntityDetailText(uint8_t type, EntityDetailTextFn fn) {
    g_text_fns[type] = std::move(fn);
}

std::string InvokeEntityDetailText(const Entity& e) {
    auto it = g_text_fns.find(e.type);
    if (it == g_text_fns.end()) return {};
    return it->second(e);
}

void DrawEntityDetail(World& world) {
    ImGui::Begin(u8"实体详情");

    if (world.selected_id == -1) {
        ImGui::TextDisabled(u8"(未选中实体)");
        ImGui::End();
        return;
    }

    const Entity* found = nullptr;
    for (const auto& e : world.entities)
        if (static_cast<int>(e.id) == world.selected_id) { found = &e; break; }

    if (!found) {
        ImGui::TextDisabled(u8"(未选中实体)");
        ImGui::End();
        return;
    }

    const Entity& e = *found;
    ImGui::Text("ID    : %llu", static_cast<unsigned long long>(e.id));
    ImGui::Text(u8"Type  : %u  Label : %s", static_cast<unsigned>(e.type), e.label);
    ImGui::Text(u8"Pos   : (%d, %d)  fxy : (%.2f, %.2f)", e.x, e.y, e.fx, e.fy);
    ImGui::Text(u8"Radius: %.2f", e.radius);
    ImGui::Separator();

    std::string text = InvokeEntityDetailText(e);
    if (ImGui::BeginChild("##detail_body", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
        if (!text.empty())
            ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
        else
            ImGui::TextDisabled(u8"(无扩展详情，调用 RegisterEntityDetailText 后显示)");
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace dui
