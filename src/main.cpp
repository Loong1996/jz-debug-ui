#include <windows.h>
#include <chrono>
#include "dui_app.h"
#include "dui_world.h"
#include "dui_canvas.h"
#include "dui_inspector.h"
#include "dui_metrics.h"
#include "dui_log.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    dui::App app;
    if (!app.Init(1280, 720, L"Debug UI Demo")) return 1;

    dui::World   world   = dui::MakeMockWorld();
    dui::Metrics metrics;

    using Clock = std::chrono::steady_clock;
    auto  last_tick  = Clock::now();
    float log_acc    = 0.f;
    int   tick_count = 0;

    while (app.Tick([&]() {
        dui::DrawInspector(world);
        dui::DrawCanvas(world);
        dui::DrawMetrics(metrics);
        dui::DrawLog();
        dui::DrawWatch();
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

        // Sample Log / Watch data
        log_acc += dt;
        if (log_acc >= 1.f) {
            log_acc -= 1.f;
            dui::Log(u8"tick #%d", tick_count);
            if (tick_count % 5 == 0) dui::LogWarn(u8"每5帧警告: count=%d", tick_count);
            if (tick_count % 13 == 0) dui::LogError(u8"模拟错误 #%d", tick_count);
            ++tick_count;
        }
        dui::Watch(u8"实体数", static_cast<int>(world.entities.size()));
        if (!world.entities.empty()) {
            dui::Watch(u8"player_x", world.entities[0].x);
            dui::Watch(u8"player_y", world.entities[0].y);
        }
    }

    return 0;
}
