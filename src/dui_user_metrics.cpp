#include "dui_user_metrics.h"
#include "dui_metrics.h"
#include <imgui.h>
#include <implot.h>
#include <string>
#include <vector>
#include <cstdio>
#pragma push_macro("SetProp")
#include <windows.h>
#pragma pop_macro("SetProp")

namespace dui {
namespace {

struct UserMetric {
    std::string            name;
    RingBuffer<float, 300> buf;
    MetricOpts             opts;
};

static std::vector<UserMetric> s_metrics;

static int find_or_add(const char* name) {
    for (int i = 0; i < static_cast<int>(s_metrics.size()); i++)
        if (s_metrics[i].name == name) return i;
    s_metrics.push_back({ std::string(name), {}, {} });
    return static_cast<int>(s_metrics.size()) - 1;
}

struct PlotGroup {
    std::vector<int> indices;
    bool        is_shared;
    const char* unit;
};

static std::vector<PlotGroup> build_groups() {
    std::vector<PlotGroup> groups;
    for (int i = 0; i < static_cast<int>(s_metrics.size()); i++) {
        const MetricOpts& opts = s_metrics[i].opts;
        bool merge = opts.share_axis
            && !groups.empty()
            && groups.back().is_shared
            && ((!groups.back().unit && !opts.unit)
                || (groups.back().unit && opts.unit
                    && std::string(groups.back().unit) == std::string(opts.unit)));
        if (merge) {
            groups.back().indices.push_back(i);
        } else {
            PlotGroup g;
            g.indices.push_back(i);
            g.is_shared = opts.share_axis;
            g.unit      = opts.unit;
            groups.push_back(std::move(g));
        }
    }
    return groups;
}

static void export_metrics_csv() {
    if (s_metrics.empty()) return;
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"CSV\0*.csv\0All\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrDefExt = L"csv";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) return;
    FILE* f = _wfopen(path, L"wb");
    if (!f) return;
    fwrite("\xEF\xBB\xBF", 1, 3, f);  // UTF-8 BOM
    fputs("index", f);
    for (const auto& m : s_metrics) { fputs(",", f); fputs(m.name.c_str(), f); }
    fputs("\n", f);
    int max_count = 0;
    for (const auto& m : s_metrics) if (m.buf.plot_count() > max_count) max_count = m.buf.plot_count();
    for (int i = 0; i < max_count; i++) {
        fprintf(f, "%d", i);
        for (const auto& m : s_metrics) {
            int cnt = m.buf.plot_count();
            if (i < cnt)
                fprintf(f, ",%.4f", m.buf.data()[(m.buf.plot_offset() + i) % 300]);
            else
                fputs(",", f);
        }
        fputs("\n", f);
    }
    fclose(f);
}

} // anonymous namespace

void Metric::Push(float value) const {
    if (idx < 0 || idx >= static_cast<int>(s_metrics.size())) return;
    s_metrics[idx].buf.push(value);
}

Metric ConfigureMetric(const char* name, const MetricOpts& opts) {
    if (!name || !name[0]) return {};
    int i = find_or_add(name);
    s_metrics[i].opts = opts;
    return { i };
}

void PushMetric(const char* name, float value) {
    if (!name || !name[0]) return;
    s_metrics[find_or_add(name)].buf.push(value);
}

void DrawUserMetrics() {
    if (s_metrics.empty()) return;
    if (ImGui::SmallButton(u8"Export CSV...")) export_metrics_csv();
    ImGui::Separator();

    auto groups = build_groups();
    for (const auto& grp : groups) {
        if (grp.indices.empty()) continue;

        std::string title;
        for (size_t i = 0; i < grp.indices.size(); i++) {
            if (i) title += " / ";
            title += s_metrics[grp.indices[i]].name;
        }

        const char* y_label = s_metrics[grp.indices[0]].opts.unit;

        if (ImPlot::BeginPlot(title.c_str(), ImVec2(-1, 130))) {
            ImPlot::SetupAxes(u8"帧", y_label ? y_label : "");

            float ymin = 1e30f, ymax = -1e30f;
            bool any_fixed = false;
            for (int idx : grp.indices) {
                const MetricOpts& o = s_metrics[idx].opts;
                if (o.y_min != o.y_max) {
                    ymin = fminf(ymin, o.y_min);
                    ymax = fmaxf(ymax, o.y_max);
                    any_fixed = true;
                }
            }
            if (any_fixed)
                ImPlot::SetupAxisLimits(ImAxis_Y1, ymin, ymax, ImPlotCond_Always);

            for (int idx : grp.indices) {
                const UserMetric& um = s_metrics[idx];
                ImPlotSpec spec(ImPlotProp_Offset, um.buf.plot_offset());
                if (um.opts.color)
                    spec.SetProp(ImPlotProp_LineColor, um.opts.color);
                ImPlot::PlotLine(um.name.c_str(),
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
