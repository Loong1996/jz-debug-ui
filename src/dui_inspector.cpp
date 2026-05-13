#include "dui_inspector.h"
#include "dui_ext.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <vector>

namespace dui {

// ---- Entity table: 3 cols (Type / 名称 / 坐标), Type cell tinted with entity color ----
static void EntityTableImpl(World& world, const std::vector<int>& idxs,
                             const char* tbl_id, bool with_scroll) {
    ImGuiTableFlags flags =
        ImGuiTableFlags_Borders      |
        ImGuiTableFlags_RowBg        |
        ImGuiTableFlags_SizingFixedFit;
    if (with_scroll) flags |= ImGuiTableFlags_ScrollY;
    float h = with_scroll ? 220.f : 0.f;

    if (!ImGui::BeginTable(tbl_id, 3, flags, ImVec2(0.f, h)))
        return;

    if (with_scroll) ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Type",   ImGuiTableColumnFlags_WidthFixed,   44.f);
    ImGui::TableSetupColumn(u8"名称", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(u8"坐标", ImGuiTableColumnFlags_WidthFixed,   72.f);
    ImGui::TableHeadersRow();

    for (int i : idxs) {
        auto& e = world.entities[i];
        bool selected = static_cast<int>(e.id) == world.selected_id;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        // Tint the Type column cell with the entity's color (semi-transparent)
        ImVec4 c4 = ImGui::ColorConvertU32ToFloat4(e.color);
        c4.w = 0.4f;
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                               ImGui::ColorConvertFloat4ToU32(c4));

        // Invisible Selectable spanning the full row, then SameLine to write Type text
        ImGui::PushID(static_cast<int>(e.id));
        if (ImGui::Selectable("##row", selected,
                ImGuiSelectableFlags_SpanAllColumns,
                ImVec2(0.f, ImGui::GetTextLineHeight())))
            world.selected_id = selected ? -1 : static_cast<int>(e.id);
        ImGui::SameLine(0.f, 0.f);
        ImGui::Text("%u", static_cast<unsigned>(e.type));

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(e.label);

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("(%d,%d)", e.x, e.y);

        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawInspector(World& world) {
    ImGui::Begin(u8"检视器");

    // ---- Static state ----
    static char search[64]       = {};
    static int  type_filter      = -1;
    static int  sort_mode        = 0;   // 0=默认 1=ID 2=Type 3=名称 4=距主角
    static bool group_by_type    = false;
    static char cell_search[64]  = {};
    static bool cell_group       = false;

    // ---- Collect unique entity types present this frame ----
    int  types[32];
    int  ntypes = 0;
    for (const auto& e : world.entities) {
        bool dup = false;
        for (int k = 0; k < ntypes; ++k)
            if (types[k] == static_cast<int>(e.type)) { dup = true; break; }
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
    if (type_filter != -1)
        for (int i = 0; i < ntypes; ++i)
            if (types[i] == type_filter) { combo_idx = i + 1; break; }

    // ---- Filter lambda (supports #1003 for id-exact match) ----
    auto pass_filter = [&](const Entity& e) {
        if (type_filter != -1 && static_cast<int>(e.type) != type_filter) return false;
        if (search[0] == '\0') return true;
        if (search[0] == '#') {
            char* endp = nullptr;
            long long want = std::strtoll(search + 1, &endp, 10);
            if (endp != search + 1)
                return static_cast<long long>(e.id) == want;
        }
        return std::strstr(e.label, search) != nullptr;
    };

    // ---- Collect filtered indices ----
    std::vector<int> show_idx;
    show_idx.reserve(world.entities.size());
    for (int i = 0; i < static_cast<int>(world.entities.size()); ++i)
        if (pass_filter(world.entities[i])) show_idx.push_back(i);

    // ---- Sort ----
    const Entity* player = nullptr;
    for (const auto& e : world.entities)
        if (static_cast<int>(e.id) == world.player_id) { player = &e; break; }

    std::sort(show_idx.begin(), show_idx.end(), [&](int ai, int bi) -> bool {
        const auto& a = world.entities[ai];
        const auto& b = world.entities[bi];
        switch (sort_mode) {
            case 1: return a.id < b.id;
            case 2: return a.type != b.type ? a.type < b.type : a.id < b.id;
            case 3: return std::strcmp(a.label, b.label) < 0;
            case 4: {
                if (!player) return a.id < b.id;
                float dax = a.fx - player->fx, day = a.fy - player->fy;
                float dbx = b.fx - player->fx, dby = b.fy - player->fy;
                return dax*dax + day*day < dbx*dbx + dby*dby;
            }
            default: return ai < bi;
        }
    });

    // ---- Toolbar (one row) ----
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 2.f));

    ImGui::SetNextItemWidth(130.f);
    ImGui::InputTextWithHint("##search", u8"搜索 (#id 或名称)", search, sizeof(search));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.f);
    if (ImGui::Combo("##type", &combo_idx, combo_items, ntypes + 1))
        type_filter = (combo_idx == 0) ? -1 : types[combo_idx - 1];
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);
    const char* sort_items[] = { u8"默认", "ID", "Type", u8"名称", u8"距主角" };
    ImGui::Combo("##sort", &sort_mode, sort_items, IM_ARRAYSIZE(sort_items));
    ImGui::SameLine();
    ImGui::Checkbox(u8"分组", &group_by_type);

    // Right-aligned entity count
    {
        char cnt[32];
        std::snprintf(cnt, sizeof(cnt), "%d / %d",
            static_cast<int>(show_idx.size()),
            static_cast<int>(world.entities.size()));
        float cnt_w = ImGui::CalcTextSize(cnt).x + 4.f;
        float avail  = ImGui::GetContentRegionAvail().x;
        if (avail > cnt_w) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - cnt_w);
            ImGui::TextDisabled("%s", cnt);
        }
    }

    ImGui::PopStyleVar();
    ImGui::Separator();

    // ---- Entity list ----
    if (show_idx.empty()) {
        ImGui::TextDisabled(u8"(无匹配项)");
    } else if (group_by_type) {
        // Collect group types (sorted ascending)
        int gtypes[32]; int ngtypes = 0;
        for (int i : show_idx) {
            int t = static_cast<int>(world.entities[i].type);
            bool dup = false;
            for (int k = 0; k < ngtypes; ++k) if (gtypes[k] == t) { dup = true; break; }
            if (!dup && ngtypes < 32) gtypes[ngtypes++] = t;
        }
        for (int i = 0; i < ngtypes; ++i)
            for (int j = i + 1; j < ngtypes; ++j)
                if (gtypes[j] < gtypes[i]) { int t = gtypes[i]; gtypes[i] = gtypes[j]; gtypes[j] = t; }

        for (int g = 0; g < ngtypes; ++g) {
            int gt = gtypes[g];
            std::vector<int> grp;
            for (int i : show_idx)
                if (static_cast<int>(world.entities[i].type) == gt)
                    grp.push_back(i);
            char hdr[32];
            std::snprintf(hdr, sizeof(hdr), u8"Type %d  (%d)##grp%d",
                gt, static_cast<int>(grp.size()), gt);
            if (ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen)) {
                char tid[16];
                std::snprintf(tid, sizeof(tid), "##tg%d", gt);
                EntityTableImpl(world, grp, tid, false);
            }
        }
    } else {
        EntityTableImpl(world, show_idx, "##ent_tbl", true);
    }

    // ---- Selected entity details ----
    if (world.selected_id != -1) {
        ImGui::Separator();
        int sel_x = 0, sel_y = 0;
        bool found = false;
        for (auto& e : world.entities) {
            if (static_cast<int>(e.id) != world.selected_id) continue;
            char sec[64];
            std::snprintf(sec, sizeof(sec),
                u8"基本信息 — %s  [type %d]##basic", e.label, e.type);
            if (ImGui::CollapsingHeader(sec, ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PushID(static_cast<int>(e.id));
                ImGui::Text("ID: %llu", static_cast<unsigned long long>(e.id));
                ImGui::Text(u8"x: %d   y: %d", e.x, e.y);
                ImGui::Text(u8"半径: %.2f", e.radius);
                InvokeEntityDrawer(e);
                ImGui::PopID();
            }
            sel_x = e.x; sel_y = e.y; found = true;
            break;
        }
        if (found) {
            // Check for same-position entities
            bool has_ov = false;
            for (const auto& e : world.entities) {
                if (static_cast<int>(e.id) == world.selected_id) continue;
                if (e.x == sel_x && e.y == sel_y) { has_ov = true; break; }
            }
            if (has_ov &&
                ImGui::CollapsingHeader(u8"同坐标其他实体##ov", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& e : world.entities) {
                    if (static_cast<int>(e.id) == world.selected_id) continue;
                    if (e.x != sel_x || e.y != sel_y) continue;
                    ImGui::PushID(static_cast<int>(e.id));
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), u8"[type %d] %s", e.type, e.label);
                    if (ImGui::Selectable(buf))
                        world.selected_id = static_cast<int>(e.id);
                    ImGui::PopID();
                }
            }
        }
    }

    // ---- Selected cell details ----
    if (world.sel_cell_valid) {
        ImGui::Separator();
        char cell_hdr[64];
        std::snprintf(cell_hdr, sizeof(cell_hdr),
            u8"选中格子 (%d, %d)##sc", world.sel_cell_x, world.sel_cell_y);
        if (ImGui::CollapsingHeader(cell_hdr, ImGuiTreeNodeFlags_DefaultOpen)) {
            int n = 0;
            for (auto& c : world.cells) {
                if (c.x != world.sel_cell_x || c.y != world.sel_cell_y) continue;
                if (n > 0) ImGui::Spacing();
                ImGui::PushID(n);
                ImGui::Text(u8"类型: %d  名称: %s", c.type, c.label);
                InvokeCellDrawer(c);
                ImGui::PopID();
                ++n;
            }
            if (n == 0) ImGui::TextDisabled(u8"(此位置无格子)");
        }
        // Entities on the selected cell
        bool any_ent = false;
        for (const auto& e : world.entities)
            if (e.x == world.sel_cell_x && e.y == world.sel_cell_y) { any_ent = true; break; }
        if (any_ent &&
            ImGui::CollapsingHeader(u8"压在此格的实体##sce", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& e : world.entities) {
                if (e.x != world.sel_cell_x || e.y != world.sel_cell_y) continue;
                bool is_sel = static_cast<int>(e.id) == world.selected_id;
                ImGui::PushID(static_cast<int>(e.id));
                char buf[64];
                std::snprintf(buf, sizeof(buf), u8"[type %d] %s", e.type, e.label);
                if (ImGui::Selectable(buf, is_sel))
                    world.selected_id = is_sel ? -1 : static_cast<int>(e.id);
                ImGui::PopID();
            }
        }
    }

    // ---- Cell list ----
    ImGui::Separator();
    if (ImGui::CollapsingHeader(u8"地图格子##cell_list", 0)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 2.f));
        ImGui::SetNextItemWidth(140.f);
        ImGui::InputTextWithHint("##csearch", u8"搜索名称", cell_search, sizeof(cell_search));
        ImGui::SameLine();
        ImGui::Checkbox(u8"分组##cgrp", &cell_group);
        ImGui::PopStyleVar();

        auto pass_cell = [&](const Cell& c) {
            if (cell_search[0] == '\0') return true;
            return std::strstr(c.label, cell_search) != nullptr;
        };

        if (cell_group) {
            int ctypes[32]; int nctypes = 0;
            for (const auto& c : world.cells) {
                if (!pass_cell(c)) continue;
                bool dup = false;
                for (int k = 0; k < nctypes; ++k) if (ctypes[k] == c.type) { dup = true; break; }
                if (!dup && nctypes < 32) ctypes[nctypes++] = c.type;
            }
            for (int i = 0; i < nctypes; ++i)
                for (int j = i + 1; j < nctypes; ++j)
                    if (ctypes[j] < ctypes[i]) { int t = ctypes[i]; ctypes[i] = ctypes[j]; ctypes[j] = t; }

            for (int g = 0; g < nctypes; ++g) {
                int gt = ctypes[g];
                int cnt_c = 0;
                for (const auto& c : world.cells) if (pass_cell(c) && c.type == gt) ++cnt_c;
                char hdr[32];
                std::snprintf(hdr, sizeof(hdr), u8"Type %d  (%d)##cgrp%d", gt, cnt_c, gt);
                if (ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen)) {
                    int ci = 0;
                    for (const auto& c : world.cells) {
                        if (c.type != gt || !pass_cell(c)) continue;
                        char buf[64];
                        std::snprintf(buf, sizeof(buf), "(%d,%d) %s", c.x, c.y, c.label);
                        bool sel = world.sel_cell_valid &&
                                   world.sel_cell_x == c.x && world.sel_cell_y == c.y;
                        ImGui::PushID(ci + gt * 1000);
                        if (ImGui::Selectable(buf, sel)) {
                            world.sel_cell_valid = true;
                            world.sel_cell_x = c.x; world.sel_cell_y = c.y;
                        }
                        ImGui::PopID();
                        ++ci;
                    }
                }
            }
        } else {
            int ci = 0; bool any = false;
            for (const auto& c : world.cells) {
                if (!pass_cell(c)) continue;
                any = true;
                char buf[64];
                std::snprintf(buf, sizeof(buf), "(%d,%d) %s", c.x, c.y, c.label);
                bool sel = world.sel_cell_valid &&
                           world.sel_cell_x == c.x && world.sel_cell_y == c.y;
                ImGui::PushID(ci++);
                if (ImGui::Selectable(buf, sel)) {
                    world.sel_cell_valid = true;
                    world.sel_cell_x = c.x; world.sel_cell_y = c.y;
                }
                ImGui::PopID();
            }
            if (!any) ImGui::TextDisabled(u8"(无匹配项)");
        }
    }

    ImGui::End();
}

} // namespace dui
