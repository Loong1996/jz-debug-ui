# dui — 游戏调试 UI 库

基于 Dear ImGui + ImPlot + DirectX 11 的游戏内调试面板库，支持独立运行和注入式嵌入两种接入方式。

---

## 目录

- [快速开始](#快速开始)
- [接入到游戏项目](#接入到游戏项目)
  - [Visual Studio 项目引入（推荐）](#visual-studio-项目引入推荐)
  - [CMake 项目引入](#cmake-项目引入)
- [接入方式](#接入方式)
  - [独立窗口模式](#独立窗口模式-init)
  - [注入式模式](#注入式模式-attach)
- [数据结构](#数据结构)
- [面板一览](#面板一览)
  - [场景视图 DrawCanvas](#场景视图-drawcanvas)
  - [检视器 DrawInspector](#检视器-drawinspector)
  - [实体详情 DrawEntityDetail](#实体详情-drawentitydetail)
  - [Metrics DrawMetrics](#metrics-drawmetrics)
  - [日志 DrawLog](#日志-drawlog)
  - [监视 DrawWatch](#监视-drawwatch)
  - [命令面板 DrawCommands](#命令面板-drawcommands)
  - [其余内置面板](#其余内置面板)
- [调试深水区（Round 6 / Round 7）](#调试深水区round-6--round-7)
- [扩展点](#扩展点)
  - [Entity / Cell 自定义字段](#entitycell-自定义字段)
  - [命令注册](#命令注册)
- [构建 Demo](#构建-demo)
- [目录结构](#目录结构)

---

## 快速开始

```cpp
#include "dui_app.h"
#include "dui_world.h"
#include "dui_draw_all.h"   // DrawAll = 所有面板一次调用

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    dui::App app;
    app.Init(1280, 720, L"Game Debug");

    dui::World   world;    // 每帧从游戏状态填充
    dui::Metrics metrics;

    while (app.Tick([&]() {
        dui::DrawAll(world, metrics);
    })) {
        // 游戏逻辑 / 填充 world ...
        dui::Watch("entity_count", (int)world.entities.size());
    }
}
```

`App::Tick` 会自动处理消息循环、帧开始/结束，以及首次启动时的默认停靠布局（左 Inspector / 中 Canvas / 右 Detail / 底部日志组）。布局会通过 `debug_ui.ini` 持久化，用户拖拽调整后下次打开保持不变。

---

## 接入到游戏项目

### Visual Studio 项目引入（推荐）

适合现有 Visual Studio 解决方案，不需要迁移到 CMake。

**第一步：编译出 lib（只需做一次）**

```bat
cd path\to\debug-ui-demo
cmake -B build
cmake --build build --config Release
```

**第二步：导入属性表**

打开你的游戏 `.sln` →  **视图 → 属性管理器** → 展开项目 → 右键 `Release | x64` → **添加现有属性表** → 选择：

```
path\to\debug-ui-demo\dui.props
```

Debug 配置同理再导入一次（`.props` 自动按 `$(Configuration)` 找对应 lib）。

**第三步：代码里直接用**

```cpp
#include "dui_app.h"
#include "dui_world.h"
#include "dui_draw_all.h"

// 全局或管理器成员
dui::App     g_dui;
dui::World   g_dui_world;
dui::Metrics g_dui_metrics;

// 游戏启动时
g_dui.Init(1280, 720, L"Game Debug");

// 每帧游戏 Tick 里
void Game::Tick(float dt) {
    // 把游戏数据填进 World
    g_dui_world.entities.clear();
    for (auto& u : m_units) {
        dui::Entity e{};
        e.id    = u.id;
        e.x     = u.grid_x;  e.y = u.grid_y;
        e.type  = (uint8_t)u.type;
        e.color = u.color;
        e.userdata = &u;     // 在 RegisterEntityDrawer 里 static_cast 回来
        snprintf(e.label, sizeof(e.label), "#%llu", u.id);
        g_dui_world.entities.push_back(e);
    }
    g_dui_world.player_id = m_player_id;

    // 驱动调试窗口（独立 OS 窗口，不干扰游戏窗口）
    g_dui.Tick([&]() {
        dui::DrawAll(g_dui_world, g_dui_metrics);
    });
}
```

> `Tick` 内部只处理 dui 自己窗口的消息，不会干扰游戏的消息循环。调试窗口关掉后 `Tick` 返回 `false`，游戏继续运行不受影响。

**仅在 Debug 版本里开启**

```cpp
#ifdef _DEBUG
    g_dui.Init(1280, 720, L"Game Debug");
    g_dui_enabled = true;
#endif

// 每帧
#ifdef _DEBUG
if (g_dui_enabled)
    g_dui.Tick([&]() { dui::DrawAll(g_dui_world, g_dui_metrics); });
#endif
```

**项目源文件为 GBK 编码时**

`dui.props` 只设置了 `/execution-charset:utf-8`（运行期字符串编码），不强制修改源文件编码，因此 GBK 项目可以直接导入，编译不受影响。

但 ImGui 显示字符串只认 UTF-8，如果你在调用 dui API 时传入了 GBK 字面量，需要先做转换：

```cpp
// 工具函数：GBK → UTF-8
std::string GbkToUtf8(const char* gbk) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, gbk, -1, nullptr, 0);
    std::wstring ws(wlen, 0);
    MultiByteToWideChar(CP_ACP, 0, gbk, -1, ws.data(), wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, ws.data(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), -1, utf8.data(), ulen, nullptr, nullptr);
    return utf8;
}

// 传给 dui 前转换
dui::Log(GbkToUtf8("玩家死亡").c_str());
dui::Watch(GbkToUtf8("当前状态").c_str(), state);
```

也可以只把包含 dui 调用的 `.cpp` 文件单独保存为 UTF-8 编码，其余文件保持 GBK 不动。

---

### CMake 项目引入

```cmake
add_subdirectory(path/to/debug-ui-demo)

target_link_libraries(my_game PRIVATE dui)
target_include_directories(my_game PRIVATE path/to/debug-ui-demo/src)
```

---

## 接入方式

### 独立窗口模式（Init）

dui 自己创建窗口和 D3D11 设备，适合独立调试工具、或嵌入任意引擎的游戏以独立调试窗口形式接入（不依赖游戏的渲染管线）。

```cpp
dui::App app;
app.SetFontPath("path/to/font.ttf", 18.f);  // 可选，加载自定义字体
app.Init(1280, 720, L"Debug UI");

while (app.Tick([&]() {
    dui::DrawAll(world, metrics);
})) {
    // 游戏逻辑
}
```

**字体配置**

Init 模式下会自动加载 `assets/fonts/LXGWWenKai-Regular.ttf`（随 Demo 附带），找不到时回退到 ImGui 内置字体（仅 ASCII，中文显示为方块）。

用 `SetFontPath` 在 `Init` 之前指定字体，可以用项目自带字体或 Windows 系统字体：

```cpp
// 项目自带字体
app.SetFontPath("assets/fonts/MyFont.ttf", 18.f);

// Windows 系统字体（无需附带文件）
app.SetFontPath("C:/Windows/Fonts/msyh.ttc", 18.f);   // 微软雅黑
app.SetFontPath("C:/Windows/Fonts/simsun.ttc", 16.f);  // 宋体

app.Init(1280, 720, L"Debug UI");
```

字符集覆盖常用简体汉字（约 2500 字，`GetGlyphRangesChineseSimplifiedCommon`），满足调试面板日常需求。

**自定义停靠布局**

默认布局在首次启动（无 ini 时）自动应用。如需自定义，在 `Init` 之前调用：

```cpp
#include <imgui_internal.h>

app.SetDockLayoutFn([](ImGuiID dsid) {
    // 用 ImGui::DockBuilder* API 自定义布局
    ImGui::DockBuilderRemoveNode(dsid);
    ImGui::DockBuilderAddNode(dsid, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dsid, ImGui::GetMainViewport()->Size);
    ImGuiID mid = dsid;
    ImGuiID left = ImGui::DockBuilderSplitNode(mid, ImGuiDir_Left, 0.3f, nullptr, &mid);
    ImGui::DockBuilderDockWindow(u8"检视器", left);
    ImGui::DockBuilderDockWindow(u8"场景视图", mid);
    ImGui::DockBuilderFinish(dsid);
});

// 传空 lambda 可完全禁用自动布局
app.SetDockLayoutFn([](ImGuiID) {});
```

#### 在 MFC / 其他引擎里调用

在 `OnIdle` 或定时器回调里同步数据并驱动 dui：

```cpp
// OnIdle / WM_TIMER / 游戏帧回调：
void CMyApp::OnGameTick(float dt) {
    // 同步游戏数据到 World ...

    m_dui_app.Tick([&]() {
        dui::DrawAll(m_dui_world, m_dui_metrics);
    });
}
```

### 注入式模式（Attach）

dui 附着到游戏已有的 D3D11 上下文，渲染叠加在游戏画面上，适合需要与游戏界面融合的场景。

```cpp
// 游戏已有：HWND hwnd, ID3D11Device* dev, ID3D11DeviceContext* ctx, IDXGISwapChain* sc
dui::App dui_app;
dui_app.SetFontPath("C:/Windows/Fonts/msyh.ttc", 18.f);  // 需要中文时必须在 Attach 前设置
dui_app.Attach(hwnd, dev, ctx, sc);

// 在游戏渲染阶段每帧调用（Present 之前）：
dui_app.BeginFrame();
dui::DrawAll(world, metrics);
dui_app.EndFrame();   // 渲染 ImGui；不调用 Present，由游戏自己 Present

// 窗口大小变化后：
dui_app.RebuildRenderTarget();
```

**注意**：注入模式下，游戏的 WndProc 需要转发 ImGui 消息：

```cpp
#include <imgui_impl_win32.h>
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return TRUE;
    // ... 游戏原有处理 ...
}
```

---

## 数据结构

### World

所有面板共享同一个 `World` 对象。游戏侧负责填充并在每帧 tick 后更新它。

```cpp
struct World {
    std::vector<Entity>   entities;
    std::vector<Cell>     cells;         // 地图格子层（在 Entity 下面渲染）
    uint32_t active_map_id = 0;          // 当前活动地图（多地图场景用）
    uint64_t selected_id   = 0;          // 主选中实体 id（0 = 无）
    uint64_t player_id     = 0;          // 主角实体 id，用于相机跟随
    bool     sel_cell_valid = false;
    int      sel_cell_x     = 0;
    int      sel_cell_y     = 0;
    std::vector<uint64_t> selected_ids;  // 多选集合（始终包含 selected_id）
};
```

### Entity

```cpp
struct Entity {
    uint64_t id;
    uint32_t map_id;             // 所属地图
    int      x, y;               // 网格整数坐标（驱动渲染 / 点击检测）
    float    fx, fy;             // 浮点坐标（SetPos 同步 x/y = roundf(fx/fy)）
    float    vx, vy;             // 速度（网格单位/秒）
    float    radius;             // 视觉填充比例 0..1
    uint32_t color;              // ABGR 格式，可用 IM_COL32 / RGBA 生成
    uint8_t  type;               // 调用方定义的类型标签
    char     label[16];          // 显示名称
    void*    userdata = nullptr; // 指向游戏自有结构，库不解读也不释放
};
```

`Entity` 支持链式 setter：`e.SetPos(fx, fy).SetVel(vx, vy).SetColor(r,g,b).SetType(1).SetLabel("#%d", id)`。

### Cell

```cpp
struct Cell {
    int      x, y;
    uint32_t color;
    uint8_t  type;
    char     label[12];
    void*    userdata = nullptr;
};
```

`userdata` 供[自定义字段扩展点](#entitycell-自定义字段)使用，把游戏侧的 `MyUnit*` / `MyTile*` 塞进来即可。

---

## 面板一览

### 场景视图 DrawCanvas

等距 45° 投影的 2D 地图视图。

```cpp
dui::DrawCanvas(world);

// 或传入外部 CanvasView 来持久化相机状态：
dui::CanvasView view;
dui::DrawCanvas(world, &view);
```

**交互：**

| 操作 | 效果 |
|------|------|
| 左键单击实体 | 选中（相机不动） |
| 左键双击实体 | 选中并把相机跳到该实体位置 |
| 左键点击格子 | 选中格子（Inspector 显示详情） |
| Shift/Ctrl+左键拖拽 | 框选多个实体 |
| Ctrl+左键点实体 | 多选切换 |
| 滚轮 | 缩放（以鼠标位置为锚点） |
| 中键拖拽 | 平移相机（自动退出跟随模式） |
| 右键拖拽 | 平移相机（自由模式下） |
| 右键单击 | 上下文菜单（实体/格子/背景） |
| Toolbar → Fit | 自动缩放居中显示当前地图所有内容 |
| Toolbar → 跟随/自由 | 切换相机跟随模式（默认"自由"） |

**工具栏第一行** 6 个显示开关（格线/格子/实体/标签/坐标轴/轨迹）—— 连线和热力图的批量开关已迁移到「图层」面板，避免与逐项开关重叠。

### 检视器 DrawInspector

```cpp
dui::DrawInspector(world);
```

搜索框 + 类型过滤 + 实体列表 + 可编辑字段 + 格子列表。

### 实体详情 DrawEntityDetail

```cpp
#include "dui_detail.h"

dui::RegisterEntityDetailText(/*type=*/1, [](const dui::Entity& e) -> std::string {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "=== 基础 ===\n"
        "ID    : %llu\n"
        "Pos   : (%.2f, %.2f)\n",
        (unsigned long long)e.id, e.fx, e.fy);
    return buf;
});

dui::DrawEntityDetail(world);
```

### Metrics DrawMetrics

```cpp
dui::Metrics metrics;
metrics.tick_ms.push(tick_duration_ms);
metrics.entity_count.push((float)world.entities.size());

dui::DrawMetrics(metrics);
```

显示 tick 耗时和实体数量的滚动折线图（最近 300 帧）。

### 日志 DrawLog

```cpp
dui::Log(u8"tick #%d", n);
dui::LogWarn(u8"警告: %s", msg);
dui::LogError(u8"错误: code=%d", code);

dui::DrawLog();
```

最多保留 1000 条，支持 Info/Warn/Error 过滤和文本搜索。

### 监视 DrawWatch

```cpp
dui::Watch(u8"实体数", (int)world.entities.size());
dui::Watch(u8"player_x", world.entities[0].x);

dui::DrawWatch();
```

同名调用覆盖上一次的值，以双列表格实时显示。支持 `int / float / bool / const char*`。

### 命令面板 DrawCommands

```cpp
dui::DrawCommands();
```

显示所有注册命令，支持搜索过滤、按类别折叠。每条命令右侧的 `[K]` 按钮可绑定快捷键。见[命令注册](#命令注册)。

### 其余内置面板

`DrawAll` 还会调用如下面板（按需可单独调用）：

| 面板 | API | 用途 |
|---|---|---|
| 事件 | `DrawEvents()` | 时间线事件流（带分类/严重级颜色） |
| Profiler | `DrawProfiler()` | 帧火焰图 + 均值表，用 `DUI_PROFILE_SCOPE("name")` 插桩 |
| 回放 | `DrawReplayPanel(world)` | World 录制 + 时间倒带（见 Round 6） |
| 小地图 | `DrawMinimap(world)` | 当前地图俯视图，点击跳镜 |
| 图层 | `DrawLayerPanel(world)` | 运行时开关所有 overlay / heatmap / link |
| 全局搜索 | `DrawGlobalSearch(world)` | Ctrl+F / Ctrl+P 统一搜实体/命令/格子/标注/书签 |

> 完整 API 速查见 [`docs/api-reference.md`](docs/api-reference.md)。

---

## 调试深水区（Round 6 / Round 7）

> 详细 API 见 [`docs/api-reference.md`](docs/api-reference.md) 相应章节。

### 时间控制 — Pause / Step / Speed

```cpp
#include "dui_time.h"

float eff = dui::EffectiveDt(dt);   // 暂停→0；步进→raw_dt 一次；否则 dt*scale
TickMockWorld(world, eff);          // 把 eff 传给你的世界 tick

dui::SetWorldPaused(true);          // 也可命令式控制
dui::SetTimeScale(2.f);             // 0.0625–8.0
dui::RequestSingleStep();
```

热键（Demo 已绑定）：`Space` 暂停 / `.` 单帧 / `[` `]` 速度 ÷2/×2。

### Replay 录制 + 时间倒带

```cpp
#include "dui_replay.h"

dui::EnableReplayRecording(true);   // 默认关闭，按 F8 切换
dui::SetReplayBufferSize(600);      // 默认 600 帧（≈10s @ 60fps）
// 进入回放后画布自动暂停 + 禁交互；回放面板提供时间轴和播放按钮
```

启用后每帧捕获完整 World 快照到环形缓冲。打开「回放」面板拖动时间轴即可倒带。

### 实体行为日志

```cpp
#include "dui_entity_log.h"

dui::LogEntity     (e.id, u8"切换目标 #%d", target_id);
dui::LogEntityWarn (e.id, u8"路径失败");
dui::LogEntityError(e.id, u8"碰撞异常");
// 实体详情面板自动显示该实体的最近日志（默认 32 条）
// DespawnEntity 销毁实体时自动清理对应日志
```

### 选择组 Ctrl+1..9

```cpp
#include "dui_select_group.h"

dui::SaveSelectionGroup  (world, /*slot=*/1);  // 保存当前多选到槽 1
dui::RecallSelectionGroup(world, 1);           // 恢复
```

Demo 已绑定 `Ctrl+Shift+1..9` 保存、`Ctrl+1..9` 恢复。

### 图层开关

`DrawLayerPanel` 把所有已注册的 `RegisterEntityOverlay` / `RegisterCellOverlay` / `RegisterCellHeatmap` / `RegisterEntityLinks` / `RegisterCellLinks` / `RegisterGlobalOverlay` 列成可勾选的复选框。热力图组和连线组带有"整体批量开关"，避免和场景视图工具栏功能重复。

```cpp
// 用代码控制图层开关（同样改变面板里的勾选状态）
dui::SetLayerEnabled("Heatmap", u8"经过频率", false);

// 枚举当前注册的图层
std::vector<dui::LayerInfo> layers;
dui::ListLayers(layers);
```

### Inspector 过滤器收藏

在检视器实体列表工具栏点击 `★` 可把当前的搜索/类型/排序/分组组合存为命名预设，下次启动自动加载（`dui_inspector_presets.ini`）。

---

## 扩展点

### Entity / Cell 自定义字段

在 Inspector 的实体/格子详情区追加任意 ImGui 控件，无需修改库代码。

```cpp
#include "dui_ext.h"
#include <imgui.h>

dui::RegisterEntityDrawer(/*type=*/1, [](dui::Entity& e) {
    auto* unit = static_cast<MyUnit*>(e.userdata);
    ImGui::Text("HP: %d / %d", unit->hp, unit->max_hp);
    ImGui::ProgressBar((float)unit->hp / unit->max_hp);
    ImGui::SliderFloat("speed", &unit->speed, 0.f, 10.f);
});

dui::RegisterCellDrawer(/*type=*/2, [](dui::Cell& c) {
    auto* tile = static_cast<MyTile*>(c.userdata);
    ImGui::Text("passable: %s", tile->passable ? "yes" : "no");
});
```

**类型名注册**（Inspector / Canvas 里显示中文名而非数字）：

```cpp
dui::RegisterEntityTypeName(0, u8"玩家");
dui::RegisterEntityTypeName(1, u8"战士");
dui::RegisterCellTypeName(1,   u8"墙壁");
```

**Canvas 标记**（在指定实体上方显示彩色三角）：

```cpp
dui::SetPlayerEntityType(0);                              // type-0 全部显示黄色三角
dui::SetEntityMarker(1002, IM_COL32(255, 80, 80, 230));  // entity #1002 显示红色三角
dui::ClearEntityMarker(1002);                             // 取消
```

### 命令注册

```cpp
#include "dui_commands.h"

// 名称格式 "Category/Name" — 面板里按 "/" 前的部分分组
dui::RegisterCommand(u8"World/重置世界", [&world]() {
    world = dui::MakeMockWorld();
    dui::Log(u8"world reset");
});

dui::RegisterCommand(u8"Player/传送到原点", [&world]() {
    if (!world.entities.empty())
        world.entities[0].SetPos(0.f, 0.f).SetVel(0.f, 0.f);  // 链式 setter 自动同步 x/y
});

dui::UnregisterCommand(u8"World/重置世界");  // 取消注册

// 带参数命令：弹出模态表单收集参数
dui::RegisterCommandWithArgs(u8"Player/传送到坐标", {
    dui::CommandArg::Int("X", 0, -50.f, 50.f),
    dui::CommandArg::Int("Y", 0, -50.f, 50.f),
}, [&world](const dui::CommandArgValue* v, int /*n*/) {
    if (!world.entities.empty())
        world.entities[0].SetPos((float)v[0].i, (float)v[1].i).SetVel(0.f, 0.f);
});
```

---

## 构建 Demo

**依赖**（放在 `third_party/`）

- [Dear ImGui](https://github.com/ocornut/imgui) — docking 分支
- [ImPlot](https://github.com/epezent/implot)
- MSVC / Visual Studio，Windows 平台，D3D11 后端

```bat
cmake -S . -B build
cmake --build build --config Release
```

**指定 Visual Studio 版本**

默认使用系统最新的 VS。如需指定版本，用 `-G` 传入生成器名称：

```bat
cmake -G "Visual Studio 15 2017" -A x64 -B build
cmake --build build --config Release
```

| VS 版本 | 生成器名称 |
|---------|-----------|
| VS 2017 | `Visual Studio 15 2017` |
| VS 2019 | `Visual Studio 16 2019` |
| VS 2022 | `Visual Studio 17 2022` |

`-A x64` 指定 64 位目标平台，建议始终带上。如需进一步锁定 MSVC 工具集版本，追加 `-T v141`（VS2017 工具集）。

产物：
- `build/Release/dui.lib` — 核心库
- `build/Release/dui_demo.exe` — 演示程序

运行演示程序需要 `assets/fonts/LXGWWenKai-Regular.ttf`（CMake 构建后自动拷贝到 exe 旁）。字体缺失时自动回退到 ImGui 内置字体。

---

## 目录结构

```
src/
  dui_app.h / .cpp           App 生命周期（Init / Attach / Tick / SetDockLayoutFn）
  dui_world.h                Entity / Cell / World 数据结构
  dui_draw_all.h / .cpp      DrawAll — 一次调用绘制所有面板
  dui_canvas.h / .cpp        场景视图 + CanvasView + 摄像机书签
  dui_inspector.h / .cpp     检视器面板 + 过滤器预设
  dui_metrics.h / .cpp       性能图表面板
  dui_log.h / .cpp           日志面板 + Log* API
  dui_ext.h / .cpp           扩展点（类型名 / Canvas Overlay / Heatmap / Links / 标记 / 多选 / 图层枚举）
  dui_detail.h / .cpp        实体详情面板 + DetailBuilder
  dui_commands.h / .cpp      命令面板 + 命令注册 + 参数化命令
  dui_hotkeys.h / .cpp       热键绑定与持久化
  dui_menubar.h / .cpp       菜单栏 + 面板可见性管理
  dui_events.h / .cpp        事件时间线面板
  dui_search.h / .cpp        Ctrl+F / Ctrl+P 全局搜索
  dui_pins.h / .cpp          世界标注 Pin
  dui_trails.h / .cpp        实体轨迹 + 格子热度
  dui_snapshot.h / .cpp      World JSON 快照 (Save/Load)
  dui_profiler.h / .cpp      帧火焰图（DUI_PROFILE_SCOPE）
  dui_time.h / .cpp          世界暂停 / 单帧 / 速度倍率
  dui_replay.h / .cpp        Replay 录制 + 时间倒带
  dui_entity_log.h / .cpp    每实体行为日志通道
  dui_minimap.h / .cpp       小地图面板
  dui_layers.h / .cpp        图层开关面板
  dui_select_group.h / .cpp  选择组 (Ctrl+1..9)
  dui_user_metrics.h / .cpp  自定义指标曲线（ConfigureMetric / Push）
  dui_mock.h / .cpp          演示用 MakeMockWorld / TickMockWorld（不在库里）
  dui_demo_setup.h / .cpp    演示用注册代码（不在库里）
  main.cpp                   演示程序入口
docs/
  api-reference.md           完整 API 速查
third_party/
  imgui/                     Dear ImGui（docking 分支）
  implot/                    ImPlot
tests/
  test_ring_buffer.cpp       RingBuffer 单元测试
assets/
  fonts/                     LXGWWenKai-Regular.ttf
dui.props                    Visual Studio 属性表，导入即可配好所有 include / lib 路径
```

**运行期生成的持久化文件**（在 exe 所在目录）：

| 文件 | 内容 |
|---|---|
| `debug_ui.ini` | ImGui 窗口位置/停靠布局 |
| `dui_hotkeys.ini` | 热键绑定 |
| `dui_bookmarks.ini` | 摄像机书签 |
| `dui_inspector_presets.ini` | Inspector 过滤器预设 |
