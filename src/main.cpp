#include <windows.h>
#include "dui.h"              // 库聚合头 —— 一行引入所有 dui API
#include "dui_mock.h"         // demo only — 不在库里
#include "dui_demo_setup.h"   // demo only — 不在库里

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
