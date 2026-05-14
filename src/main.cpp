#include <windows.h>
#include <chrono>
#include <imgui.h>
#include <imgui_internal.h>
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

    // --- Map name registrations ---
    dui::RegisterMapName(0, u8"主城");
    dui::RegisterMapName(1, u8"副本");

    // --- Entity / cell type name registrations ---
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

    // --- All type-0 entities are players; Canvas shows yellow triangle marker above them ---
    dui::SetPlayerEntityType(0);

    // --- Per-entity marker demo: mark entity #1002 (a warrior) with a red triangle ---
    dui::SetEntityMarker(1002, IM_COL32(255, 80, 80, 230));

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
        ImGuiID dsid = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        static bool layout_inited = false;
        if (!layout_inited) {
            layout_inited = true;
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dsid);
            if (node == nullptr || node->IsLeafNode()) {
                ImGui::DockBuilderRemoveNode(dsid);
                ImGui::DockBuilderAddNode(dsid, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dsid, ImGui::GetMainViewport()->Size);

                ImGuiID mid = dsid;
                ImGuiID bot = ImGui::DockBuilderSplitNode(mid, ImGuiDir_Down,  0.28f, nullptr, &mid);
                ImGuiID lft = ImGui::DockBuilderSplitNode(mid, ImGuiDir_Left,  0.20f, nullptr, &mid);
                ImGuiID rgt = ImGui::DockBuilderSplitNode(mid, ImGuiDir_Right, 0.24f, nullptr, &mid);

                ImGui::DockBuilderDockWindow(u8"检视器",   lft);
                ImGui::DockBuilderDockWindow(u8"场景视图", mid);
                ImGui::DockBuilderDockWindow(u8"实体详情", rgt);
                ImGui::DockBuilderDockWindow(u8"日志",     bot);
                ImGui::DockBuilderDockWindow(u8"监视",     bot);
                ImGui::DockBuilderDockWindow(u8"性能指标", bot);
                ImGui::DockBuilderDockWindow(u8"命令",     bot);
                ImGui::DockBuilderFinish(dsid);
            }
        }
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
