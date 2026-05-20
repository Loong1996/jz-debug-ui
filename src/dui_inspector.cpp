#include "dui_inspector.h"
#include "dui_ext.h"
#include "dui_detail.h"
#include "dui_pins.h"
#include "dui_menubar.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <vector>

namespace dui {

// ---- Entity table -----------------------------------------------------------
static void EntityTableImpl(World& world, const std::vector<int>& idxs,
                             const char* tbl_id, bool with_scroll) {
    ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (with_scroll) flags |= ImGuiTableFlags_ScrollY;

    if (!ImGui::BeginTable(tbl_id, 3, flags, ImVec2(0.f, with_scroll ? 220.f : 0.f)))
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
        ImVec4 c4 = ImGui::ColorConvertU32ToFloat4(e.color); c4.w = 0.4f;
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::ColorConvertFloat4ToU32(c4));
        ImGui::PushID(static_cast<int>(e.id));
        if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_SpanAllColumns,
                              ImVec2(0.f, ImGui::GetTextLineHeight()))) {
            if (ImGui::GetIO().KeyCtrl) SelectToggle(world, e.id);
            else { SelectClear(world); if (!selected) SelectAdd(world, e.id); }
        }
        if (HasEntityContextMenu_(e.type) && ImGui::BeginPopupContextItem("##ectx")) {
            CanvasContextCtx cctx{ &world, ImGui::GetMousePos(), e.fx, e.fy, world.active_map_id };
            InvokeEntityContextMenu_(e, cctx);
            ImGui::EndPopup();
        }
        ImGui::SameLine(0.f, 0.f);
        { const char* tn = GetEntityTypeName(e.type); char fb[16];
          if (!tn) { std::snprintf(fb, sizeof(fb), "%u", static_cast<unsigned>(e.type)); tn = fb; }
          ImGui::TextUnformatted(tn); }
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(e.label);
        ImGui::TableSetColumnIndex(2); ImGui::Text("(%d,%d)", e.x, e.y);
        ImGui::PopID();
    }
    ImGui::EndTable();
}

// ---- Cell table -------------------------------------------------------------
static void CellTableImpl(World& world, const std::vector<int>& idxs,
                          const char* tbl_id, bool with_scroll) {
    ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (with_scroll) flags |= ImGuiTableFlags_ScrollY;

    if (!ImGui::BeginTable(tbl_id, 3, flags, ImVec2(0.f, with_scroll ? 220.f : 0.f)))
        return;
    if (with_scroll) ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Type",   ImGuiTableColumnFlags_WidthFixed,   44.f);
    ImGui::TableSetupColumn(u8"名称", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(u8"坐标", ImGuiTableColumnFlags_WidthFixed,   72.f);
    ImGui::TableHeadersRow();

    for (int i : idxs) {
        auto& c = world.cells[i];
        bool selected = world.sel_cell_valid && world.sel_cell_x == c.x && world.sel_cell_y == c.y;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImVec4 c4 = ImGui::ColorConvertU32ToFloat4(c.color); c4.w = 0.4f;
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::ColorConvertFloat4ToU32(c4));
        ImGui::PushID(i);
        if (ImGui::Selectable("##crow", selected, ImGuiSelectableFlags_SpanAllColumns,
                              ImVec2(0.f, ImGui::GetTextLineHeight()))) {
            world.sel_cell_valid = true; world.sel_cell_x = c.x; world.sel_cell_y = c.y;
        }
        ImGui::SameLine(0.f, 0.f);
        { const char* tn = GetCellTypeName(c.type); char fb[16];
          if (!tn) { std::snprintf(fb, sizeof(fb), "%u", static_cast<unsigned>(c.type)); tn = fb; }
          ImGui::TextUnformatted(tn); }
        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(c.label);
        ImGui::TableSetColumnIndex(2); ImGui::Text("(%d,%d)", c.x, c.y);
        ImGui::PopID();
    }
    ImGui::EndTable();
}

// ---- Entity list tab --------------------------------------------------------
static void draw_entity_list_tab(World& world) {
    static char search[64]    = {};
    static int  type_filter   = -1;
    static int  sort_mode     = 0;
    static bool group_by_type = false;

    int types[32]; int ntypes = 0;
    for (const auto& e : world.entities) {
        if (e.map_id != world.active_map_id) continue;
        bool dup = false;
        for (int k = 0; k < ntypes; ++k) if (types[k] == static_cast<int>(e.type)) { dup = true; break; }
        if (!dup && ntypes < 32) types[ntypes++] = static_cast<int>(e.type);
    }
    for (int i = 0; i < ntypes; ++i)
        for (int j = i+1; j < ntypes; ++j)
            if (types[j] < types[i]) { int t = types[i]; types[i] = types[j]; types[j] = t; }

    static const char* ALL = u8"全部";
    const char* combo_items[33]; combo_items[0] = ALL;
    char type_bufs[32][32];
    for (int i = 0; i < ntypes; ++i) {
        const char* tn = GetEntityTypeName(static_cast<uint8_t>(types[i]));
        if (tn) std::snprintf(type_bufs[i], sizeof(type_bufs[i]), "%s", tn);
        else    std::snprintf(type_bufs[i], sizeof(type_bufs[i]), "%d", types[i]);
        combo_items[i+1] = type_bufs[i];
    }
    int combo_idx = 0;
    if (type_filter != -1)
        for (int i = 0; i < ntypes; ++i)
            if (types[i] == type_filter) { combo_idx = i+1; break; }

    auto pass = [&](const Entity& e) {
        if (e.map_id != world.active_map_id) return false;
        if (type_filter != -1 && static_cast<int>(e.type) != type_filter) return false;
        if (!search[0]) return true;
        if (search[0] == '#') {
            char* ep = nullptr; long long w = std::strtoll(search+1, &ep, 10);
            if (ep != search+1) return static_cast<long long>(e.id) == w;
        }
        if (std::strstr(e.label, search)) return true;
        const char* tn = GetEntityTypeName(e.type);
        return tn && std::strstr(tn, search);
    };

    std::vector<int> idx;
    idx.reserve(world.entities.size());
    for (int i = 0; i < static_cast<int>(world.entities.size()); ++i)
        if (pass(world.entities[i])) idx.push_back(i);

    const Entity* player = nullptr;
    for (const auto& e : world.entities)
        if (e.id == world.player_id && e.map_id == world.active_map_id) { player = &e; break; }

    std::sort(idx.begin(), idx.end(), [&](int ai, int bi) {
        const auto& a = world.entities[ai]; const auto& b = world.entities[bi];
        switch (sort_mode) {
            case 1: return a.id < b.id;
            case 2: return a.type != b.type ? a.type < b.type : a.id < b.id;
            case 3: return std::strcmp(a.label, b.label) < 0;
            case 4: if (!player) return a.id < b.id; {
                float dax=a.fx-player->fx, day=a.fy-player->fy;
                float dbx=b.fx-player->fx, dby=b.fy-player->fy;
                return dax*dax+day*day < dbx*dbx+dby*dby; }
            default: return ai < bi;
        }
    });

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 2.f));
    ImGui::SetNextItemWidth(130.f);
    ImGui::InputTextWithHint("##esrch", u8"搜索 (#id 或名称)", search, sizeof(search));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.f);
    if (ImGui::Combo("##etype", &combo_idx, combo_items, ntypes+1))
        type_filter = combo_idx == 0 ? -1 : types[combo_idx-1];
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.f);
    const char* sorts[] = { u8"默认","ID","Type",u8"名称",u8"距主角" };
    ImGui::Combo("##esort", &sort_mode, sorts, IM_ARRAYSIZE(sorts));
    ImGui::SameLine(); ImGui::Checkbox(u8"分组", &group_by_type);
    { char cnt[32]; std::snprintf(cnt, sizeof(cnt), "%d/%d",
          static_cast<int>(idx.size()), static_cast<int>(world.entities.size()));
      float cw = ImGui::CalcTextSize(cnt).x + 4.f;
      if (ImGui::GetContentRegionAvail().x > cw) {
          ImGui::SameLine();
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - cw);
          ImGui::TextDisabled("%s", cnt); } }
    ImGui::PopStyleVar();

    if (idx.empty()) { ImGui::TextDisabled(u8"(无匹配项)"); return; }
    if (!group_by_type) { EntityTableImpl(world, idx, "##et", true); return; }

    int gtypes[32]; int ngt = 0;
    for (int i : idx) { int t = static_cast<int>(world.entities[i].type);
        bool dup = false; for (int k=0;k<ngt;++k) if(gtypes[k]==t){dup=true;break;}
        if (!dup && ngt<32) gtypes[ngt++]=t; }
    for (int i=0;i<ngt;++i) for (int j=i+1;j<ngt;++j)
        if(gtypes[j]<gtypes[i]){int t=gtypes[i];gtypes[i]=gtypes[j];gtypes[j]=t;}
    for (int g=0;g<ngt;++g) {
        int gt=gtypes[g]; std::vector<int> grp;
        for (int i:idx) if(static_cast<int>(world.entities[i].type)==gt) grp.push_back(i);
        char hdr[64]; const char* gtn = GetEntityTypeName(static_cast<uint8_t>(gt));
        if (gtn) std::snprintf(hdr,sizeof(hdr),"%s (%d)##grp%d",gtn,static_cast<int>(grp.size()),gt);
        else     std::snprintf(hdr,sizeof(hdr),u8"Type %d (%d)##grp%d",gt,static_cast<int>(grp.size()),gt);
        if (ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen)) {
            char tid[16]; std::snprintf(tid,sizeof(tid),"##tg%d",gt);
            EntityTableImpl(world,grp,tid,false); }
    }
}

// ---- Cell list tab ----------------------------------------------------------
static void draw_cell_list_tab(World& world) {
    static char search[64] = {};
    static bool group      = false;
    static int  tf         = -1;
    static int  sort       = 0;

    int ct[32]; int nct=0;
    for (const auto& c : world.cells) {
        if (c.map_id != world.active_map_id) continue;
        bool dup=false; for(int k=0;k<nct;++k) if(ct[k]==static_cast<int>(c.type)){dup=true;break;}
        if (!dup && nct<32) ct[nct++]=static_cast<int>(c.type);
    }
    for(int i=0;i<nct;++i) for(int j=i+1;j<nct;++j)
        if(ct[j]<ct[i]){int t=ct[i];ct[i]=ct[j];ct[j]=t;}

    static const char* ALL=u8"全部";
    const char* ci[33]; ci[0]=ALL;
    char tb[32][32];
    for(int i=0;i<nct;++i){
        const char* ctn=GetCellTypeName(static_cast<uint8_t>(ct[i]));
        if(ctn) std::snprintf(tb[i],sizeof(tb[i]),"%s",ctn);
        else    std::snprintf(tb[i],sizeof(tb[i]),"%d",ct[i]);
        ci[i+1]=tb[i]; }
    int cidx=0;
    if(tf!=-1) for(int i=0;i<nct;++i) if(ct[i]==tf){cidx=i+1;break;}

    auto pass=[&](const Cell& c){
        if(c.map_id!=world.active_map_id) return false;
        if(tf!=-1 && static_cast<int>(c.type)!=tf) return false;
        if(!search[0]) return true;
        if(std::strstr(c.label,search)) return true;
        const char* ctn=GetCellTypeName(c.type);
        return ctn && std::strstr(ctn,search); };

    std::vector<int> idx; idx.reserve(world.cells.size());
    for(int i=0;i<static_cast<int>(world.cells.size());++i)
        if(pass(world.cells[i])) idx.push_back(i);

    std::sort(idx.begin(),idx.end(),[&](int ai,int bi){
        const auto& a=world.cells[ai]; const auto& b=world.cells[bi];
        switch(sort){
            case 1: return a.type!=b.type?a.type<b.type:(a.y!=b.y?a.y<b.y:a.x<b.x);
            case 2: return std::strcmp(a.label,b.label)<0;
            case 3: return a.y!=b.y?a.y<b.y:a.x<b.x;
            default: return ai<bi; } });

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(4.f,2.f));
    ImGui::SetNextItemWidth(130.f);
    ImGui::InputTextWithHint("##csrch",u8"搜索名称/类型",search,sizeof(search));
    ImGui::SameLine(); ImGui::SetNextItemWidth(80.f);
    if(ImGui::Combo("##ctype",&cidx,ci,nct+1)) tf=cidx==0?-1:ct[cidx-1];
    ImGui::SameLine(); ImGui::SetNextItemWidth(100.f);
    const char* sorts[]={u8"默认","Type",u8"名称",u8"坐标"};
    ImGui::Combo("##csort",&sort,sorts,IM_ARRAYSIZE(sorts));
    ImGui::SameLine(); ImGui::Checkbox(u8"分组##cgrp",&group);
    { char cnt[32]; std::snprintf(cnt,sizeof(cnt),"%d/%d",
          static_cast<int>(idx.size()),static_cast<int>(world.cells.size()));
      float cw=ImGui::CalcTextSize(cnt).x+4.f;
      if(ImGui::GetContentRegionAvail().x>cw){
          ImGui::SameLine();
          ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-cw);
          ImGui::TextDisabled("%s",cnt); } }
    ImGui::PopStyleVar();

    if(idx.empty()){ImGui::TextDisabled(u8"(无匹配项)");return;}
    if(!group){CellTableImpl(world,idx,"##ct",true);return;}

    int cgt[32]; int ncgt=0;
    for(int i:idx){ int t=static_cast<int>(world.cells[i].type);
        bool dup=false; for(int k=0;k<ncgt;++k) if(cgt[k]==t){dup=true;break;}
        if(!dup&&ncgt<32) cgt[ncgt++]=t; }
    for(int i=0;i<ncgt;++i) for(int j=i+1;j<ncgt;++j)
        if(cgt[j]<cgt[i]){int t=cgt[i];cgt[i]=cgt[j];cgt[j]=t;}
    for(int g=0;g<ncgt;++g){
        int gt=cgt[g]; std::vector<int> cgrp;
        for(int i:idx) if(static_cast<int>(world.cells[i].type)==gt) cgrp.push_back(i);
        char hdr[64]; const char* cgtn=GetCellTypeName(static_cast<uint8_t>(gt));
        if(cgtn) std::snprintf(hdr,sizeof(hdr),"%s (%d)##cgrp%d",cgtn,static_cast<int>(cgrp.size()),gt);
        else     std::snprintf(hdr,sizeof(hdr),u8"Type %d (%d)##cgrp%d",gt,static_cast<int>(cgrp.size()),gt);
        if(ImGui::CollapsingHeader(hdr,ImGuiTreeNodeFlags_DefaultOpen)){
            char ctid[16]; std::snprintf(ctid,sizeof(ctid),"##ctg%d",gt);
            CellTableImpl(world,cgrp,ctid,false); }
    }
}

// ---- DrawInspector ----------------------------------------------------------

void DrawInspector(World& world) {
    if (!ImGui::Begin(u8"检视器", &BuiltinPanelOpenRef(u8"检视器")))
        { ImGui::End(); return; }

    // Map selector
    {
        uint32_t mids[64]; int nm=0;
        for(const auto& e:world.entities){ bool dup=false;
            for(int k=0;k<nm;++k) if(mids[k]==e.map_id){dup=true;break;}
            if(!dup&&nm<64) mids[nm++]=e.map_id; }
        for(const auto& c:world.cells){ bool dup=false;
            for(int k=0;k<nm;++k) if(mids[k]==c.map_id){dup=true;break;}
            if(!dup&&nm<64) mids[nm++]=c.map_id; }
        for(int i=0;i<nm;++i) for(int j=i+1;j<nm;++j)
            if(mids[j]<mids[i]){uint32_t t=mids[i];mids[i]=mids[j];mids[j]=t;}
        char mbufs[64][48]; const char* mitems[64]; int mcidx=0;
        for(int i=0;i<nm;++i){
            int ec=0,cc=0;
            for(const auto& e:world.entities) if(e.map_id==mids[i]) ++ec;
            for(const auto& c:world.cells)    if(c.map_id==mids[i]) ++cc;
            const char* mn=GetMapName(mids[i]);
            if(mn) std::snprintf(mbufs[i],sizeof(mbufs[i]),u8"%s [%u]  实体%d  格%d",mn,mids[i],ec,cc);
            else   std::snprintf(mbufs[i],sizeof(mbufs[i]),u8"Map %u  实体%d  格%d",mids[i],ec,cc);
            mitems[i]=mbufs[i];
            if(mids[i]==world.active_map_id) mcidx=i; }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(4.f,2.f));
        ImGui::TextUnformatted(u8"地图"); ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.f);
        if(ImGui::Combo("##imap",&mcidx,mitems,nm)&&nm>0)
            SwitchActiveMap(world,mids[mcidx]);
        ImGui::PopStyleVar();
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("##insp_tabs")) {
        if (ImGui::BeginTabItem(u8"实体列表")) {
            draw_entity_list_tab(world); ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"格子列表")) {
            draw_cell_list_tab(world); ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"标注")) {
            DrawPinsPanel_(world); ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace dui
