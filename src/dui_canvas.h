#pragma once
#include "dui_world.h"
#include <string>
#include <vector>

namespace dui {

struct CanvasView {
    bool show_grid     = true;
    bool show_cells    = true;
    bool show_ents     = true;
    bool show_labels   = true;
    bool show_axis     = true;
    bool follow_player = true;
    bool show_trails   = false;
    bool show_links    = true;
    bool show_heatmaps = false;
    float cam_x = 0.f;
    float cam_y = 0.f;
    float zoom  = 1.f;  // [kZoomMin, kZoomMax]
};

// view == nullptr uses an internal static default instance.
void DrawCanvas(World& world, CanvasView* view = nullptr);

// Returns the internal default CanvasView used when DrawCanvas is called with view=nullptr.
CanvasView& GetActiveCanvasView();

// ---- Camera bookmarks ----
// Save and restore named (map_id, cam_x, cam_y, zoom) snapshots.
// Bookmarks persist to dui_bookmarks.ini across sessions.
struct CameraBookmark {
    std::string name;
    uint32_t    map_id;
    float       cam_x, cam_y, zoom;
};

void SaveCameraBookmark  (const char* name, World& world);          // captures current view + map
bool GotoCameraBookmark  (const char* name, World& world);          // returns false if not found
void DeleteCameraBookmark(const char* name);
const std::vector<CameraBookmark>& ListCameraBookmarks();

// Internal — called by DrawAll / App init.
void LoadCameraBookmarks_();
void SaveCameraBookmarks_();

} // namespace dui
