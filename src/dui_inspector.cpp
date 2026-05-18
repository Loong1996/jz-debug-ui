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
        bool selected = IsSelected(world, e.id);

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
                ImVec2(0.f, ImGui::GetTextLineHeight()))) {
            if (ImGui::GetIO().KeyCtrl) {
                SelectToggle(world, e.id);
            } else {
                SelectClear(world);
                if (!selected) SelectAdd(world, e.id);
            }
        }
        ImGui::SameLine(0.f, 0.f);
        { const char* tn = GetEntityTypeName(e.type); char fb[16];
          if (!tn) { std::snprintf(fb, sizeof(fb), "%u", static_cast<unsigned>(e.type)); tn = fb; }
          ImGui::TextUnformatted(tn); }

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(e.label);

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("(%d,%d)", e.x, e.y);

        ImGui::PopID();
    }
    ImGui::EndTable();
}

// ---- Cell table: 3 cols (Type / 名称 / 坐标), Type cell tinted with cell color ----
static void CellTableImpl(World& world, const std::vector<int>& idxs,
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
        auto& c = world.cells[i];
        bool selected = world.sel_cell_valid &&
                        world.sel_cell_x == c.x && world.sel_cell_y == c.y;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        ImVec4 c4 = ImGui::ColorConvertU32ToFloat4(c.color);
        c4.w = 0.4f;
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                               ImGui::ColorConvertFloat4ToU32(c4));

        ImGui::PushID(i);
        if (ImGui::Selectable("##crow", selected,
                ImGuiSelectableFlags_SpanAllColumns,
                ImVec2(0.f, ImGui::GetTextLineHeight()))) {
            world.sel_cell_valid = true;
            world.sel_cell_x = c.x;
            world.sel_cell_y = c.y;
        }
        ImGui::SameLine(0.f, 0.f);
        { const char* tn = GetCellTypeName(c.type); char fb[16];
          if (!tn) { std::snprintf(fb, sizeof(fb), "%u", static_cast<unsigned>(c.type)); tn = fb; }
          ImGui::TextUnformatted(tn); }

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(c.label);

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("(%d,%d)", c.x, c.y);

        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawInspector(World& world) {
    ImGui::Begin(u8"检视器");

    // ---- Map selector ----
    {
        uint32_t all_map_ids[64]; int n_all_maps = 0;
        for (const auto& e : world.entities) {
            bool dup = false;
            for (int k = 0; k < n_all_maps; ++k) if (all_map_ids[k] == e.map_id) { dup = true; break; }
            if (!dup && n_all_maps < 64) all_map_ids[n_all_maps++] = e.map_id;
        }
        for (const auto& c : world.cells) {
            bool dup = false;
            for (int k = 0; k < n_all_maps; ++k) if (all_map_ids[k] == c.map_id) { dup = true; break; }
            if (!dup && n_all_maps < 64) all_map_ids[n_all_maps++] = c.map_id;
        }
        for (int i = 0; i < n_all_maps; ++i)
            for (int j = i + 1; j < n_all_maps; ++j)
                if (all_map_ids[j] < all_map_ids[i]) { uint32_t t = all_map_ids[i]; all_map_ids[i] = all_map_ids[j]; all_map_ids[j] = t; }
        char map_bufs[64][48]; const char* map_items[64];
        int  map_combo_idx = 0;
        for (int i = 0; i < n_all_maps; ++i) {
            int ent_cnt = 0, cell_cnt = 0;
            for (const auto& e : world.entities) if (e.map_id == all_map_ids[i]) ++ent_cnt;
            for (const auto& c : world.cells)    if (c.map_id == all_map_ids[i]) ++cell_cnt;
            const char* mn = GetMapName(all_map_ids[i]);
            if (mn) std::snprintf(map_bufs[i], sizeof(map_bufs[i]), "%s [%u] (%d/%d)", mn, all_map_ids[i], ent_cnt, cell_cnt);
            else    std::snprintf(map_bufs[i], sizeof(map_bufs[i]), "Map %u (%d/%d)", all_map_ids[i], ent_cnt, cell_cnt);
            map_items[i] = map_bufs[i];
            if (all_map_ids[i] == world.active_map_id) map_combo_idx = i;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 2.f));
        ImGui::TextUnformatted(u8"地图"); ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::Combo("##insp_map", &map_combo_idx, map_items, n_all_maps) && n_all_maps > 0)
            SwitchActiveMap(world, all_map_ids[map_combo_idx]);
        ImGui::PopStyleVar();
        ImGui::Separator();
    }

    // ---- Static state ----
    static char search[64]       = {};
    static int  type_filter      = -1;
    static int  sort_mode        = 0;   // 0=默认 1=ID 2=Type 3=名称 4=距主角
    static bool group_by_type    = false;
    static char cell_search[64]  = {};
    static bool cell_group       = false;
    static int  cell_type_filter = -1;
    static int  cell_sort_mode   = 0;   // 0=默认 1=Type 2=名称 3=坐标

    // ---- Collect unique entity types present this frame (active map only) ----
    int  types[32];
    int  ntypes = 0;
    for (const auto& e : world.entities) {
        if (e.map_id != world.active_map_id) continue;
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
    char type_bufs[32][32];
    for (int i = 0; i < ntypes; ++i) {
        const char* tn = GetEntityTypeName(static_cast<uint8_t>(types[i]));
        if (tn) std::snprintf(type_bufs[i], sizeof(type_bufs[i]), "%s", tn);
        else    std::snprintf(type_bufs[i], sizeof(type_bufs[i]), "%d", types[i]);
        combo_items[i + 1] = type_bufs[i];
    }
    int combo_idx = 0;
    if (type_filter != -1)
        for (int i = 0; i < ntypes; ++i)
            if (types[i] == type_filter) { combo_idx = i + 1; break; }

    // ---- Filter lambda (supports #1003 for id-exact match) ----
    auto pass_filter = [&](const Entity& e) {
        if (e.map_id != world.active_map_id) return false;
        if (type_filter != -1 && static_cast<int>(e.type) != type_filter) return false;
        if (search[0] == '\0') return true;
        if (search[0] == '#') {
            char* endp = nullptr;
            long long want = std::strtoll(search + 1, &endp, 10);
            if (endp != search + 1)
                return static_cast<long long>(e.id) == want;
        }
        if (std::strstr(e.label, search)) return true;
        const char* tn = GetEntityTypeName(e.type);
        return tn && std::strstr(tn, search) != nullptr;
    };

    // ---- Collect filtered indices ----
    std::vector<int> show_idx;
    show_idx.reserve(world.entities.size());
    for (int i = 0; i < static_cast<int>(world.entities.size()); ++i)
        if (pass_filter(world.entities[i])) show_idx.push_back(i);

    // ---- Sort ----
    const Entity* player = nullptr;
    for (const auto& e : world.entities)
        if (static_cast<int>(e.id) == world.player_id && e.map_id == world.active_map_id) { player = &e; break; }

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
            char hdr[64];
            const char* gtn = GetEntityTypeName(static_cast<uint8_t>(gt));
            if (gtn) std::snprintf(hdr, sizeof(hdr), "%s  (%d)##grp%d", gtn, static_cast<int>(grp.size()), gt);
            else     std::snprintf(hdr, sizeof(hdr), u8"Type %d  (%d)##grp%d", gt, static_cast<int>(grp.size()), gt);
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
            char sec[80];
            const char* etn = GetEntityTypeName(e.type);
            if (etn) std::snprintf(sec, sizeof(sec), u8"基本信息 — %s  [%s]##basic", e.label, etn);
            else     std::snprintf(sec, sizeof(sec), u8"基本信息 — %s  [type %d]##basic", e.label, e.type);
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
                if (e.map_id != world.active_map_id) continue;
                if (e.x == sel_x && e.y == sel_y) { has_ov = true; break; }
            }
            if (has_ov &&
                ImGui::CollapsingHeader(u8"同坐标其他实体##ov", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& e : world.entities) {
                    if (static_cast<int>(e.id) == world.selected_id) continue;
                    if (e.map_id != world.active_map_id) continue;
                    if (e.x != sel_x || e.y != sel_y) continue;
                    ImGui::PushID(static_cast<int>(e.id));
                    char buf[64];
                    const char* ovtn = GetEntityTypeName(e.type);
                    if (ovtn) std::snprintf(buf, sizeof(buf), "[%s] %s", ovtn, e.label);
                    else      std::snprintf(buf, sizeof(buf), u8"[type %d] %s", e.type, e.label);
                    if (ImGui::Selectable(buf)) {
                        SelectClear(world);
                        SelectAdd(world, e.id);
                    }
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
                }
                ImGui::PopStyleColor(3);
                ImGui::PopID();
                ++n;
            }
            if (n == 0) ImGui::TextDisabled(u8"(此位置无格子)");
        }
        // Entities on the selected cell
        bool any_ent = false;
        for (const auto& e : world.entities)
            if (e.map_id == world.active_map_id && e.x == world.sel_cell_x && e.y == world.sel_cell_y) { any_ent = true; break; }
        if (any_ent &&
            ImGui::CollapsingHeader(u8"压在此格的实体##sce", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& e : world.entities) {
                if (e.map_id != world.active_map_id) continue;
                if (e.x != world.sel_cell_x || e.y != world.sel_cell_y) continue;
                bool is_sel = IsSelected(world, e.id);
                ImGui::PushID(static_cast<int>(e.id));
                char buf[64];
                const char* etn2 = GetEntityTypeName(e.type);
                if (etn2) std::snprintf(buf, sizeof(buf), "[%s] %s", etn2, e.label);
                else      std::snprintf(buf, sizeof(buf), u8"[type %d] %s", e.type, e.label);
                if (ImGui::Selectable(buf, is_sel)) {
                    if (is_sel) SelectClear(world);
                    else { SelectClear(world); SelectAdd(world, e.id); }
                }
                ImGui::PopID();
            }
        }
    }

    // ---- Cell list: collect unique types for combo, filter, sort (active map only) ----
    int  ctypes_all[32]; int nctypes_all = 0;
    for (const auto& c : world.cells) {
        if (c.map_id != world.active_map_id) continue;
        bool dup = false;
        for (int k = 0; k < nctypes_all; ++k)
            if (ctypes_all[k] == static_cast<int>(c.type)) { dup = true; break; }
        if (!dup && nctypes_all < 32) ctypes_all[nctypes_all++] = static_cast<int>(c.type);
    }
    for (int i = 0; i < nctypes_all; ++i)
        for (int j = i + 1; j < nctypes_all; ++j)
            if (ctypes_all[j] < ctypes_all[i]) { int t = ctypes_all[i]; ctypes_all[i] = ctypes_all[j]; ctypes_all[j] = t; }

    const char* cell_combo_items[33];
    cell_combo_items[0] = ALL_LABEL;
    char cell_type_bufs[32][32];
    for (int i = 0; i < nctypes_all; ++i) {
        const char* ctn = GetCellTypeName(static_cast<uint8_t>(ctypes_all[i]));
        if (ctn) std::snprintf(cell_type_bufs[i], sizeof(cell_type_bufs[i]), "%s", ctn);
        else     std::snprintf(cell_type_bufs[i], sizeof(cell_type_bufs[i]), "%d", ctypes_all[i]);
        cell_combo_items[i + 1] = cell_type_bufs[i];
    }
    int cell_combo_idx = 0;
    if (cell_type_filter != -1)
        for (int i = 0; i < nctypes_all; ++i)
            if (ctypes_all[i] == cell_type_filter) { cell_combo_idx = i + 1; break; }

    auto pass_cell = [&](const Cell& c) {
        if (c.map_id != world.active_map_id) return false;
        if (cell_type_filter != -1 && static_cast<int>(c.type) != cell_type_filter) return false;
        if (cell_search[0] == '\0') return true;
        if (std::strstr(c.label, cell_search)) return true;
        const char* ctn = GetCellTypeName(c.type);
        return ctn && std::strstr(ctn, cell_search) != nullptr;
    };

    std::vector<int> cell_show_idx;
    cell_show_idx.reserve(world.cells.size());
    for (int i = 0; i < static_cast<int>(world.cells.size()); ++i)
        if (pass_cell(world.cells[i])) cell_show_idx.push_back(i);

    std::sort(cell_show_idx.begin(), cell_show_idx.end(), [&](int ai, int bi) -> bool {
        const auto& a = world.cells[ai];
        const auto& b = world.cells[bi];
        switch (cell_sort_mode) {
            case 1: return a.type != b.type ? a.type < b.type : (a.y != b.y ? a.y < b.y : a.x < b.x);
            case 2: return std::strcmp(a.label, b.label) < 0;
            case 3: return a.y != b.y ? a.y < b.y : a.x < b.x;
            default: return ai < bi;
        }
    });

    // ---- Cell list header + toolbar ----
    ImGui::Separator();
    if (ImGui::CollapsingHeader(u8"地图格子##cell_list", 0)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 2.f));
        ImGui::SetNextItemWidth(130.f);
        ImGui::InputTextWithHint("##csearch", u8"搜索名称/类型", cell_search, sizeof(cell_search));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f);
        if (ImGui::Combo("##ctype", &cell_combo_idx, cell_combo_items, nctypes_all + 1))
            cell_type_filter = (cell_combo_idx == 0) ? -1 : ctypes_all[cell_combo_idx - 1];
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.f);
        const char* csort_items[] = { u8"默认", "Type", u8"名称", u8"坐标" };
        ImGui::Combo("##csort", &cell_sort_mode, csort_items, IM_ARRAYSIZE(csort_items));
        ImGui::SameLine();
        ImGui::Checkbox(u8"分组##cgrp", &cell_group);

        {
            char ccnt[32];
            std::snprintf(ccnt, sizeof(ccnt), "%d / %d",
                static_cast<int>(cell_show_idx.size()),
                static_cast<int>(world.cells.size()));
            float cnt_w = ImGui::CalcTextSize(ccnt).x + 4.f;
            float avail  = ImGui::GetContentRegionAvail().x;
            if (avail > cnt_w) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - cnt_w);
                ImGui::TextDisabled("%s", ccnt);
            }
        }

        ImGui::PopStyleVar();

        if (cell_show_idx.empty()) {
            ImGui::TextDisabled(u8"(无匹配项)");
        } else if (cell_group) {
            int cgtypes[32]; int ncgtypes = 0;
            for (int i : cell_show_idx) {
                int t = static_cast<int>(world.cells[i].type);
                bool dup = false;
                for (int k = 0; k < ncgtypes; ++k) if (cgtypes[k] == t) { dup = true; break; }
                if (!dup && ncgtypes < 32) cgtypes[ncgtypes++] = t;
            }
            for (int i = 0; i < ncgtypes; ++i)
                for (int j = i + 1; j < ncgtypes; ++j)
                    if (cgtypes[j] < cgtypes[i]) { int t = cgtypes[i]; cgtypes[i] = cgtypes[j]; cgtypes[j] = t; }

            for (int g = 0; g < ncgtypes; ++g) {
                int gt = cgtypes[g];
                std::vector<int> cgrp;
                for (int i : cell_show_idx)
                    if (static_cast<int>(world.cells[i].type) == gt)
                        cgrp.push_back(i);
                char hdr[64];
                const char* cgtn = GetCellTypeName(static_cast<uint8_t>(gt));
                if (cgtn) std::snprintf(hdr, sizeof(hdr), "%s  (%d)##cgrp%d", cgtn, static_cast<int>(cgrp.size()), gt);
                else      std::snprintf(hdr, sizeof(hdr), u8"Type %d  (%d)##cgrp%d", gt, static_cast<int>(cgrp.size()), gt);
                if (ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen)) {
                    char ctid[16];
                    std::snprintf(ctid, sizeof(ctid), "##ctg%d", gt);
                    CellTableImpl(world, cgrp, ctid, false);
                }
            }
        } else {
            CellTableImpl(world, cell_show_idx, "##cell_tbl", true);
        }
    }

    ImGui::End();
}

} // namespace dui
