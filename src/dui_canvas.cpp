#include "dui_canvas.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace dui {

static const int GRID_SIZE = 37;
static const int VIEW_HALF = GRID_SIZE / 2; // 18

void DrawCanvas(World& world, CanvasView* view) {
    static CanvasView s_default_view;
    if (!view) view = &s_default_view;

    ImGui::Begin(u8"场景视图");

    // --- Find player ---
    const Entity* player = nullptr;
    for (const auto& e : world.entities)
        if (static_cast<int>(e.id) == world.player_id) { player = &e; break; }

    // --- Toolbar ---
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    ImGui::Checkbox(u8"格线",   &view->show_grid);   ImGui::SameLine();
    ImGui::Checkbox(u8"格子",   &view->show_cells);  ImGui::SameLine();
    ImGui::Checkbox(u8"实体",   &view->show_ents);   ImGui::SameLine();
    ImGui::Checkbox(u8"标签",   &view->show_labels); ImGui::SameLine();
    ImGui::Checkbox(u8"坐标轴", &view->show_axis);   ImGui::SameLine();
    ImGui::TextDisabled("|"); ImGui::SameLine();
    if (ImGui::RadioButton(u8"跟随",  view->follow_player))  view->follow_player = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(u8"自由", !view->follow_player))  view->follow_player = false;
    ImGui::SameLine();
    if (ImGui::Button(u8"重置") && player) {
        view->cam_x = player->x;
        view->cam_y = player->y;
    }
    ImGui::PopStyleVar();

    // --- Follow mode: track selected entity, fall back to player ---
    if (view->follow_player) {
        const Entity* target = nullptr;
        if (world.selected_id != -1) {
            for (const auto& e : world.entities)
                if (static_cast<int>(e.id) == world.selected_id) { target = &e; break; }
        }
        if (!target) target = player;
        if (target) { view->cam_x = target->x; view->cam_y = target->y; }
    }

    // --- Canvas area ---
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 50.f) sz.x = 50.f;
    if (sz.y < 50.f) sz.y = 50.f;

    ImGui::InvisibleButton("canvas_area", sz,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();

    const ImVec2 center(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
    const float  th = (sz.x < sz.y ? sz.x : sz.y) * 0.5f / (2.f * VIEW_HALF + 3.f);

    // --- Right-drag pan in free mode ---
    // sub-cell accumulator lives here; single canvas, statics are fine
    static float s_rem_x = 0.f, s_rem_y = 0.f;
    if (!view->follow_player && active && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        ImVec2 md = ImGui::GetIO().MouseDelta;
        s_rem_x -= (md.x + md.y) / (2.f * th);
        s_rem_y -= (md.y - md.x) / (2.f * th);
        int ix = static_cast<int>(s_rem_x); view->cam_x += ix; s_rem_x -= static_cast<float>(ix);
        int iy = static_cast<int>(s_rem_y); view->cam_y += iy; s_rem_y -= static_cast<float>(iy);
    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        s_rem_x = s_rem_y = 0.f;
    }

    const int ccx = view->cam_x;
    const int ccy = view->cam_y;

    auto ToScreen = [&](float wx, float wy) -> ImVec2 {
        float dx = wx - static_cast<float>(ccx);
        float dy = wy - static_cast<float>(ccy);
        return ImVec2(center.x + (dx - dy) * th,
                      center.y + (dx + dy) * th);
    };

    auto ToWorld = [&](ImVec2 s, int& ox, int& oy) {
        float rx = s.x - center.x, ry = s.y - center.y;
        ox = ccx + static_cast<int>(roundf((rx + ry) / (2.f * th)));
        oy = ccy + static_cast<int>(roundf((ry - rx) / (2.f * th)));
    };

    auto TileDiamond = [&](int wx, int wy, ImVec2 pts[4]) {
        float dx = static_cast<float>(wx - ccx);
        float dy = static_cast<float>(wy - ccy);
        float cx = center.x + (dx - dy) * th;
        float cy = center.y + (dx + dy) * th;
        pts[0] = ImVec2(cx,      cy - th);
        pts[1] = ImVec2(cx + th, cy);
        pts[2] = ImVec2(cx,      cy + th);
        pts[3] = ImVec2(cx - th, cy);
    };

    auto InView = [&](int wx, int wy) {
        return abs(wx - ccx) <= VIEW_HALF && abs(wy - ccy) <= VIEW_HALF;
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 32, 255));
    dl->PushClipRect(p0, p1, true);

    // --- 1. Map cells (render all, not just in-view) ---
    if (view->show_cells) {
        for (const auto& c : world.cells) {
            ImVec2 pts[4];
            TileDiamond(c.x, c.y, pts);
            dl->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], c.color);
        }
    }

    // --- 2. Grid lines (full canvas extent) ---
    if (view->show_grid) {
        // How many grid cells fit across the canvas in each isometric axis direction.
        // A column at world x = wx is visible when |(wx-ccx)*th| < (sz.x+sz.y)/2.
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

    // --- 2b. View boundary indicator (centered on tracked entity, not camera) ---
    {
        // Same priority as follow mode: selected entity > player
        int bcx = player ? player->x : ccx;
        int bcy = player ? player->y : ccy;
        if (world.selected_id != -1) {
            for (const auto& e : world.entities)
                if (static_cast<int>(e.id) == world.selected_id) { bcx = e.x; bcy = e.y; break; }
        }
        float hf = static_cast<float>(VIEW_HALF) + 0.5f;
        float fx = static_cast<float>(bcx), fy = static_cast<float>(bcy);
        ImVec2 bnd[4] = {
            ToScreen(fx - hf, fy - hf),  // top
            ToScreen(fx + hf, fy - hf),  // right
            ToScreen(fx + hf, fy + hf),  // bottom
            ToScreen(fx - hf, fy + hf),  // left
        };
        dl->AddQuad(bnd[0], bnd[1], bnd[2], bnd[3], IM_COL32(255, 140, 0, 220), 2.f);
    }

    // --- 3. Center cell highlight ---
    {
        ImVec2 pts[4];
        TileDiamond(ccx, ccy, pts);
        dl->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], IM_COL32(50, 50, 75, 255));
    }

    // --- 4. Entities ---
    if (view->show_ents) {
        for (const auto& e : world.entities) {
            ImVec2 pts[4];
            TileDiamond(e.x, e.y, pts);

            float r = e.radius < 1.f ? e.radius : 1.f;
            ImVec2 cpt((pts[0].x + pts[2].x) * 0.5f, (pts[0].y + pts[2].y) * 0.5f);
            ImVec2 scaled[4];
            for (int k = 0; k < 4; ++k)
                scaled[k] = ImVec2(cpt.x + (pts[k].x - cpt.x) * r,
                                   cpt.y + (pts[k].y - cpt.y) * r);

            dl->AddQuadFilled(scaled[0], scaled[1], scaled[2], scaled[3], e.color);

            if (static_cast<int>(e.id) == world.selected_id)
                dl->AddQuad(pts[0], pts[1], pts[2], pts[3],
                            IM_COL32(255, 230, 0, 255), 2.f);

            if (view->show_labels && th >= 10.f) {
                float lw = ImGui::CalcTextSize(e.label).x;
                dl->AddText(ImVec2(pts[0].x - lw * 0.5f,
                                   pts[0].y - ImGui::GetFontSize() - 2.f),
                            IM_COL32(210, 210, 210, 220), e.label);
            }
        }
    }

    // --- 5. Selected cell gold outline ---
    if (world.sel_cell_valid && InView(world.sel_cell_x, world.sel_cell_y)) {
        ImVec2 pts[4];
        TileDiamond(world.sel_cell_x, world.sel_cell_y, pts);
        dl->AddQuad(pts[0], pts[1], pts[2], pts[3], IM_COL32(255, 230, 0, 255), 2.f);
    }

    // --- 6. Axis indicator (outside clip rect) ---
    dl->PopClipRect();
    if (view->show_axis) {
        const float ox = p0.x + 36.f, oy = p0.y + sz.y - 18.f;
        const float ar = 26.f, k = 0.7071f;
        dl->AddLine(ImVec2(ox, oy), ImVec2(ox + ar*k, oy + ar*k),
                    IM_COL32(240, 100, 100, 200), 1.5f);
        dl->AddText(ImVec2(ox + ar*k + 2.f, oy + ar*k - 7.f),
                    IM_COL32(240, 100, 100, 200), "+X");
        dl->AddLine(ImVec2(ox, oy), ImVec2(ox - ar*k, oy + ar*k),
                    IM_COL32(100, 210, 100, 200), 1.5f);
        dl->AddText(ImVec2(ox - ar*k - 22.f, oy + ar*k - 7.f),
                    IM_COL32(100, 210, 100, 200), "+Y");
    }

    // --- Click selection: entity > cell > clear ---
    // s_lmb_pan: left drag started on empty space → pan camera
    static bool s_lmb_pan = false;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int wx, wy;
        ToWorld(ImGui::GetIO().MousePos, wx, wy);
        world.selected_id    = -1;
        world.sel_cell_valid = false;
        s_lmb_pan = true;
        bool hit = false;
        for (const auto& e : world.entities) {
            if (e.x == wx && e.y == wy) {
                world.selected_id   = static_cast<int>(e.id);
                view->cam_x         = e.x;
                view->cam_y         = e.y;
                view->follow_player = true;
                s_lmb_pan = false;
                hit = true;
                break;
            }
        }
        if (!hit) {
            for (const auto& c : world.cells) {
                if (c.x == wx && c.y == wy) {
                    world.sel_cell_valid = true;
                    world.sel_cell_x     = wx;
                    world.sel_cell_y     = wy;
                    break;
                }
            }
        }
    }

    // Left-drag on background pans camera (same math as right-drag, separate accumulator)
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
        s_lmb_pan  = false;
        s_lrem_x = s_lrem_y = 0.f;
    }

    // --- Hover tooltip: entity takes priority over cell ---
    if (hovered) {
        int wx, wy;
        ToWorld(ImGui::GetIO().MousePos, wx, wy);
        if (InView(wx, wy)) {
            bool shown = false;
            for (const auto& e : world.entities) {
                if (e.x == wx && e.y == wy) {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf),
                        u8"[type %d] %s  (%d, %d)", e.type, e.label, e.x, e.y);
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(buf);
                    ImGui::EndTooltip();
                    shown = true;
                    break;
                }
            }
            if (!shown) {
                for (const auto& c : world.cells) {
                    if (c.x == wx && c.y == wy) {
                        char buf[64];
                        std::snprintf(buf, sizeof(buf),
                            u8"[type %d] %s  (%d, %d)", c.type, c.label, c.x, c.y);
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(buf);
                        ImGui::EndTooltip();
                        break;
                    }
                }
            }
        }
    }

    ImGui::End();
}

} // namespace dui
