#include "dui_canvas.h"
#include "dui_ext.h"
#include "dui_trails.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace dui {

constexpr float kViewHalf = 18.f;
constexpr float kZoomMin  = 0.25f;
constexpr float kZoomMax  = 4.0f;

void DrawCanvas(World& world, CanvasView* view) {
    static CanvasView s_default_view;
    if (!view) view = &s_default_view;

    // Reset camera when active map changes
    static uint32_t s_last_drawn_map_id = ~0u;
    if (world.active_map_id != s_last_drawn_map_id) {
        view->cam_x = view->cam_y = 0.f;
        view->zoom  = 1.f;
        s_last_drawn_map_id = world.active_map_id;
    }

    ImGui::Begin(u8"场景视图");

    // --- Find player ---
    const Entity* player = nullptr;
    for (const auto& e : world.entities)
        if (static_cast<int>(e.id) == world.player_id && e.map_id == world.active_map_id)
            { player = &e; break; }

    // --- Follow mode: track selected entity, fall back to player ---
    if (view->follow_player) {
        const Entity* target = nullptr;
        if (world.selected_id != -1)
            for (const auto& e : world.entities)
                if (static_cast<int>(e.id) == world.selected_id && e.map_id == world.active_map_id)
                    { target = &e; break; }
        if (!target) target = player;
        if (target) { view->cam_x = target->fx; view->cam_y = target->fy; }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

    // === Toolbar row 1: map selector + visibility toggles ===
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
                if (all_map_ids[j] < all_map_ids[i]) {
                    uint32_t t = all_map_ids[i]; all_map_ids[i] = all_map_ids[j]; all_map_ids[j] = t;
                }
        char map_bufs[64][32]; const char* map_items[64];
        int  map_combo_idx = 0;
        for (int i = 0; i < n_all_maps; ++i) {
            const char* mn = GetMapName(all_map_ids[i]);
            if (mn) std::snprintf(map_bufs[i], sizeof(map_bufs[i]), "%s [%u]", mn, all_map_ids[i]);
            else    std::snprintf(map_bufs[i], sizeof(map_bufs[i]), "Map %u", all_map_ids[i]);
            map_items[i] = map_bufs[i];
            if (all_map_ids[i] == world.active_map_id) map_combo_idx = i;
        }
        ImGui::SetNextItemWidth(110.f);
        if (ImGui::Combo("##cvmap", &map_combo_idx, map_items, n_all_maps) && n_all_maps > 0)
            SwitchActiveMap(world, all_map_ids[map_combo_idx]);
        ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
    }
    ImGui::Checkbox(u8"格线",   &view->show_grid);   ImGui::SameLine();
    ImGui::Checkbox(u8"格子",   &view->show_cells);  ImGui::SameLine();
    ImGui::Checkbox(u8"实体",   &view->show_ents);   ImGui::SameLine();
    ImGui::Checkbox(u8"标签",   &view->show_labels); ImGui::SameLine();
    ImGui::Checkbox(u8"坐标轴", &view->show_axis);   ImGui::SameLine();
    ImGui::Checkbox(u8"轨迹",   &view->show_trails); ImGui::SameLine();
    ImGui::Checkbox(u8"连线",   &view->show_links);  ImGui::SameLine();
    ImGui::Checkbox(u8"热力图", &view->show_heatmaps);

    // === Toolbar row 2: camera controls ===
    if (ImGui::RadioButton(u8"跟随",  view->follow_player))  view->follow_player = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(u8"自由", !view->follow_player))  view->follow_player = false;
    ImGui::SameLine();
    if (ImGui::Button(u8"重置")) {
        if (player) { view->cam_x = player->fx; view->cam_y = player->fy; }
        else        { view->cam_x = view->cam_y = 0.f; }
        view->zoom = 1.f;
    }
    ImGui::SameLine();
    static bool s_do_fit = false;
    if (ImGui::Button("Fit")) s_do_fit = true;
    ImGui::SameLine();
    {
        char zoom_buf[16];
        std::snprintf(zoom_buf, sizeof(zoom_buf), "%.0f%%", view->zoom * 100.f);
        if (ImGui::Button(zoom_buf)) view->zoom = 1.f;
    }

    ImGui::PopStyleVar();

    // --- Canvas area ---
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz  = ImGui::GetContentRegionAvail();
    if (sz.x < 50.f) sz.x = 50.f;
    if (sz.y < 50.f) sz.y = 50.f;

    ImGui::InvisibleButton("canvas_area", sz,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();

    const ImVec2 center(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
    const float  sz_min  = sz.x < sz.y ? sz.x : sz.y;
    const float  th_base = sz_min * 0.5f / (2.f * kViewHalf + 3.f);

    // --- Fit-to-extents (triggered by toolbar Fit button) ---
    if (s_do_fit) {
        s_do_fit = false;
        float bx0 = 1e9f, bx1 = -1e9f, by0 = 1e9f, by1 = -1e9f;
        bool any = false;
        for (const auto& e : world.entities) if (e.map_id == world.active_map_id) {
            bx0 = fminf(bx0, e.fx); bx1 = fmaxf(bx1, e.fx);
            by0 = fminf(by0, e.fy); by1 = fmaxf(by1, e.fy); any = true;
        }
        for (const auto& c : world.cells) if (c.map_id == world.active_map_id) {
            float cx = static_cast<float>(c.x), cy = static_cast<float>(c.y);
            bx0 = fminf(bx0, cx); bx1 = fmaxf(bx1, cx);
            by0 = fminf(by0, cy); by1 = fmaxf(by1, cy); any = true;
        }
        if (any) {
            view->cam_x = (bx0 + bx1) * 0.5f;
            view->cam_y = (by0 + by1) * 0.5f;
            float hbx = (bx1 - bx0) * 0.5f + 1.f;
            float hby = (by1 - by0) * 0.5f + 1.f;
            view->zoom = std::max(kZoomMin, std::min(kZoomMax,
                sz_min * 0.5f * 0.85f / ((hbx + hby) * th_base)));
        } else {
            view->cam_x = view->cam_y = 0.f;
            view->zoom = 1.f;
        }
        view->follow_player = false;
    }

    // --- Scroll-wheel zoom (anchor at mouse in free mode; zoom-only in follow mode) ---
    if (hovered && ImGui::GetIO().MouseWheel != 0.f) {
        float factor = ImGui::GetIO().MouseWheel > 0.f ? 1.1f : (1.f / 1.1f);
        if (view->follow_player) {
            view->zoom = std::max(kZoomMin, std::min(kZoomMax, view->zoom * factor));
        } else {
            ImVec2 mp  = ImGui::GetIO().MousePos;
            float  mrx = mp.x - center.x, mry = mp.y - center.y;
            float  old_th = th_base * view->zoom;
            float  wx0 = view->cam_x + (mrx + mry) / (2.f * old_th);
            float  wy0 = view->cam_y + (mry - mrx) / (2.f * old_th);
            view->zoom = std::max(kZoomMin, std::min(kZoomMax, view->zoom * factor));
            float  new_th = th_base * view->zoom;
            view->cam_x = wx0 - (mrx + mry) / (2.f * new_th);
            view->cam_y = wy0 - (mry - mrx) / (2.f * new_th);
        }
    }

    // --- Middle-drag pan (exits follow mode) ---
    static ImVec2 s_mid_start_mouse = {0.f, 0.f};
    static float  s_mid_start_cam_x = 0.f, s_mid_start_cam_y = 0.f;
    static bool   s_mid_dragging    = false;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        s_mid_start_mouse = ImGui::GetIO().MousePos;
        s_mid_start_cam_x = view->cam_x;
        s_mid_start_cam_y = view->cam_y;
        s_mid_dragging    = true;
    }
    if (s_mid_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        view->follow_player = false;
        ImVec2 mp  = ImGui::GetIO().MousePos;
        float  dpx = mp.x - s_mid_start_mouse.x;
        float  dpy = mp.y - s_mid_start_mouse.y;
        float  cur_th = th_base * view->zoom;
        view->cam_x = s_mid_start_cam_x - (dpx + dpy) / (2.f * cur_th);
        view->cam_y = s_mid_start_cam_y - (dpy - dpx) / (2.f * cur_th);
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        s_mid_dragging = false;
    }

    // --- Right-drag pan (free mode only) ---
    static float s_rem_x = 0.f, s_rem_y = 0.f;
    if (!view->follow_player && active && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        float  cur_th = th_base * view->zoom;
        ImVec2 md = ImGui::GetIO().MouseDelta;
        s_rem_x -= (md.x + md.y) / (2.f * cur_th);
        s_rem_y -= (md.y - md.x) / (2.f * cur_th);
        int ix = static_cast<int>(s_rem_x); view->cam_x += ix; s_rem_x -= static_cast<float>(ix);
        int iy = static_cast<int>(s_rem_y); view->cam_y += iy; s_rem_y -= static_cast<float>(iy);
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        s_rem_x = s_rem_y = 0.f;
    }

    // --- Stable per-frame camera state ---
    const float fcx = view->cam_x;
    const float fcy = view->cam_y;
    const float th  = th_base * view->zoom;
    const int   ccx = static_cast<int>(roundf(fcx));
    const int   ccy = static_cast<int>(roundf(fcy));

    auto ToScreen = [&](float wx, float wy) -> ImVec2 {
        float dx = wx - fcx, dy = wy - fcy;
        return ImVec2(center.x + (dx - dy) * th, center.y + (dx + dy) * th);
    };

    SetCanvasViewState_(fcx, fcy, th, center);

    auto ToWorld = [&](ImVec2 s, int& ox, int& oy) {
        float rx = s.x - center.x, ry = s.y - center.y;
        ox = static_cast<int>(roundf(fcx + (rx + ry) / (2.f * th)));
        oy = static_cast<int>(roundf(fcy + (ry - rx) / (2.f * th)));
    };

    auto TileDiamond = [&](float wx, float wy, ImVec2 pts[4]) {
        float dx = wx - fcx, dy = wy - fcy;
        float cx = center.x + (dx - dy) * th;
        float cy = center.y + (dx + dy) * th;
        pts[0] = ImVec2(cx,      cy - th);
        pts[1] = ImVec2(cx + th, cy);
        pts[2] = ImVec2(cx,      cy + th);
        pts[3] = ImVec2(cx - th, cy);
    };

    auto InView = [&](int wx, int wy) {
        return abs(wx - ccx) <= static_cast<int>(kViewHalf) &&
               abs(wy - ccy) <= static_cast<int>(kViewHalf);
    };

    // --- Pre-scan hover hits: single pass reused by highlights and tooltip ---
    int hover_wx = 0, hover_wy = 0;
    std::vector<int> hover_entity_idx, hover_cell_idx;
    if (hovered) {
        ToWorld(ImGui::GetIO().MousePos, hover_wx, hover_wy);
        for (int i = 0; i < static_cast<int>(world.entities.size()); ++i) {
            const auto& e = world.entities[i];
            if (e.map_id == world.active_map_id && e.x == hover_wx && e.y == hover_wy)
                hover_entity_idx.push_back(i);
        }
        for (int i = 0; i < static_cast<int>(world.cells.size()); ++i) {
            const auto& c = world.cells[i];
            if (c.map_id == world.active_map_id && c.x == hover_wx && c.y == hover_wy)
                hover_cell_idx.push_back(i);
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 32, 255));
    dl->PushClipRect(p0, p1, true);

    // --- 1. Map cells ---
    if (view->show_cells) {
        for (int i = 0; i < static_cast<int>(world.cells.size()); ++i) {
            const auto& c = world.cells[i];
            if (c.map_id != world.active_map_id) continue;
            ImVec2 pts[4];
            TileDiamond(static_cast<float>(c.x), static_cast<float>(c.y), pts);
            dl->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], c.color);
        }
        // Hover highlight on hovered cells
        for (int idx : hover_cell_idx) {
            const auto& c = world.cells[idx];
            ImVec2 pts[4];
            TileDiamond(static_cast<float>(c.x), static_cast<float>(c.y), pts);
            dl->AddQuad(pts[0], pts[1], pts[2], pts[3], IM_COL32(255, 255, 255, 160), 2.f);
        }

        // --- 1b. Per-type cell overlays ---
        for (const auto& c : world.cells) {
            if (c.map_id != world.active_map_id) continue;
            InvokeCellOverlays_(world, c, dl);
        }
    }

    // --- 1c. Heatmaps — covers entire viewport, independent of show_cells ---
    if (view->show_heatmaps)
        InvokeCellHeatmaps_(world, dl, ccx, ccy, static_cast<int>(kViewHalf));

    // --- 1d. Entity trails (below entity sprites) ---
    if (view->show_trails && IsEntityTrailsEnabled()) InvokeTrails_(world, dl);

    // --- 2. Grid lines ---
    if (view->show_grid) {
        int gr = static_cast<int>((sz.x + sz.y) / (2.f * th)) + 2;
        for (int i = ccx - gr; i <= ccx + gr + 1; ++i) {
            float wx = static_cast<float>(i) - 0.5f;
            ImVec2 a = ToScreen(wx, static_cast<float>(ccy - gr) - 0.5f);
            ImVec2 b = ToScreen(wx, static_cast<float>(ccy + gr) + 0.5f);
            dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
        }
        for (int j = ccy - gr; j <= ccy + gr + 1; ++j) {
            float wy = static_cast<float>(j) - 0.5f;
            ImVec2 a = ToScreen(static_cast<float>(ccx - gr) - 0.5f, wy);
            ImVec2 b = ToScreen(static_cast<float>(ccx + gr) + 0.5f, wy);
            dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
        }
    }

    // --- 2b. View boundary indicator (orange diamond, game-world range) ---
    {
        float bcx = player ? player->fx : fcx;
        float bcy = player ? player->fy : fcy;
        if (world.selected_id != -1)
            for (const auto& e : world.entities)
                if (static_cast<int>(e.id) == world.selected_id && e.map_id == world.active_map_id)
                    { bcx = e.fx; bcy = e.fy; break; }
        float hf = kViewHalf + 0.5f;
        ImVec2 bnd[4] = {
            ToScreen(bcx - hf, bcy - hf),
            ToScreen(bcx + hf, bcy - hf),
            ToScreen(bcx + hf, bcy + hf),
            ToScreen(bcx - hf, bcy + hf),
        };
        dl->AddQuad(bnd[0], bnd[1], bnd[2], bnd[3], IM_COL32(255, 140, 0, 220), 2.f);
    }

    // --- 3. Center cell highlight ---
    {
        ImVec2 pts[4];
        TileDiamond(static_cast<float>(ccx), static_cast<float>(ccy), pts);
        dl->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], IM_COL32(50, 50, 75, 255));
    }

    // --- 4. Entities ---
    if (view->show_ents) {
        for (int i = 0; i < static_cast<int>(world.entities.size()); ++i) {
            const auto& e = world.entities[i];
            if (e.map_id != world.active_map_id) continue;
            ImVec2 pts[4];
            TileDiamond(e.fx, e.fy, pts);

            float r = e.radius < 1.f ? e.radius : 1.f;
            ImVec2 cpt((pts[0].x + pts[2].x) * 0.5f, (pts[0].y + pts[2].y) * 0.5f);
            ImVec2 scaled[4];
            for (int k = 0; k < 4; ++k)
                scaled[k] = ImVec2(cpt.x + (pts[k].x - cpt.x) * r,
                                   cpt.y + (pts[k].y - cpt.y) * r);
            dl->AddQuadFilled(scaled[0], scaled[1], scaled[2], scaled[3], e.color);

            // Selected outline: bright yellow for primary, dimmer for multi-select
            if (IsSelected(world, e.id)) {
                bool primary = static_cast<int>(e.id) == world.selected_id;
                dl->AddQuad(pts[0], pts[1], pts[2], pts[3],
                            primary ? IM_COL32(255, 230, 0, 255)
                                    : IM_COL32(255, 185, 0, 140),
                            primary ? 2.f : 1.f);
            }

            // Hover ring (white)
            for (int hi : hover_entity_idx) {
                if (hi == i) {
                    dl->AddQuad(pts[0], pts[1], pts[2], pts[3], IM_COL32(255, 255, 255, 200), 2.f);
                    break;
                }
            }

            // Entity marker: per-entity color takes priority; falls back to player-type yellow.
            {
                const uint32_t* mc = GetEntityMarker(e.id);
                uint32_t col = mc ? *mc
                             : IsPlayerEntityType(e.type) ? IM_COL32(255, 220, 80, 230)
                             : 0u;
                if (col) {
                    float ts     = fmaxf(th * 0.35f, 4.f);
                    float tip_y  = pts[0].y - ts * 0.4f;
                    float base_y = tip_y - ts * 1.2f;
                    dl->AddTriangleFilled(
                        ImVec2(pts[0].x - ts, base_y),
                        ImVec2(pts[0].x + ts, base_y),
                        ImVec2(pts[0].x,      tip_y),
                        col);
                }
            }

            // Label
            if (view->show_labels && th >= 10.f) {
                std::string lbl = InvokeEntityLabel(e);
                if (!lbl.empty()) {
                    float lw = ImGui::CalcTextSize(lbl.c_str()).x;
                    dl->AddText(ImVec2(pts[0].x - lw * 0.5f,
                                       pts[0].y - ImGui::GetFontSize() - 2.f),
                                IM_COL32(210, 210, 210, 220), lbl.c_str());
                }
            }
        }
    }

    // --- 5. Selected cell cyan outline ---
    if (world.sel_cell_valid && InView(world.sel_cell_x, world.sel_cell_y)) {
        ImVec2 pts[4];
        TileDiamond(static_cast<float>(world.sel_cell_x), static_cast<float>(world.sel_cell_y), pts);
        dl->AddQuad(pts[0], pts[1], pts[2], pts[3], IM_COL32(50, 220, 255, 255), 2.f);
    }

    // --- 5b. Entity + global overlays ---
    if (view->show_ents) {
        for (const auto& e : world.entities) {
            if (e.map_id != world.active_map_id) continue;
            InvokeEntityOverlays_(world, e, dl);
        }
    }
    InvokeGlobalOverlays_(world, dl);

    // --- 5c. Entity links and cell links ---
    if (view->show_links) {
        InvokeEntityLinks_(world, dl);
        InvokeCellLinks_  (world, dl);
    }

    // --- 6. Axis indicator ---
    dl->PopClipRect();
    if (view->show_axis) {
        const float ox = p0.x + 36.f, oy = p0.y + sz.y - 18.f;
        const float ar = 26.f, k = 0.7071f;
        dl->AddLine(ImVec2(ox, oy), ImVec2(ox + ar*k, oy + ar*k), IM_COL32(240, 100, 100, 200), 1.5f);
        dl->AddText(ImVec2(ox + ar*k + 2.f, oy + ar*k - 7.f), IM_COL32(240, 100, 100, 200), "+X");
        dl->AddLine(ImVec2(ox, oy), ImVec2(ox - ar*k, oy + ar*k), IM_COL32(100, 210, 100, 200), 1.5f);
        dl->AddText(ImVec2(ox - ar*k - 22.f, oy + ar*k - 7.f), IM_COL32(100, 210, 100, 200), "+Y");
    }

    // --- Map watermark (foreground draw list, not clipped by canvas) ---
    {
        ImDrawList* fg  = ImGui::GetForegroundDrawList();
        const char* mn  = GetMapName(world.active_map_id);
        char        buf[48];
        if (mn) std::snprintf(buf, sizeof(buf), "%s [%u]", mn, world.active_map_id);
        else    std::snprintf(buf, sizeof(buf), "Map %u", world.active_map_id);
        fg->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.5f,
                    ImVec2(p0.x + 8.f, p0.y + 8.f),
                    IM_COL32(200, 200, 220, 77), buf);
    }

    // --- Cursor world coordinate HUD ---
    if (hovered) {
        char coord_buf[32];
        std::snprintf(coord_buf, sizeof(coord_buf), "(%d, %d)", hover_wx, hover_wy);
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(p0.x + 4.f, p0.y + sz.y - ImGui::GetFontSize() - 4.f),
            IM_COL32(200, 200, 200, 180), coord_buf);
    }

    // --- Click selection with overlap popup ---
    struct Hit { int kind; int index; };
    static bool s_lmb_pan      = false;
    static int  s_popup_wx     = 0, s_popup_wy = 0;
    static int  s_popup_nhits  = 0, s_popup_nskipped = 0;
    static Hit  s_popup_hits[32];

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int wx, wy;
        ToWorld(ImGui::GetIO().MousePos, wx, wy);
        s_lmb_pan = false;

        Hit hits[32];
        int nhits = 0, nskipped = 0;
        for (int i = 0; i < static_cast<int>(world.entities.size()); ++i) {
            const auto& e = world.entities[i];
            if (e.map_id == world.active_map_id && e.x == wx && e.y == wy) {
                if (nhits < 32) hits[nhits++] = { 0, i };
                else nskipped++;
            }
        }
        for (int i = 0; i < static_cast<int>(world.cells.size()); ++i) {
            const auto& c = world.cells[i];
            if (c.map_id == world.active_map_id && c.x == wx && c.y == wy) {
                if (nhits < 32) hits[nhits++] = { 1, i };
                else nskipped++;
            }
        }

        if (nhits == 0) {
            s_lmb_pan = true;
        } else if (nhits == 1 && nskipped == 0) {
            if (hits[0].kind == 0) {
                const auto& e = world.entities[hits[0].index];
                SelectClear(world);
                SelectAdd(world, e.id);
                view->cam_x         = e.fx;
                view->cam_y         = e.fy;
                view->follow_player = true;
            } else {
                const auto& c        = world.cells[hits[0].index];
                world.sel_cell_valid = true;
                world.sel_cell_x     = c.x;
                world.sel_cell_y     = c.y;
            }
        } else {
            s_popup_wx       = wx;
            s_popup_wy       = wy;
            s_popup_nhits    = nhits;
            s_popup_nskipped = nskipped;
            std::memcpy(s_popup_hits, hits, sizeof(Hit) * static_cast<size_t>(nhits));
            ImGui::OpenPopup("##overlap_pick");
        }
    }

    // Left-drag on background pans camera
    static float s_lrem_x = 0.f, s_lrem_y = 0.f;
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && s_lmb_pan) {
        view->follow_player = false;
        ImVec2 md = ImGui::GetIO().MouseDelta;
        s_lrem_x -= (md.x + md.y) / (2.f * th);
        s_lrem_y -= (md.y - md.x) / (2.f * th);
        int ix = static_cast<int>(s_lrem_x); view->cam_x += ix; s_lrem_x -= static_cast<float>(ix);
        int iy = static_cast<int>(s_lrem_y); view->cam_y += iy; s_lrem_y -= static_cast<float>(iy);
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        s_lmb_pan = false;
        s_lrem_x  = s_lrem_y = 0.f;
    }

    // --- Overlap selection popup ---
    if (ImGui::BeginPopup("##overlap_pick")) {
        ImGui::TextDisabled(u8"(%d, %d)  —  %d 个对象", s_popup_wx, s_popup_wy, s_popup_nhits);
        ImGui::Separator();
        for (int i = 0; i < s_popup_nhits; ++i) {
            const Hit& h = s_popup_hits[i];
            char buf[96];
            if (h.kind == 0) {
                const auto& e  = world.entities[h.index];
                const char* tn = GetEntityTypeName(e.type);
                char        tbuf[16];
                if (!tn) { std::snprintf(tbuf, sizeof(tbuf), "%u", static_cast<unsigned>(e.type)); tn = tbuf; }
                std::snprintf(buf, sizeof(buf), u8"[E %s] %s", tn, e.label);
            } else {
                const auto& c  = world.cells[h.index];
                const char* tn = GetCellTypeName(c.type);
                char        tbuf[16];
                if (!tn) { std::snprintf(tbuf, sizeof(tbuf), "%u", static_cast<unsigned>(c.type)); tn = tbuf; }
                std::snprintf(buf, sizeof(buf), u8"[C %s] %s", tn, c.label);
            }
            ImGui::PushID(i);
            if (ImGui::Selectable(buf)) {
                if (h.kind == 0) {
                    const auto& e = world.entities[h.index];
                    SelectClear(world);
                    SelectAdd(world, e.id);
                    view->cam_x         = e.fx;
                    view->cam_y         = e.fy;
                    view->follow_player = true;
                } else {
                    const auto& c        = world.cells[h.index];
                    world.sel_cell_valid = true;
                    world.sel_cell_x     = c.x;
                    world.sel_cell_y     = c.y;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        if (s_popup_nskipped > 0)
            ImGui::TextDisabled(u8"(+%d more)", s_popup_nskipped);
        ImGui::EndPopup();
    }

    // --- Right-click context menu (released without dragging > 6px) ---
    struct CtxHit { int kind; int index; };
    static CtxHit s_ctx_hits[32];
    static int    s_ctx_nhits = 0;
    static float  s_ctx_wx = 0.f, s_ctx_wy = 0.f;

    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        if (drag.x * drag.x + drag.y * drag.y < 36.f) {
            int cwx, cwy;
            ToWorld(ImGui::GetIO().MousePos, cwx, cwy);
            s_ctx_wx = static_cast<float>(cwx);
            s_ctx_wy = static_cast<float>(cwy);
            s_ctx_nhits = 0;
            bool has_any = HasBgContextMenu_();
            for (int i = 0; i < static_cast<int>(world.entities.size()); ++i) {
                const auto& e = world.entities[i];
                if (e.map_id != world.active_map_id || e.x != cwx || e.y != cwy) continue;
                has_any = true;
                if (HasEntityContextMenu_(e.type) && s_ctx_nhits < 32)
                    s_ctx_hits[s_ctx_nhits++] = { 0, i };
            }
            for (int i = 0; i < static_cast<int>(world.cells.size()); ++i) {
                const auto& c = world.cells[i];
                if (c.map_id != world.active_map_id || c.x != cwx || c.y != cwy) continue;
                has_any = true;
                if (HasCellContextMenu_(c.type) && s_ctx_nhits < 32)
                    s_ctx_hits[s_ctx_nhits++] = { 1, i };
            }
            if (has_any) ImGui::OpenPopup("##canvas_ctx");
        }
    }
    if (ImGui::BeginPopup("##canvas_ctx")) {
        CanvasContextCtx cctx{ &world, ImGui::GetMousePos(), s_ctx_wx, s_ctx_wy, world.active_map_id };
        bool first = true;
        for (int i = 0; i < s_ctx_nhits; ++i) {
            const CtxHit& h = s_ctx_hits[i];
            if (h.kind == 0) {
                auto& e = world.entities[h.index];
                if (!first) ImGui::Separator();
                const char* tn = GetEntityTypeName(e.type); char fb[16];
                if (!tn) { std::snprintf(fb, sizeof(fb), "Type %u", static_cast<unsigned>(e.type)); tn = fb; }
                ImGui::TextDisabled("[E %s] %s", tn, e.label);
                InvokeEntityContextMenu_(e, cctx);
                first = false;
            } else {
                auto& c = world.cells[h.index];
                if (!first) ImGui::Separator();
                const char* tn = GetCellTypeName(c.type); char fb[16];
                if (!tn) { std::snprintf(fb, sizeof(fb), "Type %u", static_cast<unsigned>(c.type)); tn = fb; }
                ImGui::TextDisabled("[C %s] %s", tn, c.label);
                InvokeCellContextMenu_(c, cctx);
                first = false;
            }
        }
        if (HasBgContextMenu_()) {
            if (!first) ImGui::Separator();
            InvokeBgContextMenu_(cctx);
        }
        ImGui::EndPopup();
    }

    // --- Hover tooltip (reuses pre-scanned hover lists) ---
    if (hovered && (!hover_entity_idx.empty() || !hover_cell_idx.empty())) {
        ImGui::BeginTooltip();
        ImGui::Text("(%d, %d)", hover_wx, hover_wy);
        ImGui::Separator();
        for (int idx : hover_entity_idx) {
            const auto& e  = world.entities[idx];
            const char* tn = GetEntityTypeName(e.type);
            char        tbuf[16];
            if (!tn) { std::snprintf(tbuf, sizeof(tbuf), "%u", static_cast<unsigned>(e.type)); tn = tbuf; }
            ImGui::Text(u8"[E %s] %s", tn, e.label);
        }
        for (int idx : hover_cell_idx) {
            const auto& c  = world.cells[idx];
            const char* tn = GetCellTypeName(c.type);
            char        tbuf[16];
            if (!tn) { std::snprintf(tbuf, sizeof(tbuf), "%u", static_cast<unsigned>(c.type)); tn = tbuf; }
            ImGui::Text(u8"[C %s] %s", tn, c.label);
        }
        ImGui::EndTooltip();
    }

    ImGui::End();
}

} // namespace dui
