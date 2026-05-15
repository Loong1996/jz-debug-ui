#include "dui_demo_setup.h"
#include "dui_ext.h"
#include "dui_commands.h"
#include "dui_detail.h"
#include "dui_log.h"
#include "dui_user_metrics.h"
#include "dui_events.h"
#include "dui_hotkeys.h"
#include "dui_mock.h"
#include <imgui.h>
#include <cstdio>
#include <cstdlib>

namespace dui {
namespace demo {

static Metric s_m_ai_ms;
static Metric s_m_ai_nodes;

void SetupRegistrations(World& world) {
    World* wp = &world;

    RegisterMapNames({
        { 0, u8"主城" },
        { 1, u8"副本" },
    });

    RegisterEntityTypeNames({
        { 0, u8"主角" },
        { 1, u8"战士" },
        { 2, u8"法师" },
    });

    RegisterCellTypeNames({
        { 1, u8"墙壁" },
        { 2, u8"水域" },
        { 3, u8"陷阱" },
    });

    // Custom Inspector fields for type-1 entities
    RegisterEntityDrawer(1, [](Entity& e) {
        ImGui::Text(u8"速度: vx=%.2f  vy=%.2f", e.vx, e.vy);
    });

    // All type-0 entities are players
    SetPlayerEntityType(0);

    // Per-entity marker: entity #1002 (a warrior) gets a red triangle
    SetEntityMarker(1002, IM_COL32(255, 80, 80, 230));

    // Detail panel: builder style for type-1 entities
    RegisterEntityDetailText(1, [](const Entity& e, DetailBuilder& db) {
        db.Section(u8"基础")
          .KV("ID",    e.id)
          .KV("Type",  static_cast<int>(e.type))
          .KV("Label", e.label)
          .Blank()
          .Section(u8"物理")
          .KVFmt(u8"Pos", "(%.2f, %.2f)", e.fx, e.fy)
          .KVFmt(u8"Vel", "(%.2f, %.2f)", e.vx, e.vy)
          .KV(u8"Radius", e.radius)
          .Blank()
          .Section(u8"渲染")
          .KVFmt(u8"Color", "0x%08X", static_cast<unsigned>(e.color));
    });

    // Canvas overlay: draw velocity vector for type-1 entities
    RegisterEntityOverlay(1, [](const Entity& e, const CanvasOverlayCtx& ctx) {
        float speed = e.vx * e.vx + e.vy * e.vy;
        if (speed < 0.01f) return;
        ImVec2 from = ctx.ToScreen(e.fx, e.fy);
        float scale = 3.f;
        ImVec2 to   = ctx.ToScreen(e.fx + e.vx * scale, e.fy + e.vy * scale);
        ctx.dl->AddLine(from, to, IM_COL32(255, 220, 60, 200), 1.5f);
        // Arrowhead
        float dx = to.x - from.x, dy = to.y - from.y;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 4.f) {
            float nx = dx / len, ny = dy / len;
            float px = -ny * 4.f, py = nx * 4.f;
            ctx.dl->AddTriangleFilled(
                to,
                ImVec2(to.x - nx * 8.f + px, to.y - ny * 8.f + py),
                ImVec2(to.x - nx * 8.f - px, to.y - ny * 8.f - py),
                IM_COL32(255, 220, 60, 200));
        }
    });

    // Zero-arg commands
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
        if (!wp->entities.empty())
            wp->entities[0].SetPos(0.f, 0.f).SetVel(0.f, 0.f);
    });
    RegisterCommand(u8"Player/停止移动", [wp] {
        if (!wp->entities.empty()) {
            wp->entities[0].SetVel(0.f, 0.f);
            Log(u8"player stopped");
        }
    });

    // Parameterized command: teleport player to custom coords
    RegisterCommandWithArgs(u8"Player/传送到坐标", {
        CommandArg::Int("X", 0, -50.f, 50.f),
        CommandArg::Int("Y", 0, -50.f, 50.f),
    }, [wp](const CommandArgValue* v, int) {
        if (!wp->entities.empty()) {
            wp->entities[0].SetPos(static_cast<float>(v[0].i), static_cast<float>(v[1].i))
                           .SetVel(0.f, 0.f);
            Log(u8"teleported player to (%d, %d)", v[0].i, v[1].i);
        }
    });

    // Hotkey: F5 resets the world
    BindHotkey(ImGuiKey_F5, 0, u8"World/重置世界");

    // Metric configuration — store handles for zero-lookup Push
    s_m_ai_ms    = ConfigureMetric("ai/decision_ms", { "ms" });
    s_m_ai_nodes = ConfigureMetric("ai/path_nodes",  { "nodes", 0.f, 30.f });
}

void PerFrameDemo(World& world, Metrics& metrics, float dt, int& tick_count) {
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();
    TickMockWorld(world, dt);
    metrics.tick_ms.push(
        std::chrono::duration<float>(Clock::now() - t0).count() * 1000.f);
    metrics.entity_count.push(static_cast<float>(world.entities.size()));

    s_m_ai_ms.Push(0.1f + (std::rand() % 100) * 0.003f);
    s_m_ai_nodes.Push(static_cast<float>(5 + std::rand() % 20));

    static float log_acc   = 0.f;
    static float event_acc = 0.f;

    if (EveryNSeconds(1.0f, dt, log_acc)) {
        Log(u8"tick #%d", tick_count);
        if (tick_count % 5  == 0) LogWarn(u8"每5帧警告: count=%d", tick_count);
        if (tick_count % 13 == 0) LogError(u8"模拟错误 #%d", tick_count);
        ++tick_count;
    }

    if (EveryNSeconds(3.0f, dt, event_acc)) {
        static int ecnt = 0;
        if (ecnt % 2 == 0) PushEventFmt("Combat", u8"Boss 刷新 #%d", ecnt);
        else                PushEventFmt("AI",     u8"路径重新规划 #%d", ecnt);
        ecnt++;
        if (ecnt % 7 == 0) PushEvent("System", u8"区块加载完成");
    }

    Watch(u8"默认/实体数", static_cast<int>(world.entities.size()));
    if (!world.entities.empty()) {
        const Entity& p = world.entities[0];
        Watch("player/pos", p.fx, p.fy);
        Watch("player/vel", p.vx, p.vy);
    }
    static float mem_acc = 0.f;
    if (EveryNSeconds(0.5f, dt, mem_acc))
        Watch("mem/heap_kb", static_cast<float>(world.entities.size() * 128 + 4096));
    Watch("ai/state", (tick_count % 7 < 3) ? "patrol" : "chase");
}

} // namespace demo
} // namespace dui
