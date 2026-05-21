#pragma once
#include <array>

namespace dui {

template<typename T, int N>
class RingBuffer {
public:
    RingBuffer() : head_(0), count_(0) { buf_.fill(T{}); }

    void push(T v) {
        buf_[head_] = v;
        head_ = (head_ + 1) % N;
        if (count_ < N) ++count_;
    }

    const T* data()     const { return buf_.data(); }
    int      capacity() const { return N; }
    int      size()     const { return count_; }
    // For ImPlot scrolling ring-buffer display
    int plot_count()    const { return (count_ == N) ? N     : count_; }
    int plot_offset()   const { return (count_ == N) ? head_ : 0; }

private:
    std::array<T, N> buf_;
    int head_;
    int count_;
};

struct Metrics {
    RingBuffer<float, 300> tick_ms;
    RingBuffer<float, 300> entity_count;
};

// Built-in tick_ms / entity_count line plots + user metric curves
// (ConfigureMetric / Push) + Tunable controls.
void DrawMetrics(const Metrics& m);

// Variant for projects that don't manage a Metrics instance. The panel still
// shows user metric curves and Tunable controls; built-in plots are skipped.
void DrawMetrics();

} // namespace dui
