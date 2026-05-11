#include "dui_canvas.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace dui {

ImVec2 WorldToScreen(float wx, float wy, const Camera& cam,
                     ImVec2 origin, ImVec2 size) {
    return ImVec2(
        origin.x + size.x * 0.5f + (wx - cam.offset_x) * cam.zoom,
        origin.y + size.y * 0.5f - (wy - cam.offset_y) * cam.zoom
    );
}

void DrawCanvas(World& world, Camera& cam) {
    ImGui::Begin("Scene");

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 50.f) sz.x = 50.f;
    if (sz.y < 50.f) sz.y = 50.f;
    ImVec2 p1 = ImVec2(p0.x + sz.x, p0.y + sz.y);

    ImGui::InvisibleButton("canvas_area", sz,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();

    // Pan: right-drag
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.f)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        cam.offset_x -= delta.x / cam.zoom;
        cam.offset_y += delta.y / cam.zoom;
    }

    // Zoom: scroll wheel
    if (hovered && ImGui::GetIO().MouseWheel != 0.f) {
        float factor = (ImGui::GetIO().MouseWheel > 0) ? 1.1f : 0.9f;
        cam.zoom *= factor;
        if (cam.zoom < 1.f)   cam.zoom = 1.f;
        if (cam.zoom > 500.f) cam.zoom = 500.f;
    }

    // Select entity: left-click (pixel threshold 20px)
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        world.selected_id = -1;
        float best = 20.f;
        for (const auto& e : world.entities) {
            ImVec2 sp = WorldToScreen(e.x, e.y, cam, p0, sz);
            float dx = mouse.x - sp.x, dy = mouse.y - sp.y;
            float d  = sqrtf(dx * dx + dy * dy);
            if (d < best) { best = d; world.selected_id = static_cast<int>(e.id); }
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 32, 255));
    dl->PushClipRect(p0, p1, true);

    // Grid
    float half_w = sz.x * 0.5f / cam.zoom;
    float half_h = sz.y * 0.5f / cam.zoom;
    float left   = cam.offset_x - half_w, right  = cam.offset_x + half_w;
    float bottom = cam.offset_y - half_h, top    = cam.offset_y + half_h;

    float grid_step = 1.f;
    if (cam.zoom < 5.f)  grid_step = 5.f;
    if (cam.zoom < 1.5f) grid_step = 10.f;

    float xs = floorf(left   / grid_step) * grid_step;
    float ys = floorf(bottom / grid_step) * grid_step;

    for (float wx = xs; wx <= right; wx += grid_step) {
        ImVec2 a = WorldToScreen(wx, top,    cam, p0, sz);
        ImVec2 b = WorldToScreen(wx, bottom, cam, p0, sz);
        dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
    }
    for (float wy = ys; wy <= top; wy += grid_step) {
        ImVec2 a = WorldToScreen(left,  wy, cam, p0, sz);
        ImVec2 b = WorldToScreen(right, wy, cam, p0, sz);
        dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
    }

    // Axes (slightly brighter)
    dl->AddLine(WorldToScreen(left,  0.f, cam, p0, sz),
                WorldToScreen(right, 0.f, cam, p0, sz),
                IM_COL32(80, 80, 100, 255));
    dl->AddLine(WorldToScreen(0.f, bottom, cam, p0, sz),
                WorldToScreen(0.f, top,    cam, p0, sz),
                IM_COL32(80, 80, 100, 255));

    // Entities
    for (const auto& e : world.entities) {
        ImVec2 sp = WorldToScreen(e.x, e.y, cam, p0, sz);
        float r   = e.radius * cam.zoom;
        if (r < 3.f) r = 3.f;
        dl->AddCircleFilled(sp, r, e.color);
        if (static_cast<int>(e.id) == world.selected_id)
            dl->AddCircle(sp, r + 4.f, IM_COL32(255, 230, 0, 255), 0, 2.5f);
        dl->AddText(ImVec2(sp.x + r + 3.f, sp.y - 7.f),
                    IM_COL32(220, 220, 220, 255), e.label);
    }

    dl->PopClipRect();

    // Hover tooltip
    if (hovered) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        for (const auto& e : world.entities) {
            ImVec2 sp = WorldToScreen(e.x, e.y, cam, p0, sz);
            float dx = mouse.x - sp.x, dy = mouse.y - sp.y;
            if (sqrtf(dx*dx + dy*dy) < e.radius * cam.zoom + 4.f) {
                char buf[64];
                std::snprintf(buf, sizeof(buf),
                    "%s  (%.2f, %.2f)", e.label, e.x, e.y);
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
