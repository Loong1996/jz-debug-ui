# dui API 快查手册

所有符号位于 `namespace dui`。颜色统一用 `RGBA(r,g,b,a=220)` 或 `IM_COL32`。

---

## 数据结构

### World
```cpp
struct World {
    std::vector<Entity>   entities;
    std::vector<Cell>     cells;
    uint32_t active_map_id = 0;
    uint64_t selected_id   = 0;   // 主选中；0 = 未选中
    uint64_t player_id     = 0;
    bool     sel_cell_valid;
    int      sel_cell_x, sel_cell_y;
    std::vector<uint64_t> selected_ids; // 多选集合
};
```

### Entity（链式 setter）
```cpp
e.SetPos(fx, fy)          // 同步 x/y
 .SetVel(vx, vy)
 .SetColor(r, g, b, a=220)
 .SetType(uint8_t)
 .SetRadius(float)
 .SetLabel("fmt", ...)
 .SetMapId(uint32_t)
 .SetUserdata(void*);
// 字段：id, map_id, x, y, fx, fy, vx, vy, radius, color, type, label[16], userdata
```

### Cell（链式 setter）
```cpp
c.SetPos(x, y).SetColor(r,g,b).SetType(t).SetLabel("...").SetMapId(m).SetUserdata(v);
// 字段：map_id, x, y, color, type, label[12], userdata
```

---

## 注册名称

```cpp
RegisterMapNames        ({{0, u8"主城"}, {1, u8"副本"}});
RegisterEntityTypeNames ({{0, u8"主角"}, {1, u8"战士"}});
RegisterCellTypeNames   ({{1, u8"墙壁"}, {2, u8"水域"}});
RegisterMapName         (uint32_t id, const char* name);  // 单条
RegisterEntityTypeName  (uint8_t  type, const char* name);
RegisterCellTypeName    (uint8_t  type, const char* name);
const char* GetMapName       (uint32_t id);   // nullptr 表示未注册
const char* GetEntityTypeName(uint8_t type);
const char* GetCellTypeName  (uint8_t type);
```

---

## Inspector 扩展

```cpp
// 在 Inspector 行末追加自定义字段（只读 ImGui 内容）
RegisterEntityDrawer(uint8_t type, [](Entity& e){ ImGui::Text(...); });
RegisterCellDrawer  (uint8_t type, [](Cell&   c){ ... });

// Detail 面板（实体详情窗口）— 支持两种写法
RegisterEntityDetailText(type, [](const Entity& e) -> std::string { return "..."; });
RegisterEntityDetailText(type, [](const Entity& e, DetailBuilder& db){
    db.Section("基础").KV("ID", e.id).KVFmt("Pos","(%.1f,%.1f)",e.fx,e.fy).Blank();
});

// Cell 详情（Inspector 格子折叠区）— 同 Entity 两个重载
RegisterCellDetailText(type, [](const Cell& c) -> std::string { ... });
RegisterCellDetailText(type, [](const Cell& c, DetailBuilder& db){ ... });

// 可编辑区（Detail 面板 / Inspector 折叠"编辑"分区）
RegisterEntityEditor(type, [](Entity& e){
    ImGui::DragFloat2("速度", &e.vx, 0.05f, -5.f, 5.f);
    ImGui::ColorEdit3(...);
});
RegisterCellEditor(type, [](Cell& c){ ... });
```

### DetailBuilder 链式 API
```cpp
db.Section("标题")          // === 标题 ===
  .KV("Key", int/float/uint64_t/const char*)
  .KVFmt("Key", "%.2f px", value)
  .Line("自由格式 %d", n)
  .Blank();                 // 空行
```

---

## Canvas Overlay（画布叠层）

### 实体 / 格子 / 全局
```cpp
RegisterEntityOverlay(type, [](const Entity& e, const CanvasOverlayCtx& ctx){ ... });
RegisterCellOverlay  (type, [](const Cell&   c, const CanvasOverlayCtx& ctx){ ... });
RegisterGlobalOverlay  ("name", [](const World& w, const CanvasOverlayCtx& ctx){ ... });
UnregisterGlobalOverlay("name");
```

### CanvasOverlayCtx 绘图方法（世界坐标，自动转屏幕）
```cpp
ctx.DrawCircle      (cx, cy, r_world, col, thickness=1.5f);
ctx.DrawCircleFilled(cx, cy, r_world, col);
ctx.DrawLine        (x0, y0, x1, y1, col, thickness=1.5f);
ctx.DrawRect        (x0, y0, x1, y1, col, thickness=1.5f);
ctx.DrawRectFilled  (x0, y0, x1, y1, col);
ctx.DrawArrow       (x0, y0, x1, y1, col, thickness=1.5f, head_px=9.f);
ctx.DrawCone        (cx, cy, dx, dy, r_world, half_angle_rad, col);  // 填充扇形
ctx.DrawTextWorld   (wx, wy, col, text);
ctx.DrawPolyline    (ImVec2* world_pts, n, col, thickness=1.5f, closed=false);
// ctx.dl      — ImDrawList*（可直接调用 ImDrawList API）
// ctx.tile_half — screen px / world unit
```

---

## 热力图

```cpp
struct HeatmapOpts {
    float    min_value  = 0.f,  max_value = 1.f;
    uint32_t low_color  = RGBA(0,80,200,100);
    uint32_t high_color = RGBA(220,40,40,180);
    bool     auto_range = false;  // 每帧对视口自动归一
};
RegisterCellHeatmap  ("name", [](int x, int y) -> float { return ...; }, opts);
UnregisterCellHeatmap("name");
```

---

## 连线（Links）

```cpp
// 实体→实体
struct EntityLink { uint64_t target_id; uint32_t color=…; float thickness=1.5f; bool arrow=true; bool dashed=false; };
RegisterEntityLinks  ("name", [](const Entity& src) -> std::vector<EntityLink> { ... });
UnregisterEntityLinks("name");

// 格子→格子
struct CellLink { int target_x, target_y; uint32_t color=…; float thickness=1.5f; bool arrow=true; bool dashed=false; };
RegisterCellLinks  ("name", [](const Cell& src) -> std::vector<CellLink> { ... });
UnregisterCellLinks("name");
```

---

## 右键上下文菜单

```cpp
struct CanvasContextCtx { World* world; ImVec2 click_screen; float wx, wy; uint32_t map_id; };

// 在回调内调用 ImGui::MenuItem / ImGui::Separator 等
RegisterEntityContextMenu(type, [](Entity& e, const CanvasContextCtx& ctx){
    if (ImGui::MenuItem("删除")) DespawnEntity(*ctx.world, e.id);
});
RegisterCellContextMenu  (type, [](Cell& c, const CanvasContextCtx& ctx){ ... });
RegisterCanvasBackgroundContextMenu([](float wx, float wy){ ... });

UnregisterEntityContextMenu(type);
UnregisterCellContextMenu  (type);
UnregisterCanvasBackgroundContextMenu();
// Inspector 行右键自动复用 EntityContextMenu 同一注册表
```

---

## 世界操作

```cpp
bool    DespawnEntity(World& w, uint64_t id);         // 同时清选择/轨迹/标记
Entity& SpawnEntityAt(World& w, float fx, float fy,   // 自动分配 id
                      uint8_t type, const char* label = nullptr);
void    SwitchActiveMap(World& w, uint32_t new_map_id);
```

---

## 多选

```cpp
SelectAdd   (World& w, uint64_t id, bool set_primary = true);
SelectRemove(World& w, uint64_t id);
SelectToggle(World& w, uint64_t id);
SelectClear (World& w);
bool IsSelected(const World& w, uint64_t id);
```

---

## 实体标记 / 玩家类型

```cpp
SetPlayerEntityType(uint8_t type);          // Canvas 画黄色三角
SetEntityMarker(uint64_t id, uint32_t col); // 单实体彩色三角（优先级更高）
ClearEntityMarker(uint64_t id);
SetEntityLabelFn(type, [](const Entity& e) -> std::string { ... }); // 自定义气泡标签
```

---

## 轨迹 & 热度

```cpp
EnableEntityTrails(bool on);      // 主开关，默认 false
SetTrailLength(int n);            // 最大采样数，默认 60
RecordTrailSnapshot(world);       // 每帧 tick 后调用，记录当前位置

AccumulateTileVisits(world);      // 累加格子踩踏热度
SetTileVisitDecay(float decay);   // 0<decay<1，0.99≈半衰期70帧
float GetTileVisitHeat(map_id, x, y);
ClearAllTrails();  ClearTrailsFor(uint64_t id);  ClearTileHeat();
```

---

## 命令面板

```cpp
RegisterCommand  ("Category/名称", []{ ... });
UnregisterCommand("Category/名称");

// 带参命令（打开模态表单）
RegisterCommandWithArgs("Player/传送到坐标", {
    CommandArg::Int("X", 0, -50.f, 50.f),
    CommandArg::Float("Y", 0.f),
    CommandArg::Bool("重置速度", true),
    CommandArg::String("备注"),
    CommandArg::Enum("模式", 0, items, count),
}, [](const CommandArgValue* v, int n){
    // v[i].i / v[i].f / v[i].b / v[i].s
});
```

---

## 热键

```cpp
BindHotkey  (ImGuiKey_F5, 0,                   "World/重置世界");
BindHotkey  (ImGuiKey_R,  ImGuiMod_Ctrl,        "Player/传送到原点");
UnbindHotkey(ImGuiKey_F5, 0);
std::string label = GetHotkeyLabel("World/重置世界");  // "[F5]"

// 命令面板里每条命令右侧的 [K] 按钮打开"绑定快捷键"弹窗，按下任意按键即可绑定。
// 弹窗会跳过修饰键单独按下、Esc 取消、以及所有鼠标键 / 滚轮（避免误绑左键到命令）。
// 绑定持久化到 dui_hotkeys.ini。
```

---

## 日志 / Watch / Tunable

```cpp
Log     ("tick #%d", n);
LogWarn ("警告 %s", msg);
LogError("错误 %d", code);

Watch("player/pos", fx, fy);   // 存为 "(x.xx, y.yy)"
Watch("name", int/float/bool/const char*);
WatchFmt("name", "%.1f ms", v);
RemoveWatch("name");  ClearWatch();

// Tunable：Watch 面板内渲染为滑条/复选框，指针须持续有效
Tunable("ai/攻击强度", &s_aggro, 0.f, 1.f);
Tunable("ai/步数上限", &s_steps, 0, 100);
Tunable("ai/冻结",    &s_freeze);
RemoveTunable("name");

// 辅助
bool EveryNSeconds(float interval, float dt, float& acc);
```

---

## 指标图（Metrics）

```cpp
Metric h = ConfigureMetric("ai/decision_ms", {
    .unit       = "ms",
    .y_min      = 0.f, .y_max = 50.f,  // 0==0 → 自动适应
    .color      = RGBA(255,160,0),
    .share_axis = false,
});
h.Push(3.14f);                          // 零查找推送
PushMetric("ai/decision_ms", 3.14f);    // 按名推送（慢一点）
```

---

## 事件时间线

```cpp
PushEvent   ("Combat", u8"Boss 刷新 #1");
PushEventFmt("AI",     u8"路径重规划 #%d", n);
PushEventFmtColored("System", RGBA(60,200,60), u8"加载完成");
```

---

## 用户面板

```cpp
enum class PanelDock { Center, Left, Right, Bottom, Floating };

RegisterPanel(u8"AI 状态", [](){
    ImGui::Text("...");
}, PanelDock::Right);
UnregisterPanel(u8"AI 状态");
// 须在第一次 App::Tick 前注册，否则面板以浮动形式出现
```

---

## CSV 导出

Watch 面板和 Metric 面板各有 **Export CSV...** 按钮，点击弹出系统保存对话框，输出 UTF-8 BOM 格式的 `.csv` 文件。无需额外 API 调用。

---

## 事件严重级（Round 5）

```cpp
enum class EventSeverity : uint8_t { Info = 0, Warn = 1, Error = 2 };

PushEvent("Combat", u8"Boss 刷新", 0, EventSeverity::Info);   // 默认
PushEvent("AI",     u8"路径失败",  0, EventSeverity::Warn);   // 黄色光晕
PushEvent("System", u8"崩溃转储",  0, EventSeverity::Error);  // 红色光晕
// 旧调用点零改动（默认参数 EventSeverity::Info）
```

---

## 摄像机书签（Round 5）

```cpp
struct CameraBookmark { std::string name; uint32_t map_id; float cam_x, cam_y, zoom; };

SaveCameraBookmark  ("BOSS 房", world);   // 保存当前视角 + 地图
bool ok = GotoCameraBookmark("BOSS 房", world);  // 切地图 + 跳坐标 + 关 follow
DeleteCameraBookmark("BOSS 房");
const std::vector<CameraBookmark>& ListCameraBookmarks();
CanvasView& GetActiveCanvasView();        // 直接读写 cam_x/y/zoom/follow_player/show_links/show_heatmaps
// 默认值：cam=(0,0), zoom=1, follow_player=false, show_heatmaps=false, show_links=true

// 书签持久化到 dui_bookmarks.ini（App::Init 自动加载）
// Canvas 工具栏"书签"按钮提供 GUI 入口
```

---

## 世界标注 Pin（Round 5）

```cpp
struct Pin { uint64_t id; uint32_t map_id; float wx, wy; uint32_t color; std::string text; };

uint64_t AddPin(uint32_t map_id, float wx, float wy,
                const char* text, uint32_t color = RGBA(255, 200, 0));
bool     RemovePin(uint64_t pin_id);
void     ClearAllPins();
void     ClearPinsOnMap(uint32_t map_id);
const std::vector<Pin>& ListPins();
// Inspector "标注 (N)" 折叠区提供 GUI 跳转 / 编辑 / 删除
// 右键背景菜单 "在此添加标注…" 可直接在画布上插钉
```

---

## 格子三件（Round 5）

```cpp
// Spawn / Despawn
Cell& SpawnCellAt (World& w, uint32_t map_id, int x, int y,
                   uint8_t type, const char* label = nullptr);
bool  DespawnCell (World& w, uint32_t map_id, int x, int y);

// 自定义格子标签（覆盖 c.label 字段）
using CellLabelFn = std::function<std::string(const Cell&)>;
void        RegisterCellLabelFn(uint8_t type, CellLabelFn fn);
std::string InvokeCellLabel    (const Cell& c);  // 内部使用
```

---

## World 快照（Round 5）

```cpp
// 保存 / 加载 JSON（含 entities / cells / pins / 选中状态）
bool SaveWorldSnapshot(const World& w, const char* path);
bool LoadWorldSnapshot(World& w,       const char* path);

// 带系统文件对话框的版本（Win32，弹出保存/打开窗口）
bool SaveWorldSnapshotDialog(const World& w);
bool LoadWorldSnapshotDialog(World& w);

// Demo 命令：Ctrl+P → "World/保存快照…" / "World/加载快照…"
// Demo 热键：F6 保存，F7 加载
```

---

## Profiler 火焰图（Round 5）

```cpp
// 插桩
DUI_PROFILE_SCOPE("name");            // RAII，自动嵌套深度

// 面板（DrawAll 已自动调用，无需手动注册）
DrawProfiler();                       // 火焰图 + 均值表

// App::Tick 入口/出口已自动夹 BeginProfilerFrame_/EndProfilerFrame_
// 每帧开始调 BeginProfilerFrame_()，结束调 EndProfilerFrame_()
```

---

## 时间控制（Round 6）

```cpp
// 暂停 / 恢复
bool  IsWorldPaused();
void  SetWorldPaused(bool paused);
bool  ToggleWorldPaused();            // 返回新的 paused 状态

// 速度倍率（范围 0.0625–8.0）
float GetTimeScale();
void  SetTimeScale(float s);

// 单帧步进（调一次 = 下一帧跑 raw_dt，然后重新暂停）
void RequestSingleStep();

// Demo 接入（在 TickMockWorld 之前调用）
float eff = dui::EffectiveDt(dt);    // 暂停→0; 步进→raw_dt 一次; 否则 dt*scale
TickMockWorld(world, eff);

// 热键（Demo 绑定）
//   Space   → 切换暂停
//   .       → 单帧步进
//   [       → 速度 ÷2
//   ]       → 速度 ×2
// 命令：World/暂停 / World/单帧 / World/速度- / World/速度+
```

---

## 实体行为日志（Round 6）

```cpp
#include "dui_entity_log.h"

struct EntityLogEntry { char ts[12]; uint8_t level; char text[160]; };

// 写入（level: 0=Info, 1=Warn, 2=Error）
void LogEntity     (uint64_t id, const char* fmt, ...);
void LogEntityWarn (uint64_t id, const char* fmt, ...);
void LogEntityError(uint64_t id, const char* fmt, ...);

// 配置 / 查询
void SetEntityLogSize(int n_per_entity);              // 默认 32
void ClearEntityLog  (uint64_t id);
const EntityLogEntry* GetEntityLog(uint64_t id, int* out_count);  // 旧→新顺序

// 实体销毁时自动清理（DespawnEntity 内部调用）
// 详情面板"日志"折叠区自动展示最近条目
```

---

## Replay 录制 + 时间倒带（Round 6）

```cpp
#include "dui_replay.h"

// 录制开关（默认关闭）
void EnableReplayRecording(bool on);      // F8 热键切换
bool IsReplayRecording();
void SetReplayBufferSize(int frames);     // 默认 600（≈10s @60fps）
int  GetReplayBufferFrames();             // 已录帧数

// 进入 / 退出回放查看模式
bool IsReplayActive();
void EnterReplay();                       // 暂停 world + 跳到最新帧
void ExitReplay();                        // 恢复之前的 paused 状态

// 时间轴游标
void SetReplayCursor(int frame);
int  GetReplayCursor();
const World* GetReplayWorld();            // replay_active 时返回历史帧

// 面板热键（回放面板中）
//   ⏮ / ⏭   跳到首 / 末帧
//   ◀◀ / ▶▶  后退 / 前进一帧
//   ▶ / ⏸    自动播放 / 暂停
//   "返回实时" 退出回放模式

// 注意：回放期间画布禁止交互；REPLAY 水印标明当前帧号
```

---

## 统一搜索（Round 7）

Ctrl+P 或 Ctrl+F 打开全局搜索面板，一并搜索：

| 前缀 | 类型 | 分值优先级 |
|------|------|-----------|
| `[E]`   | Entity（id/label/type/map） | 3–10 |
| `[CMD]` | 命令（有/无参数） | 7 |
| `[CELL]` | Cell（坐标/label/type） | 1–9 |
| `[PIN]`  | 世界标注 | 1–9 |
| `[BMK]`  | 摄像机书签 | 1–9 |

回车或点击结果项 → 切地图 + 跳坐标 + 选中；命令直接执行。

---

## 小地图（Round 7）

内置面板 **小地图**，默认停靠在右侧。

- 灰色背景俯视图，显示当前地图所有实体（彩色圆点）和格子（彩色方块）
- 玩家实体（SetPlayerEntityType 注册的类型）用**黄色**显示
- 白色矩形框 = 场景视图当前视口（跟随 cam_x/y/zoom 实时更新）
- 左键点击小地图 → 主画布摄像机跳到对应坐标（退出 follow 模式）
- 悬浮时 tooltip 显示鼠标处的世界坐标

---

## Inspector 过滤器收藏（Round 7）

检视器 → 实体列表工具栏 → **★** 按钮打开预设管理 Popup：

- 点击已保存的预设名称 → 立即应用（search / type_filter / sort_mode / group）
- `×` 删除单个预设
- 输入名称 + **保存** → 保存当前过滤组合为新预设
- 预设持久化到 `dui_inspector_presets.ini`，重启后自动加载

---

## 选择组 Ctrl+1..9（Round 7）

```cpp
#include "dui_select_group.h"

void SaveSelectionGroup  (World& w, int slot); // slot 1..9，保存当前多选集合
void RecallSelectionGroup(World& w, int slot); // 恢复并选中该组（不在则无操作）
bool HasSelectionGroup   (int slot);           // 该槽是否有保存
int  CountSelectionGroup (int slot);           // 槽内实体数

// 热键（Demo 绑定）
//   Ctrl+Shift+1..9 → 保存选择组 1..9
//   Ctrl+1..9       → 恢复选择组 1..9
// 命令：Select/保存组N / Select/恢复组N（N=1..9）
```

---

## 图层开关（Round 7）

```cpp
// 枚举所有已注册图层（GlobalOverlay / Heatmap / EntityLinks / CellLinks /
//                    EntityOverlay / CellOverlay）
struct LayerInfo { const char* kind; const char* name; bool enabled; };
void ListLayers     (std::vector<LayerInfo>& out);
void SetLayerEnabled(const char* kind, const char* name, bool on);

// 面板：视图 → 图层（默认停靠右侧）
//
// 分组结构：
//   [☑] 热力图 ▶            ← 整体 checkbox 绑定 CanvasView.show_heatmaps
//        [☑] tile_visits     ← 逐项 checkbox 绑定 HeatmapEntry.enabled
//   [☑] 连线 ▶              ← 整体 checkbox 绑定 CanvasView.show_links
//        [☑] 目标连线        ← EntityLinks + CellLinks 合并在此分组下
//        [☑] 水→陷阱
//        全局 Overlay ▶      ← 普通折叠，无组级 checkbox
//        [☑] xxx
//        实体 Overlay ▶      ← 仅列已 RegisterEntityOverlay 的类型
//        [☑] 战士
//        格子 Overlay ▶      ← 仅列已 RegisterCellOverlay 的类型
//        [☑] 水域
//
// 整体关闭时所有逐项 checkbox 置灰但仍可见。
// 场景视图工具栏第一行只保留 6 项基础显示（格线/格子/实体/标签/坐标轴/轨迹），
// 连线 / 热力图 的批量开关已迁移到本面板，避免重叠。
```
