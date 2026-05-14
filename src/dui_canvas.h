#pragma once
#include "dui_world.h"

namespace dui {

struct CanvasView {
    bool show_grid     = true;
    bool show_cells    = true;
    bool show_ents     = true;
    bool show_labels   = true;
    bool show_axis     = true;
    bool follow_player = true;
    float cam_x = 0.f;
    float cam_y = 0.f;
    float zoom  = 1.f;  // [kZoomMin, kZoomMax]
};

// view == nullptr uses an internal static default instance.
void DrawCanvas(World& world, CanvasView* view = nullptr);

} // namespace dui
