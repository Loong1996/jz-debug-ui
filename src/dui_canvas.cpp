#include "dui_canvas.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace dui {

static const int GRID_SIZE = 37;
static const int VIEW_HALF = GRID_SIZE / 2; // 18

void DrawCanvas(World& world) {
    ImGui::Begin(u8"场景视图");

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 50.f) sz.x = 50.f;
    if (sz.y < 50.f) sz.y = 50.f;

    ImGui::InvisibleButton("canvas_area", sz, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();

    // Find player
    const Entity* player = nullptr;
    for (const auto& e : world.entities)
        if (static_cast<int>(e.id) == world.player_id) { player = &e; break; }
    const int pcx = player ? player->x : 0;
    const int pcy = player ? player->y : 0;

    // Isometric projection:
    //   +X world → right-down (+1, +1) on screen
    //   +Y world → left-down  (-1, +1) on screen
    // tile_half (th) = half the screen-space diagonal of one cell.
    // Visible diamond spans ±VIEW_HALF cells; extreme screen reach = VIEW_HALF*th from center.
    const ImVec2 center(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
    const float th = (sz.x < sz.y ? sz.x : sz.y) * 0.5f / (VIEW_HALF + 1.5f);

    // World (float) → screen absolute position
    auto ToScreen = [&](float wx, float wy) -> ImVec2 {
        float dx = static_cast<float>(wx - pcx), dy = static_cast<float>(wy - pcy);
        return ImVec2(center.x + (dx - dy) * th,
                      center.y + (dx + dy) * th);
    };

    // Inverse: screen pixel → nearest world grid cell
    auto ToWorld = [&](ImVec2 s, int& ox, int& oy) {
        float rx = s.x - center.x, ry = s.y - center.y;
        ox = static_cast<int>(pcx) + static_cast<int>(roundf((rx + ry) / (2.f * th)));
        oy = static_cast<int>(pcy) + static_cast<int>(roundf((ry - rx) / (2.f * th)));
    };

    // Diamond vertices of the tile whose world-center is at integer (wx, wy)
    // (tile occupies world region [wx-0.5, wx+0.5] x [wy-0.5, wy+0.5])
    auto TileDiamond = [&](int wx, int wy, ImVec2 pts[4]) {
        float dx = static_cast<float>(wx - pcx), dy = static_cast<float>(wy - pcy);
        float cx = center.x + (dx - dy) * th;
        float cy = center.y + (dx + dy) * th;
        pts[0] = ImVec2(cx,      cy - th); // top
        pts[1] = ImVec2(cx + th, cy);      // right (+X)
        pts[2] = ImVec2(cx,      cy + th); // bottom
        pts[3] = ImVec2(cx - th, cy);      // left  (+Y)
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 32, 255));
    dl->PushClipRect(p0, p1, true);

    // --- Grid lines (38 in each isometric direction) ---
    // Lines of constant wx (run parallel to +Y axis → left-down on screen)
    for (int i = -VIEW_HALF; i <= VIEW_HALF + 1; ++i) {
        float wx = pcx + i - 0.5f;
        ImVec2 a = ToScreen(wx, pcy - VIEW_HALF - 0.5f);
        ImVec2 b = ToScreen(wx, pcy + VIEW_HALF + 0.5f);
        dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
    }
    // Lines of constant wy (run parallel to +X axis → right-down on screen)
    for (int j = -VIEW_HALF; j <= VIEW_HALF + 1; ++j) {
        float wy = pcy + j - 0.5f;
        ImVec2 a = ToScreen(pcx - VIEW_HALF - 0.5f, wy);
        ImVec2 b = ToScreen(pcx + VIEW_HALF + 0.5f, wy);
        dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
    }

    // --- Center cell highlight (player position) ---
    {
        ImVec2 pts[4];
        TileDiamond(pcx, pcy, pts);
        dl->AddQuadFilled(pts[0], pts[1], pts[2], pts[3], IM_COL32(50, 50, 75, 255));
    }

    // --- Entities ---
    for (const auto& e : world.entities) {
        int dx = e.x - pcx, dy = e.y - pcy;
        if (dx < -VIEW_HALF || dx > VIEW_HALF ||
            dy < -VIEW_HALF || dy > VIEW_HALF) continue;

        ImVec2 pts[4];
        TileDiamond(e.x, e.y, pts);

        // Scale visual diamond by entity radius (fill ratio)
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

        // Label above the tile (only when tiles are large enough to read)
        if (th >= 10.f) {
            float lw = ImGui::CalcTextSize(e.label).x;
            dl->AddText(ImVec2(pts[0].x - lw * 0.5f,
                               pts[0].y - ImGui::GetFontSize() - 2.f),
                        IM_COL32(210, 210, 210, 220), e.label);
        }
    }

    // --- Axis indicator (bottom-left corner, outside clip) ---
    dl->PopClipRect();
    {
        const float ox = p0.x + 36.f, oy = p0.y + sz.y - 18.f;
        const float ar = 26.f, k = 0.7071f;
        // +X: right-down
        dl->AddLine(ImVec2(ox, oy), ImVec2(ox + ar * k, oy + ar * k),
                    IM_COL32(240, 100, 100, 200), 1.5f);
        dl->AddText(ImVec2(ox + ar * k + 2.f, oy + ar * k - 7.f),
                    IM_COL32(240, 100, 100, 200), "+X");
        // +Y: left-down
        dl->AddLine(ImVec2(ox, oy), ImVec2(ox - ar * k, oy + ar * k),
                    IM_COL32(100, 210, 100, 200), 1.5f);
        dl->AddText(ImVec2(ox - ar * k - 22.f, oy + ar * k - 7.f),
                    IM_COL32(100, 210, 100, 200), "+Y");
    }

    // --- Click selection (inverse isometric transform) ---
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int wx, wy;
        ToWorld(ImGui::GetIO().MousePos, wx, wy);
        world.selected_id = -1;
        for (const auto& e : world.entities) {
            if (e.x == wx && e.y == wy) {
                world.selected_id = static_cast<int>(e.id);
                break;
            }
        }
    }

    // --- Hover tooltip ---
    if (hovered) {
        int wx, wy;
        ToWorld(ImGui::GetIO().MousePos, wx, wy);
        if (abs(wx - pcx) <= VIEW_HALF && abs(wy - pcy) <= VIEW_HALF) {
            for (const auto& e : world.entities) {
                if (e.x == wx && e.y == wy) {
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%s  (%d, %d)", e.label, e.x, e.y);
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(buf);
                    ImGui::EndTooltip();
                    break;
                }
            }
        }
    }

    ImGui::End();
}

} // namespace dui
