#include "dui_draw_all.h"
#include "dui_inspector.h"
#include "dui_canvas.h"
#include "dui_metrics.h"
#include "dui_log.h"
#include "dui_commands.h"
#include "dui_detail.h"
#include "dui_events.h"
#include "dui_search.h"

namespace dui {

void DrawAll(World& world, Metrics& metrics) {
    DrawInspector(world);
    DrawCanvas(world);
    DrawMetrics(metrics);
    DrawLog();
    DrawWatch();
    DrawCommands();
    DrawEntityDetail(world);
    DrawEvents();
    DrawGlobalSearch(world);
}

} // namespace dui
