#include "dui_demo_setup.h"
#include "dui_ext.h"
#include "dui_commands.h"
#include "dui_detail.h"
#include "dui_log.h"
#include "dui_user_metrics.h"
#include "dui_events.h"
#include "dui_hotkeys.h"
#include "dui_mock.h"
#include "dui_trails.h"
#include <imgui.h>
#include <chrono>
#include <cmath>
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

    // Canvas overlay: velocity arrow + vision cone + aggro circle for type-1 warriors
    RegisterEntityOverlay(1, [](const Entity& e, const CanvasOverlayCtx& ctx) {
        // Velocity arrow (uses new DrawArrow helper)
        float speed = e.vx * e.vx + e.vy * e.vy;
        if (speed > 0.01f)
            ctx.DrawArrow(e.fx, e.fy, e.fx + e.vx * 3.f, e.fy + e.vy * 3.f,
                          IM_COL32(255, 220, 60, 200));
        // Vision cone — 120° forward view, radius 4 world units
        float vlen = sqrtf(speed);
        float dx = vlen > 0.01f ? e.vx / vlen : 1.f;
        float dy = vlen > 0.01f ? e.vy / vlen : 0.f;
        ctx.DrawCone(e.fx, e.fy, dx, dy, 4.f, 1.047f,  // 60° half-angle
                     IM_COL32(255, 220, 60, 35));
        // Aggro radius — 2 world units
        ctx.DrawCircle(e.fx, e.fy, 2.f, IM_COL32(255, 80, 80, 100), 1.f);
    });

    // ---- Context menu demo: right-click type-1 warriors ----
    RegisterEntityContextMenu(1, [wp](Entity& e, const CanvasContextCtx& ctx) {
        if (ImGui::MenuItem(u8"传送到此处")) {
            e.SetPos(ctx.wx, ctx.wy).SetVel(0.f, 0.f);
            Log(u8"warrior #%llu teleported to (%.0f, %.0f)",
                static_cast<unsigned long long>(e.id), ctx.wx, ctx.wy);
        }
        if (ImGui::MenuItem(u8"染红")) e.SetColor(220, 60, 60);
        if (ImGui::MenuItem(u8"染蓝")) e.SetColor(60, 120, 255);
        if (ImGui::MenuItem(u8"停止移动")) e.SetVel(0.f, 0.f);
        ImGui::Separator();
        if (ImGui::MenuItem(u8"删除")) {
            DespawnEntity(*ctx.world, e.id);
            Log(u8"warrior #%llu despawned", static_cast<unsigned long long>(e.id));
        }
    });

    // Background context menu: spawn a new warrior at the right-clicked position
    RegisterCanvasBackgroundContextMenu([wp](float bx, float by) {
        if (ImGui::MenuItem(u8"在此生成战士")) {
            Entity& ne = SpawnEntityAt(*wp, bx, by, 1, u8"新战士");
            ne.SetColor(100, 200, 100);
            Log(u8"spawned warrior #%llu at (%.0f, %.0f)",
                static_cast<unsigned long long>(ne.id), bx, by);
        }
    });

    // ---- Entity editor demo: drag-edit type-1 velocity and color ----
    RegisterEntityEditor(1, [](Entity& e) {
        ImGui::DragFloat2(u8"速度", &e.vx, 0.05f, -5.f, 5.f);
        ImGui::DragFloat(u8"半径", &e.radius, 0.01f, 0.1f, 1.5f);
        float col[3] = {
            static_cast<float>(e.color & 0xFF) / 255.f,
            static_cast<float>((e.color >> 8) & 0xFF) / 255.f,
            static_cast<float>((e.color >> 16) & 0xFF) / 255.f,
        };
        if (ImGui::ColorEdit3(u8"颜色", col))
            e.SetColor(static_cast<uint8_t>(col[0]*255),
                       static_cast<uint8_t>(col[1]*255),
                       static_cast<uint8_t>(col[2]*255));
    });

    // ---- Cell detail text demo: type-1 wall cells show passability info ----
    RegisterCellDetailText(1, [](const Cell& c, DetailBuilder& db) {
        db.Section(u8"属性")
          .KV(u8"可通行", "否")
          .KV(u8"类型",   "墙壁")
          .KVFmt(u8"坐标", "(%d, %d)", c.x, c.y);
    });

    // ---- Cell links demo: type-2 (水域) cells link to nearest type-3 (陷阱) cell ----
    RegisterCellLinks(u8"水→陷阱", [wp](const Cell& src) -> std::vector<CellLink> {
        if (src.type != 2) return {};
        float best = 1e30f; int bx = 0, by = 0; bool found = false;
        for (const auto& c : wp->cells) {
            if (c.type != 3 || c.map_id != src.map_id) continue;
            float dx = static_cast<float>(c.x - src.x), dy = static_cast<float>(c.y - src.y);
            float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; bx = c.x; by = c.y; found = true; }
        }
        if (!found) return {};
        return { { bx, by, RGBA(80, 200, 255, 180), 1.5f, true, true } };
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

    // ---- Cell overlay demo: draw X icon on water (type 2) and trap (type 3) cells ----
    auto draw_x_icon = [](const Cell& c, const CanvasOverlayCtx& ctx) {
        ImVec2 center = ctx.ToScreen(static_cast<float>(c.x), static_cast<float>(c.y));
        float  r      = 5.f;
        uint32_t col  = (c.type == 3) ? IM_COL32(255, 60, 60, 220) : IM_COL32(80, 180, 255, 200);
        ctx.dl->AddLine(ImVec2(center.x - r, center.y - r), ImVec2(center.x + r, center.y + r), col, 1.5f);
        ctx.dl->AddLine(ImVec2(center.x + r, center.y - r), ImVec2(center.x - r, center.y + r), col, 1.5f);
    };
    RegisterCellOverlay(2, draw_x_icon);
    RegisterCellOverlay(3, draw_x_icon);

    // ---- Heatmap demo: tile visit frequency — redder = more often walked over ----
    RegisterCellHeatmap(u8"经过频率", [wp](int x, int y) -> float {
        return GetTileVisitHeat(wp->active_map_id, x, y);
    }, { 0.f, 1.f, RGBA(0, 60, 180, 0), RGBA(255, 60, 0, 220), true });

    // ---- Entity links demo: each type-1 warrior points to its nearest same-type peer ----
    RegisterEntityLinks(u8"最近战士", [wp](const Entity& src) -> std::vector<EntityLink> {
        if (src.type != 1) return {};
        float best_dist2 = 1e30f;
        uint64_t best_id = 0;
        for (const auto& e : wp->entities) {
            if (e.id == src.id || e.type != 1 || e.map_id != src.map_id) continue;
            float dx = e.fx - src.fx, dy = e.fy - src.fy;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_dist2) { best_dist2 = d2; best_id = e.id; }
        }
        if (best_id == 0) return {};
        return { { best_id, RGBA(255, 140, 0, 200), 1.5f, true, true } };
    });

    // ---- Trails demo: enable trails with 80-sample history ----
    EnableEntityTrails(true);
    SetTrailLength(80);
    SetTileVisitDecay(0.99f);

    // ---- Custom user panel demo ----
    RegisterPanel(u8"AI 状态", [wp] {
        if (wp->entities.empty()) { ImGui::TextDisabled("(no entities)"); return; }
        const auto& p = wp->entities[0];
        ImGui::Text(u8"玩家位置: (%.1f, %.1f)", p.fx, p.fy);
        ImGui::Text(u8"实体总数: %d", static_cast<int>(wp->entities.size()));
        ImGui::Text(u8"多选数量: %d", static_cast<int>(wp->selected_ids.size()));
        ImGui::Separator();
        ImGui::TextDisabled(u8"用 RegisterPanel 注册的用户面板示例");
    }, PanelDock::Right);

    // ---- Multi-selection batch command demos ----
    RegisterCommand(u8"Selection/清除选择", [wp] { SelectClear(*wp); });

    RegisterCommand(u8"Selection/选中实体染红", [wp] {
        for (auto id : wp->selected_ids)
            for (auto& e : wp->entities)
                if (e.id == id) { e.SetColor(220, 60, 60); break; }
        Log(u8"colored %d entities", static_cast<int>(wp->selected_ids.size()));
    });

    RegisterCommand(u8"Selection/选中实体传送到原点", [wp] {
        for (auto id : wp->selected_ids)
            for (auto& e : wp->entities)
                if (e.id == id) { e.SetPos(0.f, 0.f).SetVel(0.f, 0.f); break; }
    });
}

void PerFrameDemo(World& world, Metrics& metrics, float dt, int& tick_count) {
    using Clock = std::chrono::steady_clock;
    auto t0 = Clock::now();
    TickMockWorld(world, dt);
    RecordTrailSnapshot(world);
    AccumulateTileVisits(world);
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

    // Tunable demo: live-adjustable AI aggression drives the warrior color intensity
    static float s_aggro   = 0.5f;
    static bool  s_freeze  = false;
    Tunable(u8"ai/攻击强度", &s_aggro, 0.f, 1.f);
    Tunable(u8"ai/冻结实体", &s_freeze);
    if (!s_freeze)
        Watch(u8"ai/aggro_pct", static_cast<int>(s_aggro * 100));
}

} // namespace demo
} // namespace dui
