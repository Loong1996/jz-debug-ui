#include <cassert>
#include <cstdio>
#include "dui_metrics.h"

int main() {
    using namespace dui;

    // empty
    RingBuffer<float, 4> rb;
    assert(rb.size() == 0);
    assert(rb.plot_count() == 0);
    assert(rb.plot_offset() == 0);

    // partial fill: 2 of 4
    rb.push(1.f);
    rb.push(2.f);
    assert(rb.size() == 2);
    assert(rb.plot_count() == 2);
    assert(rb.plot_offset() == 0);
    assert(rb.data()[0] == 1.f);
    assert(rb.data()[1] == 2.f);

    // exactly full: 4 of 4, head wraps to 0
    rb.push(3.f);
    rb.push(4.f);
    assert(rb.size() == 4);
    assert(rb.plot_count() == 4);
    assert(rb.plot_offset() == 0);

    // overflow: 5th push overwrites slot 0, head moves to 1
    rb.push(5.f);
    assert(rb.size() == 4);
    assert(rb.plot_count() == 4);
    assert(rb.plot_offset() == 1);
    assert(rb.data()[0] == 5.f);

    std::puts("RingBuffer: all assertions passed");
    return 0;
}
