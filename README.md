# dui — 游戏调试 UI 库

基于 Dear ImGui + ImPlot + DirectX 11 的游戏内调试面板库，支持独立运行和注入式嵌入两种接入方式。

---

## 目录

- [快速开始](#快速开始)
- [接入方式](#接入方式)
  - [独立窗口模式](#独立窗口模式-init)
  - [注入式模式](#注入式模式-attach)
- [数据结构](#数据结构)
- [面板一览](#面板一览)
  - [场景视图 DrawCanvas](#场景视图-drawcanvas)
  - [检视器 DrawInspector](#检视器-drawinspector)
  - [Metrics DrawMetrics](#metrics-drawmetrics)
  - [日志 DrawLog](#日志-drawlog)
  - [监视 DrawWatch](#监视-drawwatch)
  - [命令面板 DrawCommands](#命令面板-drawcommands)
- [扩展点](#扩展点)
  - [Entity / Cell 自定义字段](#entitycell-自定义字段)
  - [命令注册](#命令注册)
- [构建](#构建)
- [目录结构](#目录结构)

---

## 快速开始

```cpp
#include "dui_app.h"
#include "dui_world.h"
#include "dui_canvas.h"
#include "dui_inspector.h"
#include "dui_log.h"
#include "dui_commands.h"

int WINAPI WinMain(...) {
    dui::App app;
    app.Init(1280, 720, L"My Game Debug UI");

    dui::World world = /* 填入你的游戏数据 */;

    while (app.Tick([&]() {
        dui::DrawCanvas(world);
        dui::DrawInspector(world);
        dui::DrawLog();
        dui::DrawWatch();
        dui::DrawCommands();
    })) {
        // 游戏逻辑 tick
        dui::Watch("entity_count", (int)world.entities.size());
    }
}
```

---

## 接入方式

### 独立窗口模式（Init）

dui 自己创建窗口和 D3D11 设备，适合独立调试工具或快速原型。

```cpp
dui::App app;
if (!app.Init(1280, 720, L"Debug UI")) return 1;

// 可选：在 Init 之前调用，加载自定义字体
app.SetFontPath("path/to/font.ttf", 18.f);

while (app.Tick([&]() {
    // 在这里调用各 Draw* 函数
})) {
    // 游戏逻辑
}
```

`Tick` 内部按顺序执行：`PumpMessages → BeginFrame → draw_fn → EndFrame + Present`。

### 注入式模式（Attach）

dui 附着到游戏已有的 D3D11 上下文，适合集成进正在运行的游戏。

```cpp
// 游戏已有：HWND hwnd, ID3D11Device* dev, ID3D11DeviceContext* ctx, IDXGISwapChain* sc

dui::App dui_app;
dui_app.Attach(hwnd, dev, ctx, sc);

// 在游戏主循环的渲染阶段每帧调用：
dui_app.BeginFrame();
dui::DrawCanvas(world);
dui::DrawLog();
// ... 其他面板 ...
dui_app.EndFrame();   // 渲染 ImGui；不调用 Present，由游戏自己 Present

// 窗口大小变化后重建 RTV：
dui_app.RebuildRenderTarget();
```

**注意**：在注入模式下，游戏的 WndProc 需要转发 ImGui 消息：

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
    std::vector<Entity> entities;
    std::vector<Cell>   cells;        // 地图格子层（在 Entity 下面渲染）
    int  selected_id    = -1;         // 当前选中的实体 id，-1 表示无
    int  player_id      = -1;         // 主角实体 id，用于相机跟随
    bool sel_cell_valid = false;
    int  sel_cell_x     = 0;
    int  sel_cell_y     = 0;
};
```

### Entity

```cpp
struct Entity {
    uint64_t id;
    int      x, y;              // 网格整数坐标（驱动渲染）
    float    fx, fy;            // 浮点坐标累加器（用 roundf 驱动 x/y）
    float    vx, vy;            // 速度（网格单位/秒）
    float    radius;            // 视觉填充比例 0..1
    uint32_t color;             // ABGR 格式，可用 IM_COL32 生成
    uint8_t  type;              // 调用方定义的类型标签
    char     label[16];         // 显示名称
    void*    userdata = nullptr; // 指向游戏自有结构，库不解读也不释放
};
```

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

`userdata` 供 [自定义字段扩展点](#entitycell-自定义字段) 使用，把游戏侧的 `MyUnit*` / `MyTile*` 塞进来即可。

---

## 面板一览

### 场景视图 DrawCanvas

等距 45° 投影的 2D 地图视图，以当前相机位置为中心渲染。

```cpp
dui::DrawCanvas(world);

// 或传入外部 CanvasView 来持久化相机状态：
dui::CanvasView view;
dui::DrawCanvas(world, &view);
```

**Toolbar 功能：**

| 控件 | 说明 |
|------|------|
| 格线 / 格子 / 实体 / 标签 / 坐标轴 | 图层开关 |
| 跟随 / 自由 | 相机模式切换 |
| 重置 | 相机跳回主角位置 |

**交互：**
- 左键点击实体 → 选中并切换到跟随模式
- 左键点击格子 → 选中格子（Inspector 下方显示详情）
- 左键拖拽空白区域 → 自由平移相机
- 右键拖拽 → 自由平移相机

**橙色边框**：始终锚定在被跟随实体的 37×37 格视野范围上，与相机是否自由平移无关。

### 检视器 DrawInspector

```cpp
dui::DrawInspector(world);
```

- 顶部搜索框（按 `label` 子串过滤）+ 类型下拉（按 `type` 过滤）
- 实体列表：点击选中/取消；选中后下方展示 x/y/radius 可编辑字段及[自定义扩展字段](#entitycell-自定义字段)
- 地图格子折叠列表：点击选中，在场景视图上高亮金框

### Metrics DrawMetrics

```cpp
dui::Metrics metrics;
// 每帧更新：
metrics.tick_ms.push(tick_duration_ms);
metrics.entity_count.push((float)world.entities.size());

dui::DrawMetrics(metrics);
```

显示 tick 耗时和实体数量的滚动折线图（ImPlot 实现，最近 300 帧）。

### 日志 DrawLog

```cpp
// 任意位置推送消息：
dui::Log(u8"tick #%d", n);
dui::LogWarn(u8"警告: %s", msg);
dui::LogError(u8"错误: code=%d", code);

// 在 Tick 回调里绘制面板：
dui::DrawLog();
```

- 最多保留 1000 条（环形缓冲）
- 按 Info / Warn / Error 着色，支持 checkbox 过滤和文本子串搜索
- 自动滚到底部（用户上滚查阅时不会被打断）

### 监视 DrawWatch

```cpp
// 任意位置更新 key-value：
dui::Watch(u8"实体数", (int)world.entities.size());
dui::Watch(u8"player_x", world.entities[0].x);
dui::Watch("speed", velocity);   // 支持 int / float / bool / const char*

dui::DrawWatch();
```

同名调用覆盖上一次的值，以双列表格实时显示。

### 命令面板 DrawCommands

```cpp
dui::DrawCommands();
```

显示所有注册命令，支持搜索过滤、按类别折叠。见[命令注册](#命令注册)。

---

## 扩展点

### Entity / Cell 自定义字段

在检视器的实体/格子详情区追加任意 ImGui 控件，无需修改库代码。

```cpp
#include "dui_ext.h"
#include <imgui.h>

// 在程序启动时注册（覆盖同 type 的上一次注册）
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

**生命周期**：`userdata` 指针由游戏管理，库只在 drawer 回调里使用它，不持有所有权。确保实体/格子在 `world.entities` / `world.cells` vector 里时指针不悬空。

### 命令注册

```cpp
#include "dui_commands.h"

// 名称格式 "Category/Name" — 面板里按 "/" 前的部分分组
dui::RegisterCommand(u8"World/重置世界", [&world]() {
    world = dui::MakeMockWorld();
    dui::Log(u8"world reset");
});

dui::RegisterCommand(u8"Player/传送到原点", [&world]() {
    if (!world.entities.empty()) {
        world.entities[0].x = world.entities[0].y = 0;
        world.entities[0].fx = world.entities[0].fy = 0.f;
    }
});

// 无 "/" 的命令归入"通用"分组
dui::RegisterCommand(u8"截图", []() { /* ... */ });

// 取消注册
dui::UnregisterCommand(u8"World/重置世界");
```

面板交互：点击「执行」按钮或双击命令行触发。

---

## 构建

**依赖**（均以 git submodule 形式放在 `third_party/`）

- [Dear ImGui](https://github.com/ocornut/imgui) — docking 分支或 master
- [ImPlot](https://github.com/epezent/implot)
- MSVC / Visual Studio（Windows 平台，使用 D3D11 后端）

**构建步骤**

```bat
cmake -S . -B build
cmake --build build --config Debug
```

产物：
- `build/Debug/dui.lib` — 核心库（可被其他 exe 链接）
- `build/Debug/dui_demo.exe` — 演示程序

运行演示程序需要 `assets/fonts/LXGWWenKai-Regular.ttf`（CMake 会自动拷贝到输出目录旁）。若字体文件缺失，自动回退到 ImGui 内置字体（不含中文字形）。

**链接到自己的项目**

```cmake
add_subdirectory(path/to/dui)

target_link_libraries(my_game PRIVATE dui)
target_include_directories(my_game PRIVATE path/to/dui/src)
```

---

## 目录结构

```
src/
  dui_app.h / .cpp        App 生命周期（Init / Attach / Tick）
  dui_world.h             Entity / Cell / World 数据结构
  dui_mock.h / .cpp       演示用 MakeMockWorld / TickMockWorld（不在库里）
  dui_canvas.h / .cpp     场景视图面板
  dui_inspector.h / .cpp  检视器面板
  dui_metrics.h / .cpp    性能图表面板
  dui_log.h / .cpp        日志面板 + Log* API
  dui_ext.h / .cpp        Entity / Cell 自定义字段扩展点
  dui_commands.h / .cpp   命令面板 + RegisterCommand API
  main.cpp                演示程序入口
third_party/
  imgui/                  Dear ImGui
  implot/                 ImPlot
tests/
  test_ring_buffer.cpp    RingBuffer 单元测试
assets/
  fonts/                  LXGWWenKai-Regular.ttf
```
