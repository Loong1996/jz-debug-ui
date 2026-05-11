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
    auto last_ui   = Clock::now();
    auto last_tick = Clock::now();

    while (app.PumpMessages()) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - last_tick).count();
        if (dt > 0.05f) dt = 0.05f;
        last_tick = now;

        auto t0 = Clock::now();
        dui::TickMockWorld(world, dt);
        float tick_ms = std::chrono::duration<float>(
            Clock::now() - t0).count() * 1000.f;
        metrics.tick_ms.push(tick_ms);
        metrics.entity_count.push(
            static_cast<float>(world.entities.size()));

        if (now - last_ui >= std::chrono::milliseconds(16)) {
            last_ui = now;
            app.BeginFrame();
            dui::DrawInspector(world);
            dui::DrawCanvas(world);
            dui::DrawMetrics(metrics);
            app.EndFrame();
        }
    }

    app.Shutdown();
    return 0;
}
