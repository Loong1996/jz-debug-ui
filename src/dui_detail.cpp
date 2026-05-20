#include "dui_detail.h"
#include "dui_ext.h"
#include "dui_menubar.h"
#include <imgui.h>
#include <cstdio>
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

// ---- DrawEntityDetail -------------------------------------------------------
// Unified "current selection" panel: selected entity (top) + selected cell (bottom).

void DrawEntityDetail(World& world) {
    if (!ImGui::Begin(u8"实体详情", &BuiltinPanelOpenRef(u8"实体详情")))
        { ImGui::End(); return; }

    bool anything_shown = false;

    // ---- Selected entity ----
    if (world.selected_id != 0) {
        Entity* found = nullptr;
        for (auto& e : world.entities)
            if (e.id == world.selected_id && e.map_id == world.active_map_id)
                { found = &e; break; }

        if (found) {
            anything_shown = true;
            Entity& e = *found;

            // Tinted header
            const char* etn = GetEntityTypeName(e.type);
            char hdr[80];
            if (etn) std::snprintf(hdr, sizeof(hdr), u8"实体 — %s  [%s]", e.label, etn);
            else     std::snprintf(hdr, sizeof(hdr), u8"实体 — %s  [type %u]", e.label, static_cast<unsigned>(e.type));
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.f, 1.f), "%s", hdr);
            ImGui::Separator();

            // Basic info
            ImGui::PushID(static_cast<int>(e.id));
            ImGui::Text("ID     : %llu", static_cast<unsigned long long>(e.id));
            ImGui::Text(u8"Pos    : (%d, %d)  fxy (%.2f, %.2f)", e.x, e.y, e.fx, e.fy);
            ImGui::Text(u8"Radius : %.2f", e.radius);

            // RegisterEntityDrawer fields
            InvokeEntityDrawer(e);

            // Same-position overlay
            bool has_ov = false;
            for (const auto& oe : world.entities) {
                if (oe.id == e.id || oe.map_id != world.active_map_id) continue;
                if (oe.x == e.x && oe.y == e.y) { has_ov = true; break; }
            }
            if (has_ov) {
                ImGui::Spacing();
                ImGui::TextDisabled(u8"▸ 同坐标其他实体");
                for (auto& oe : world.entities) {
                    if (oe.id == e.id || oe.map_id != world.active_map_id) continue;
                    if (oe.x != e.x || oe.y != e.y) continue;
                    ImGui::PushID(static_cast<int>(oe.id));
                    char buf[64];
                    const char* otn = GetEntityTypeName(oe.type);
                    if (otn) std::snprintf(buf, sizeof(buf), "  [%s] %s", otn, oe.label);
                    else     std::snprintf(buf, sizeof(buf), u8"  [type %u] %s", static_cast<unsigned>(oe.type), oe.label);
                    if (ImGui::Selectable(buf)) { SelectClear(world); SelectAdd(world, oe.id); }
                    ImGui::PopID();
                }
            }

            // Live editor
            if (g_entity_editors.count(e.type) > 0) {
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(u8"编辑##eedit")) InvokeEntityEditor_(e);
            }
            ImGui::PopID();

            // Registered detail text
            std::string text = InvokeEntityDetailText(e);
            if (!text.empty()) {
                ImGui::Spacing();
                if (ImGui::BeginChild("##ent_detail", ImVec2(0, 160.f), ImGuiChildFlags_Borders))
                    ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
                ImGui::EndChild();
            }
        }
    }

    // ---- Selected cell ----
    if (world.sel_cell_valid) {
        if (anything_shown) { ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); }
        anything_shown = true;

        char cell_hdr[48];
        std::snprintf(cell_hdr, sizeof(cell_hdr),
            u8"格子 (%d, %d)", world.sel_cell_x, world.sel_cell_y);
        ImGui::TextColored(ImVec4(0.75f, 1.f, 0.55f, 1.f), "%s", cell_hdr);
        ImGui::Separator();

        int n = 0;
        for (auto& c : world.cells) {
            if (c.map_id != world.active_map_id) continue;
            if (c.x != world.sel_cell_x || c.y != world.sel_cell_y) continue;
            ImGui::PushID(n);
            const char* ctn = GetCellTypeName(c.type); char cnfb[16];
            if (!ctn) { std::snprintf(cnfb, sizeof(cnfb), "%d", c.type); ctn = cnfb; }
            char chdr[64];
            std::snprintf(chdr, sizeof(chdr), "[%s] %s##csc%d", ctn, c.label, n);
            ImVec4 ch4 = ImGui::ColorConvertU32ToFloat4(c.color);
            ch4.w = 0.55f; ImGui::PushStyleColor(ImGuiCol_Header,        ImGui::ColorConvertFloat4ToU32(ch4));
            ch4.w = 0.70f; ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::ColorConvertFloat4ToU32(ch4));
            ch4.w = 0.85f; ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImGui::ColorConvertFloat4ToU32(ch4));
            if (ImGui::CollapsingHeader(chdr, ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text(u8"坐标: (%d, %d)", c.x, c.y);
                InvokeCellDrawer(c);
                std::string ctext = InvokeCellDetailText(c);
                if (!ctext.empty()) {
                    ImGui::Separator();
                    ImGui::TextUnformatted(ctext.c_str(), ctext.c_str() + ctext.size());
                }
                if (g_cell_editors.count(c.type) > 0 &&
                    ImGui::CollapsingHeader(u8"编辑##cedit")) InvokeCellEditor_(c);
            }
            ImGui::PopStyleColor(3);
            ImGui::PopID();
            ++n;
        }
        if (n == 0) ImGui::TextDisabled(u8"(此位置无格子)");

        // Entities sitting on this cell
        bool any_ent = false;
        for (const auto& e : world.entities)
            if (e.map_id == world.active_map_id &&
                e.x == world.sel_cell_x && e.y == world.sel_cell_y)
                { any_ent = true; break; }
        if (any_ent) {
            ImGui::Spacing();
            ImGui::TextDisabled(u8"▸ 压在此格的实体");
            for (auto& e : world.entities) {
                if (e.map_id != world.active_map_id) continue;
                if (e.x != world.sel_cell_x || e.y != world.sel_cell_y) continue;
                bool is_sel = IsSelected(world, e.id);
                ImGui::PushID(static_cast<int>(e.id));
                char buf[64];
                const char* etn = GetEntityTypeName(e.type);
                if (etn) std::snprintf(buf, sizeof(buf), "  [%s] %s", etn, e.label);
                else     std::snprintf(buf, sizeof(buf), u8"  [type %u] %s", static_cast<unsigned>(e.type), e.label);
                if (ImGui::Selectable(buf, is_sel)) {
                    if (is_sel) SelectClear(world);
                    else { SelectClear(world); SelectAdd(world, e.id); }
                }
                ImGui::PopID();
            }
        }
    }

    if (!anything_shown)
        ImGui::TextDisabled(u8"(未选中任何实体或格子)");

    ImGui::End();
}

} // namespace dui
