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
