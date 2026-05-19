#include "dui_detail.h"
#include <imgui.h>
#include <unordered_map>

namespace {
    std::unordered_map<uint8_t, dui::EntityDetailTextFn>    g_text_fns;
    std::unordered_map<uint8_t, dui::EntityDetailBuilderFn> g_builder_fns;
    std::unordered_map<uint8_t, dui::CellDetailTextFn>      g_cell_text_fns;
    std::unordered_map<uint8_t, dui::CellDetailBuilderFn>   g_cell_builder_fns;
    std::unordered_map<uint8_t, dui::EntityEditFn>          g_entity_editors;
    std::unordered_map<uint8_t, dui::CellEditFn>            g_cell_editors;
}

namespace dui {

void RegisterEntityDetailText(uint8_t type, EntityDetailTextFn fn) {
    g_builder_fns.erase(type);
    g_text_fns[type] = std::move(fn);
}
void RegisterEntityDetailText(uint8_t type, EntityDetailBuilderFn fn) {
    g_text_fns.erase(type);
    g_builder_fns[type] = std::move(fn);
}

std::string InvokeEntityDetailText(const Entity& e) {
    auto it = g_text_fns.find(e.type);
    if (it != g_text_fns.end()) return it->second(e);
    auto ib = g_builder_fns.find(e.type);
    if (ib != g_builder_fns.end()) {
        DetailBuilder db;
        ib->second(e, db);
        return db.Take();
    }
    return {};
}

// ---- Cell detail text ----

void RegisterCellDetailText(uint8_t type, CellDetailTextFn fn) {
    g_cell_builder_fns.erase(type);
    g_cell_text_fns[type] = std::move(fn);
}
void RegisterCellDetailText(uint8_t type, CellDetailBuilderFn fn) {
    g_cell_text_fns.erase(type);
    g_cell_builder_fns[type] = std::move(fn);
}

std::string InvokeCellDetailText(const Cell& c) {
    auto it = g_cell_text_fns.find(c.type);
    if (it != g_cell_text_fns.end()) return it->second(c);
    auto ib = g_cell_builder_fns.find(c.type);
    if (ib != g_cell_builder_fns.end()) {
        DetailBuilder db;
        ib->second(c, db);
        return db.Take();
    }
    return {};
}

bool InvokeCellDetailText_(const Cell& c) {
    std::string text = InvokeCellDetailText(c);
    if (text.empty()) return false;
    ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
    return true;
}

// ---- Entity/cell editors ----

void RegisterEntityEditor(uint8_t type, EntityEditFn fn) {
    g_entity_editors[type] = std::move(fn);
}
void RegisterCellEditor(uint8_t type, CellEditFn fn) {
    g_cell_editors[type] = std::move(fn);
}

bool InvokeEntityEditor_(Entity& e) {
    auto it = g_entity_editors.find(e.type);
    if (it == g_entity_editors.end()) return false;
    it->second(e);
    return true;
}
bool InvokeCellEditor_(Cell& c) {
    auto it = g_cell_editors.find(c.type);
    if (it == g_cell_editors.end()) return false;
    it->second(c);
    return true;
}

// ---- DrawEntityDetail ----

void DrawEntityDetail(World& world) {
    ImGui::Begin(u8"实体详情");

    if (world.selected_id == 0) {
        ImGui::TextDisabled(u8"(未选中实体)");
        ImGui::End();
        return;
    }

    Entity* found = nullptr;
    for (auto& e : world.entities)
        if (e.id == world.selected_id && e.map_id == world.active_map_id) { found = &e; break; }

    if (!found) {
        ImGui::TextDisabled(u8"(未选中实体)");
        ImGui::End();
        return;
    }

    Entity& e = *found;
    ImGui::Text("ID    : %llu", static_cast<unsigned long long>(e.id));
    ImGui::Text(u8"Type  : %u  Label : %s", static_cast<unsigned>(e.type), e.label);
    ImGui::Text(u8"Pos   : (%d, %d)  fxy : (%.2f, %.2f)", e.x, e.y, e.fx, e.fy);
    ImGui::Text(u8"Radius: %.2f", e.radius);

    // Live editor (RegisterEntityEditor) — placed before the scrollable detail body.
    if (g_entity_editors.count(e.type) > 0) {
        if (ImGui::CollapsingHeader(u8"编辑##edit_section")) {
            ImGui::PushID(static_cast<int>(e.id));
            InvokeEntityEditor_(e);
            ImGui::PopID();
        }
    }
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
