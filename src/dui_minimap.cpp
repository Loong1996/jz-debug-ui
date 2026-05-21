#include "dui_minimap.h"
#include "dui_ext.h"
#include "dui_canvas.h"
#include "dui_menubar.h"
#include "dui_replay.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace dui {

// kViewHalf matches the canvas constant so the viewport rect is accurate.
static constexpr float kCanvasViewHalf = 18.f;

void DrawMinimap(World& world) {
    if (!ImGui::Begin(u8"小地图", &BuiltinPanelOpenRef(u8"小地图")))
        { ImGui::End(); return; }

    const World& rw = SelectActiveWorld_(world);

    // Collect all entities and cells on the active map to compute bounds.
    float bx0 = 1e9f, bx1 = -1e9f, by0 = 1e9f, by1 = -1e9f;
    bool any = false;

    for (const auto& e : rw.entities) {
        if (e.map_id != rw.active_map_id) continue;
        bx0 = fminf(bx0, e.fx); bx1 = fmaxf(bx1, e.fx);
        by0 = fminf(by0, e.fy); by1 = fmaxf(by1, e.fy);
        any = true;
    }
    for (const auto& c : rw.cells) {
        if (c.map_id != rw.active_map_id) continue;
        float cx = static_cast<float>(c.x), cy = static_cast<float>(c.y);
        bx0 = fminf(bx0, cx); bx1 = fmaxf(bx1, cx);
        by0 = fminf(by0, cy); by1 = fmaxf(by1, cy);
        any = true;
    }

    if (!any) {
        // Nothing on map — use a sensible default so the panel isn't blank.
        bx0 = -10.f; bx1 = 10.f; by0 = -10.f; by1 = 10.f;
    }

    // Pad bounds.
    const float pad = 3.f;
    bx0 -= pad; bx1 += pad; by0 -= pad; by1 += pad;
    const float bw = bx1 - bx0, bh = by1 - by0;

    // Draw area — use square region that fits in the window content area.
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 20.f) avail.x = 20.f;
    if (avail.y < 20.f) avail.y = 20.f;
    float side = avail.x < avail.y ? avail.x : avail.y;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    // Center the square if the window is wider/taller.
    if (avail.x > side) p0.x += (avail.x - side) * 0.5f;
    if (avail.y > side) p0.y += (avail.y - side) * 0.5f;
    ImVec2 p1(p0.x + side, p0.y + side);

    // World → minimap pixel helpers.
    auto toMapX = [&](float wx) { return p0.x + (wx - bx0) / bw * side; };
    auto toMapY = [&](float wy) { return p0.y + (wy - by0) / bh * side; };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background.
    dl->AddRectFilled(p0, p1, IM_COL32(25, 25, 30, 255));
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255));

    // Cells — tiny colored squares.
    if (bw > 0.f && bh > 0.f) {
        float cell_px = side / bw;  // pixels per world unit
        float hs = fmaxf(1.5f, cell_px * 0.45f);
        for (const auto& c : rw.cells) {
            if (c.map_id != rw.active_map_id) continue;
            float sx = toMapX(static_cast<float>(c.x));
            float sy = toMapY(static_cast<float>(c.y));
            ImU32 col = c.color | IM_COL32(0, 0, 0, 200); // ensure opaque-ish
            dl->AddRectFilled(ImVec2(sx - hs, sy - hs), ImVec2(sx + hs, sy + hs), col);
        }
    }

    // Entities — colored dots; player type gets a bright yellow dot.
    for (const auto& e : rw.entities) {
        if (e.map_id != rw.active_map_id) continue;
        float sx = toMapX(e.fx);
        float sy = toMapY(e.fy);
        float r = 3.f;
        ImU32 col;
        if (IsPlayerEntityType(e.type) || e.id == rw.follower_id) {
            col = IM_COL32(255, 230, 50, 255);
            r   = 4.5f;
        } else {
            // Use entity color, override alpha.
            ImVec4 fc = ImGui::ColorConvertU32ToFloat4(e.color);
            fc.w = 1.f;
            col  = ImGui::ColorConvertFloat4ToU32(fc);
        }
        dl->AddCircleFilled(ImVec2(sx, sy), r, col);
    }

    // Viewport rectangle — centered at cam_x/cam_y with extent ≈ kViewHalf/zoom.
    {
        const CanvasView& v = GetActiveCanvasView();
        float half = kCanvasViewHalf / v.zoom;
        float vx0 = toMapX(v.cam_x - half), vy0 = toMapY(v.cam_y - half);
        float vx1 = toMapX(v.cam_x + half), vy1 = toMapY(v.cam_y + half);
        // Clamp to minimap bounds.
        vx0 = fmaxf(vx0, p0.x); vy0 = fmaxf(vy0, p0.y);
        vx1 = fminf(vx1, p1.x); vy1 = fminf(vy1, p1.y);
        if (vx1 > vx0 && vy1 > vy0)
            dl->AddRect(ImVec2(vx0, vy0), ImVec2(vx1, vy1), IM_COL32(255, 255, 255, 180), 0.f, 0, 1.5f);
    }

    // Invisible button to capture click interaction.
    ImGui::SetCursorScreenPos(p0);
    ImGui::InvisibleButton("##minimap_area", ImVec2(side, side));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        float wx = bx0 + (mp.x - p0.x) / side * bw;
        float wy = by0 + (mp.y - p0.y) / side * bh;
        CanvasView& v = GetActiveCanvasView();
        v.cam_x        = wx;
        v.cam_y        = wy;
        v.follow_player = false;
    }

    // Tooltip with cursor world position.
    if (ImGui::IsItemHovered()) {
        ImVec2 mp = ImGui::GetIO().MousePos;
        float wx = bx0 + (mp.x - p0.x) / side * bw;
        float wy = by0 + (mp.y - p0.y) / side * bh;
        ImGui::SetTooltip("(%.1f, %.1f)", wx, wy);
    }

    ImGui::End();
}

} // namespace dui
