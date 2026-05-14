#include <windows.h>
#include <chrono>
#include <imgui.h>
#include "dui_app.h"
#include "dui_world.h"
#include "dui_mock.h"
#include "dui_canvas.h"
#include "dui_inspector.h"
#include "dui_metrics.h"
#include "dui_log.h"
#include "dui_ext.h"
#include "dui_commands.h"
#include "dui_detail.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    dui::App app;
    if (!app.Init(1280, 720, L"Debug UI Demo")) return 1;

    dui::World   world   = dui::MakeMockWorld();
    dui::Metrics metrics;

    // --- Type name registrations ---
    dui::RegisterEntityTypeName(0, u8"主角");
    dui::RegisterEntityTypeName(1, u8"战士");
    dui::RegisterEntityTypeName(2, u8"法师");
    dui::RegisterCellTypeName(1, u8"墙壁");
    dui::RegisterCellTypeName(2, u8"水域");
    dui::RegisterCellTypeName(3, u8"陷阱");

    // --- Extension point demo: custom fields for type-1 entities ---
    dui::RegisterEntityDrawer(1, [](dui::Entity& e) {
        ImGui::Text(u8"速度: vx=%.2f  vy=%.2f", e.vx, e.vy);
    });

    // --- Detail panel demo: pre-formatted multi-line text for type-1 entities ---
    dui::RegisterEntityDetailText(1, [](const dui::Entity& e) -> std::string {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "=== \xE5\x9F\xBA\xE7\xA1\x80 ===\n"         // === 基础 ===
            "ID    : %llu\n"
            "Type  : %u\n"
            "Label : %s\n"
            "\n"
            "=== \xE7\x89\xA9\xE7\x90\x86 ===\n"         // === 物理 ===
            "Pos   : (%.2f, %.2f)\n"
            "Vel   : (%.2f, %.2f)\n"
            "Radius: %.2f\n"
            "\n"
            "=== \xE6\xB8\xB2\xE6\x9F\x93 ===\n"         // === 渲染 ===
            "Color : 0x%08X\n",
            static_cast<unsigned long long>(e.id),
            static_cast<unsigned>(e.type), e.label,
            e.fx, e.fy, e.vx, e.vy, e.radius,
            static_cast<unsigned>(e.color));
        return buf;
    });

    // --- Command panel demo ---
    dui::RegisterCommand(u8"World/重置世界", [&world] {
        world = dui::MakeMockWorld();
        dui::Log(u8"World reset");
    });
    dui::RegisterCommand(u8"World/清空实体", [&world] {
        world.entities.clear();
        world.selected_id = -1;
        dui::LogWarn(u8"all entities cleared");
    });
    dui::RegisterCommand(u8"Player/传送到原点", [&world] {
        if (!world.entities.empty()) {
            auto& p = world.entities[0];
            p.x = p.y = 0; p.fx = p.fy = 0.f;
        }
    });
    dui::RegisterCommand(u8"Player/停止移动", [&world] {
        if (!world.entities.empty()) {
            auto& p = world.entities[0];
            p.vx = p.vy = 0.f;
            dui::Log(u8"player stopped");
        }
    });

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
        dui::DrawCommands();
        dui::DrawEntityDetail(world);
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

        log_acc += dt;
        if (log_acc >= 1.f) {
            log_acc -= 1.f;
            dui::Log(u8"tick #%d", tick_count);
            if (tick_count % 5  == 0) dui::LogWarn(u8"每5帧警告: count=%d", tick_count);
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
