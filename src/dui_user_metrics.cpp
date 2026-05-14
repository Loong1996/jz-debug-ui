#include "dui_user_metrics.h"
#include "dui_metrics.h"
#include <imgui.h>
#include <implot.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace dui {
namespace {

struct UserMetric {
    RingBuffer<float, 300> buf;
    MetricOpts             opts;
};

static std::vector<std::string>               s_order;
static std::unordered_map<std::string, UserMetric> s_metrics;

struct PlotGroup {
    std::vector<std::string> names;
    bool        is_shared;
    const char* unit;
};

static std::vector<PlotGroup> build_groups() {
    std::vector<PlotGroup> groups;
    for (const auto& name : s_order) {
        auto it = s_metrics.find(name);
        if (it == s_metrics.end()) continue;
        const MetricOpts& opts = it->second.opts;

        bool merge = opts.share_axis
            && !groups.empty()
            && groups.back().is_shared
            && ((!groups.back().unit && !opts.unit)
                || (groups.back().unit && opts.unit
                    && std::string(groups.back().unit) == std::string(opts.unit)));

        if (merge) {
            groups.back().names.push_back(name);
        } else {
            PlotGroup g;
            g.names.push_back(name);
            g.is_shared = opts.share_axis;
            g.unit      = opts.unit;
            groups.push_back(std::move(g));
        }
    }
    return groups;
}


} // anonymous namespace

void PushMetric(const char* name, float value) {
    if (!name || !name[0]) return;
    auto it = s_metrics.find(name);
    if (it == s_metrics.end()) {
        s_order.push_back(name);
        it = s_metrics.emplace(name, UserMetric{}).first;
    }
    it->second.buf.push(value);
}

void ConfigureMetric(const char* name, const MetricOpts& opts) {
    if (!name || !name[0]) return;
    auto it = s_metrics.find(name);
    if (it == s_metrics.end()) {
        s_order.push_back(name);
        it = s_metrics.emplace(name, UserMetric{}).first;
    }
    it->second.opts = opts;
}

void DrawUserMetrics() {
    if (s_metrics.empty()) return;
    ImGui::Separator();

    auto groups = build_groups();
    for (const auto& grp : groups) {
        if (grp.names.empty()) continue;

        // Build plot title
        std::string title;
        for (size_t i = 0; i < grp.names.size(); i++) {
            if (i) title += " / ";
            title += grp.names[i];
        }

        // y axis label from unit of first metric in group
        const char* y_label = nullptr;
        auto it0 = s_metrics.find(grp.names[0]);
        if (it0 != s_metrics.end() && it0->second.opts.unit)
            y_label = it0->second.opts.unit;

        if (ImPlot::BeginPlot(title.c_str(), ImVec2(-1, 130))) {
            ImPlot::SetupAxes(u8"帧", y_label ? y_label : "");

            // Apply y-axis limits: use union of all fixed ranges in the group,
            // or auto-fit if any member requests it.
            float ymin = 1e30f, ymax = -1e30f;
            bool any_fixed = false;
            for (const auto& n : grp.names) {
                auto it = s_metrics.find(n);
                if (it == s_metrics.end()) continue;
                const MetricOpts& o = it->second.opts;
                if (o.y_min != o.y_max) {
                    ymin = fminf(ymin, o.y_min);
                    ymax = fmaxf(ymax, o.y_max);
                    any_fixed = true;
                }
            }
            if (any_fixed)
                ImPlot::SetupAxisLimits(ImAxis_Y1, ymin, ymax, ImPlotCond_Always);

            for (size_t i = 0; i < grp.names.size(); i++) {
                auto it = s_metrics.find(grp.names[i]);
                if (it == s_metrics.end()) continue;
                const UserMetric& um = it->second;
                ImPlotSpec spec(ImPlotProp_Offset, um.buf.plot_offset());
                if (um.opts.color)
                    spec.SetProp(ImPlotProp_LineColor, um.opts.color);
                ImPlot::PlotLine(grp.names[i].c_str(),
                    um.buf.data(),
                    um.buf.plot_count(),
                    1.0, 0.0,
                    spec);
            }
            ImPlot::EndPlot();
        }
    }
}

} // namespace dui
