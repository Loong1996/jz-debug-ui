#pragma once
#include <imgui.h>
#include "dui_world.h"

namespace dui {

struct Camera {
    float offset_x = 0.f;
    float offset_y = 0.f;
    float zoom = 20.f;  // pixels per world unit
};

ImVec2 WorldToScreen(float wx, float wy, const Camera& cam,
                     ImVec2 origin, ImVec2 size);

void DrawCanvas(World& world, Camera& cam);

} // namespace dui
