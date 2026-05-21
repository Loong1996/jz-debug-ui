#include "dui_draw_all.h"
#include "dui_ext.h"
#include "dui_inspector.h"
#include "dui_canvas.h"
#include "dui_metrics.h"
#include "dui_log.h"
#include "dui_commands.h"
#include "dui_detail.h"
#include "dui_events.h"
#include "dui_search.h"
#include "dui_profiler.h"
#include "dui_menubar.h"
#include "dui_replay.h"
#include "dui_minimap.h"
#include "dui_layers.h"

namespace dui {
namespace {

void DrawAllImpl(World& world, const Metrics* metrics) {
    CaptureReplayFrame_(world);
    DrawMenuBar();
    DrawInspector(world);
    DrawCanvas(world);
    if (metrics) DrawMetrics(*metrics);
    else         DrawMetrics();
    DrawLog();
    DrawWatch();
    DrawCommands();
    DrawEntityDetail(world);
    DrawEvents();
    InvokeUserPanels_();
    DrawGlobalSearch(world);
    DrawProfiler();
    DrawReplayPanel(world);
    DrawMinimap(world);
    DrawLayerPanel(world);
}

} // namespace

void DrawAll(World& world)                   { DrawAllImpl(world, nullptr); }
void DrawAll(World& world, Metrics& metrics) { DrawAllImpl(world, &metrics); }

} // namespace dui
