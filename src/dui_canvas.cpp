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

    // Cell size: largest square that fits, showing GRID_SIZE cells
    const float cell = (sz.x < sz.y ? sz.x : sz.y) / static_cast<float>(GRID_SIZE);
    const float grid_px = cell * GRID_SIZE;
    const ImVec2 go(p0.x + (sz.x - grid_px) * 0.5f,
                    p0.y + (sz.y - grid_px) * 0.5f);

    // World coord → top-left corner of that grid cell in screen space
    auto CellTL = [&](int wx, int wy) -> ImVec2 {
        int col = wx - pcx + VIEW_HALF;
        int row = VIEW_HALF - (wy - pcy);   // Y-axis points up
        return ImVec2(go.x + col * cell, go.y + row * cell);
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 32, 255));
    dl->PushClipRect(p0, p1, true);

    // Grid lines
    for (int i = 0; i <= GRID_SIZE; ++i) {
        float x = go.x + i * cell;
        float y = go.y + i * cell;
        dl->AddLine(ImVec2(x, go.y), ImVec2(x, go.y + grid_px), IM_COL32(55, 55, 65, 255));
        dl->AddLine(ImVec2(go.x, y), ImVec2(go.x + grid_px, y), IM_COL32(55, 55, 65, 255));
    }

    // Highlight center cell (player's cell)
    const ImVec2 ctl(go.x + VIEW_HALF * cell, go.y + VIEW_HALF * cell);
    dl->AddRectFilled(ctl, ImVec2(ctl.x + cell, ctl.y + cell), IM_COL32(50, 50, 75, 255));

    // Entities
    for (const auto& e : world.entities) {
        int dx = e.x - pcx, dy = e.y - pcy;
        if (dx < -VIEW_HALF || dx > VIEW_HALF ||
            dy < -VIEW_HALF || dy > VIEW_HALF) continue;

        ImVec2 tl = CellTL(e.x, e.y);
        float pad = cell * (1.f - e.radius) * 0.5f;
        if (pad < 0.f) pad = 0.f;
        dl->AddRectFilled(ImVec2(tl.x + pad, tl.y + pad),
                          ImVec2(tl.x + cell - pad, tl.y + cell - pad),
                          e.color);

        if (static_cast<int>(e.id) == world.selected_id)
            dl->AddRect(tl, ImVec2(tl.x + cell, tl.y + cell),
                        IM_COL32(255, 230, 0, 255), 0.f, 0, 2.f);

        if (cell >= 18.f)
            dl->AddText(ImVec2(tl.x + 2.f, tl.y + 2.f),
                        IM_COL32(220, 220, 220, 255), e.label);
    }

    dl->PopClipRect();

    // Select entity: left-click
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        world.selected_id = -1;
        for (const auto& e : world.entities) {
            int dx = e.x - pcx, dy = e.y - pcy;
            if (dx < -VIEW_HALF || dx > VIEW_HALF ||
                dy < -VIEW_HALF || dy > VIEW_HALF) continue;
            ImVec2 tl = CellTL(e.x, e.y);
            if (mouse.x >= tl.x && mouse.x < tl.x + cell &&
                mouse.y >= tl.y && mouse.y < tl.y + cell) {
                world.selected_id = static_cast<int>(e.id);
                break;
            }
        }
    }

    // Hover tooltip
    if (hovered) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        for (const auto& e : world.entities) {
            int dx = e.x - pcx, dy = e.y - pcy;
            if (dx < -VIEW_HALF || dx > VIEW_HALF ||
                dy < -VIEW_HALF || dy > VIEW_HALF) continue;
            ImVec2 tl = CellTL(e.x, e.y);
            if (mouse.x >= tl.x && mouse.x < tl.x + cell &&
                mouse.y >= tl.y && mouse.y < tl.y + cell) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%s  (%d, %d)", e.label, e.x, e.y);
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(buf);
                ImGui::EndTooltip();
                break;
            }
        }
    }

    ImGui::End();
}

} // namespace dui
