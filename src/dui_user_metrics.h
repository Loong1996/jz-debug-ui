#pragma once
#include <cstdint>

namespace dui {

struct MetricOpts {
    const char* unit       = nullptr;  // axis label, e.g. "ms" / "kB"
    float       y_min      = 0.f;     // y_min == y_max → auto-fit
    float       y_max      = 0.f;
    uint32_t    color      = 0;       // 0 = ImPlot default
    bool        share_axis = false;   // group with adjacent share_axis metrics of same unit
};

void PushMetric    (const char* name, float value);
void ConfigureMetric(const char* name, const MetricOpts& opts);
void DrawUserMetrics();  // called from DrawMetrics after built-in plots

} // namespace dui
