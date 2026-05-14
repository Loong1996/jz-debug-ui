#include "dui_demo_setup.h"
#include "dui_ext.h"
#include "dui_commands.h"
#include "dui_detail.h"
#include "dui_log.h"
#include "dui_mock.h"
#include <imgui.h>
#include <chrono>
#include <cstdio>

namespace dui {
namespace demo {

void SetupRegistrations(World& world) {
    World* wp = &world;

    // Map names
    RegisterMapName(0, u8"主城");
    RegisterMapName(1, u8"副本");

    // Entity / cell type names
    RegisterEntityTypeName(0, u8"主角");
    RegisterEntityTypeName(1, u8"战士");
    RegisterEntityTypeName(2, u8"法师");
    RegisterCellTypeName(1, u8"墙壁");
    RegisterCellTypeName(2, u8"水域");
    RegisterCellTypeName(3, u8"陷阱");

    // Custom Inspector fields for type-1 entities
    RegisterEntityDrawer(1, [](Entity& e) {
        ImGui::Text(u8"速度: vx=%.2f  vy=%.2f", e.vx, e.vy);
    });

    // All type-0 entities are players
    SetPlayerEntityType(0);

    // Per-entity marker: entity #1002 (a warrior) gets a red triangle
    SetEntityMarker(1002, IM_COL32(255, 80, 80, 230));

    // Detail panel: pre-formatted text for type-1 entities
    RegisterEntityDetailText(1, [](const Entity& e) -> std::string {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "=== \xe5\x9f\xba\xe7\xa1\x80 ===\n"
            "ID    : %llu\n"
            "Type  : %u\n"
            "Label : %s\n"
            "\n"
            "=== \xe7\x89\xa9\xe7\x90\x86 ===\n"
            "Pos   : (%.2f, %.2f)\n"
            "Vel   : (%.2f, %.2f)\n"
            "Radius: %.2f\n"
            "\n"
            "=== \xe6\xb8\xb2\xe6\x9f\x93 ===\n"
            "Color : 0x%08X\n",
            static_cast<unsigned long long>(e.id),
            static_cast<unsigned>(e.type), e.label,
            e.fx, e.fy, e.vx, e.vy, e.radius,
            static_cast<unsigned>(e.color));
        return buf;
    });

    // Commands
    RegisterCommand(u8"World/重置世界", [wp] {
        *wp = MakeMockWorld();
        Log(u8"World reset");
    });
    RegisterCommand(u8"World/清空实体", [wp] {
        wp->entities.clear();
        wp->selected_id = -1;
        LogWarn(u8"all entities cleared");
    });
    RegisterCommand(u8"Player/传送到原点", [wp] {
        if (!wp->entities.empty()) {
            auto& p = wp->entities[0];
            p.x = p.y = 0; p.fx = p.fy = 0.f;
        }
    });
    RegisterCommand(u8"Player/停止移动", [wp] {
        if (!wp->entities.empty()) {
            auto& p = wp->entities[0];
            p.vx = p.vy = 0.f;
            Log(u8"player stopped");
        }
    });
}

void PerFrameDemo(World& world, Metrics& metrics, float dt, int& tick_count) {
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();
    TickMockWorld(world, dt);
    metrics.tick_ms.push(
        std::chrono::duration<float>(Clock::now() - t0).count() * 1000.f);
    metrics.entity_count.push(static_cast<float>(world.entities.size()));

    static float log_acc = 0.f;
    log_acc += dt;
    if (log_acc >= 1.f) {
        log_acc -= 1.f;
        Log(u8"tick #%d", tick_count);
        if (tick_count % 5  == 0) LogWarn(u8"每5帧警告: count=%d", tick_count);
        if (tick_count % 13 == 0) LogError(u8"模拟错误 #%d", tick_count);
        ++tick_count;
    }
    Watch(u8"实体数", static_cast<int>(world.entities.size()));
    if (!world.entities.empty()) {
        Watch(u8"player_x", world.entities[0].x);
        Watch(u8"player_y", world.entities[0].y);
    }
}

} // namespace demo
} // namespace dui
