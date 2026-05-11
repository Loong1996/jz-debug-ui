#include <windows.h>
#include <chrono>
#include "dui_app.h"
#include "dui_world.h"
#include "dui_canvas.h"
#include "dui_inspector.h"
#include "dui_metrics.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    dui::App app;
    if (!app.Init(1280, 720, L"Debug UI Demo")) return 1;

    dui::World   world   = dui::MakeMockWorld();
    dui::Metrics metrics;

    using Clock = std::chrono::steady_clock;
    auto last_tick = Clock::now();

    // app.Tick() pumps messages, draws one frame, then returns.
    // Put game logic (tick + metrics update) in the loop body.
    while (app.Tick([&]() {
        dui::DrawInspector(world);
        dui::DrawCanvas(world);
        dui::DrawMetrics(metrics);
    })) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - last_tick).count();
        if (dt > 0.05f) dt = 0.05f;
        last_tick = now;

        auto t0 = Clock::now();
        dui::TickMockWorld(world, dt);
        metrics.tick_ms.push(
            std::chrono::duration<float>(Clock::now() - t0).count() * 1000.f);
        metrics.entity_count.push(static_cast<float>(world.entities.size()));
    }

    return 0;
}
