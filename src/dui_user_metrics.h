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

// Handle returned by ConfigureMetric. Push() skips the per-call name lookup.
struct Metric {
    int idx = -1;
    bool valid() const { return idx >= 0; }
    void Push(float value) const;
};

// ConfigureMetric returns a Metric handle for zero-lookup Push.
// Callers that ignore the return value continue to work unchanged.
Metric ConfigureMetric(const char* name, const MetricOpts& opts);

void PushMetric    (const char* name, float value);
void DrawUserMetrics();  // called from DrawMetrics after built-in plots

} // namespace dui
