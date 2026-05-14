#include <windows.h>
#include <chrono>
#include "dui_app.h"
#include "dui_world.h"
#include "dui_mock.h"
#include "dui_metrics.h"
#include "dui_draw_all.h"
#include "dui_demo_setup.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    dui::App app;
    if (!app.Init(1280, 720, L"Debug UI Demo")) return 1;

    dui::World   world   = dui::MakeMockWorld();
    dui::Metrics metrics;
    dui::demo::SetupRegistrations(world);

    using Clock = std::chrono::steady_clock;
    auto last_tick  = Clock::now();
    int  tick_count = 0;

    while (app.Tick([&]() { dui::DrawAll(world, metrics); })) {
        auto  now = Clock::now();
        float dt  = std::chrono::duration<float>(now - last_tick).count();
        if (dt > 0.05f) dt = 0.05f;
        last_tick = now;
        dui::demo::PerFrameDemo(world, metrics, dt, tick_count);
    }
    return 0;
}
