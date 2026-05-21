#include "dui_metrics.h"
#include "dui_user_metrics.h"
#include "dui_menubar.h"
#include <imgui.h>
#include <implot.h>

namespace dui {
namespace {

void DrawMetricsImpl(const Metrics* m) {
    if (ImGui::Begin(u8"性能指标", &BuiltinPanelOpenRef(u8"性能指标"))) {
        if (m) {
            if (ImPlot::BeginPlot(u8"帧耗时 (ms)", ImVec2(-1, 150))) {
                ImPlot::SetupAxes(u8"帧", "ms");
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 5.0, ImPlotCond_Always);
                ImPlot::PlotLine("tick_ms",
                    m->tick_ms.data(),
                    m->tick_ms.plot_count(),
                    1.0, 0.0,
                    ImPlotSpec(ImPlotProp_Offset, m->tick_ms.plot_offset()));
                ImPlot::EndPlot();
            }

            if (ImPlot::BeginPlot(u8"实体数量", ImVec2(-1, 150))) {
                ImPlot::SetupAxes(u8"帧", u8"数量");
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 20.0, ImPlotCond_Always);
                ImPlot::PlotLine("entities",
                    m->entity_count.data(),
                    m->entity_count.plot_count(),
                    1.0, 0.0,
                    ImPlotSpec(ImPlotProp_Offset, m->entity_count.plot_offset()));
                ImPlot::EndPlot();
            }
        }
    }
    DrawUserMetrics();
    ImGui::End();
}

} // namespace

void DrawMetrics(const Metrics& m) { DrawMetricsImpl(&m); }
void DrawMetrics()                  { DrawMetricsImpl(nullptr); }

} // namespace dui
