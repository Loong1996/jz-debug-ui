#include "dui_metrics.h"
#include <imgui.h>
#include <implot.h>

namespace dui {

void DrawMetrics(const Metrics& m) {
    ImGui::Begin("Metrics");

    if (ImPlot::BeginPlot("Tick Time (ms)", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("frame", "ms");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 5.0, ImPlotCond_Always);
        ImPlot::PlotLine("tick_ms",
            m.tick_ms.data(),
            m.tick_ms.plot_count(),
            1.0, 0.0, 0,
            m.tick_ms.plot_offset());
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("Entity Count", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("frame", "count");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 20.0, ImPlotCond_Always);
        ImPlot::PlotLine("entities",
            m.entity_count.data(),
            m.entity_count.plot_count(),
            1.0, 0.0, 0,
            m.entity_count.plot_offset());
        ImPlot::EndPlot();
    }

    ImGui::End();
}

} // namespace dui
