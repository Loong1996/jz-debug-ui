#include <windows.h>
#include "dui_app.h"
#include "dui_world.h"
#include "dui_mock.h"
#include "dui_metrics.h"
#include "dui_draw_all.h"
#include "dui_demo_setup.h"
#include "dui_trails.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    dui::App app;
    if (!app.Init(1280, 720, L"Debug UI Demo")) return 1;

    dui::World   world   = dui::MakeMockWorld();
    dui::Metrics metrics;
    dui::demo::SetupRegistrations(world);

    int tick_count = 0;
    while (app.Tick([&](float dt) {
        dui::demo::PerFrameDemo(world, metrics, dt, tick_count);
        dui::DrawAll(world, metrics);
    })) {}
    return 0;
}
