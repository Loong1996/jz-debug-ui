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
    int  cam_x = 0;
    int  cam_y = 0;
};

// view == nullptr uses an internal static default instance.
void DrawCanvas(World& world, CanvasView* view = nullptr);

} // namespace dui
