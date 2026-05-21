#pragma once
// dui — 单文件聚合头：一次 #include "dui.h" 引入所有库 API。
//
// 适用场景：游戏服务端 / 工具 / 任何"懒得选头文件"的接入点。
// 想精简编译时间时，可改用对应的细分头：dui_app.h / dui_world.h / dui_draw_all.h ...
//
// 注：dui_mock.h / dui_demo_setup.h 不在这里 —— 那两个属于 Demo 自身代码，不是库 API。

// ---- 核心 ----
#include "dui_app.h"          // App 生命周期：Init / Attach / Tick / BeginFrame / EndFrame
#include "dui_world.h"        // Entity / Cell / World 数据结构
#include "dui_draw_all.h"     // DrawAll(world[, metrics]) — 一次绘制所有面板

// ---- 面板 ----
#include "dui_canvas.h"       // 场景视图 + CanvasView + 摄像机书签
#include "dui_inspector.h"    // 检视器
#include "dui_detail.h"       // 实体详情 + DetailBuilder
#include "dui_metrics.h"      // 性能指标（可选 Metrics）
#include "dui_log.h"          // 全局日志
#include "dui_events.h"       // 事件时间线
#include "dui_commands.h"     // 命令面板 + 命令注册
#include "dui_menubar.h"      // 菜单栏 + 面板可见性
#include "dui_search.h"       // Ctrl+F / Ctrl+P 全局搜索
#include "dui_profiler.h"     // 帧火焰图 (DUI_PROFILE_SCOPE)
#include "dui_replay.h"       // 录制 + 时间倒带
#include "dui_minimap.h"      // 小地图
#include "dui_layers.h"       // 图层开关面板

// ---- 注册 / 扩展点 ----
#include "dui_ext.h"          // 类型名 / Overlay / Heatmap / Links / 标记 / 多选 / 图层枚举
#include "dui_hotkeys.h"      // 热键绑定
#include "dui_pins.h"         // 世界标注
#include "dui_trails.h"       // 实体轨迹 + 格子热度
#include "dui_snapshot.h"     // World JSON 快照
#include "dui_time.h"         // 世界暂停 / 单帧 / 速度倍率
#include "dui_entity_log.h"   // 每实体行为日志
#include "dui_select_group.h" // 选择组 Ctrl+1..9
#include "dui_user_metrics.h" // 自定义曲线 / Watch / Tunable
