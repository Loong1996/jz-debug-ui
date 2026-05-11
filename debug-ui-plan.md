# Windows C++ 服务端调试 UI 方案

## 背景

- 项目类型:Windows C++ 服务端
- 需求:在服务端进程内嵌一个调试界面,用于模拟客户端的简单 2D 界面
- 用途:
  - 实时查看服务端内部状态(玩家/实体属性、队列、计数器等)
  - 2D 可视化场景(地图、实体位置、碰撞框、连线等)
  - 实时曲线(tick 耗时、消息量、网络流量等)
  - 调试操作面板(开关、参数滑条、触发按钮)
- **运行模式:单线程**——业务逻辑和 UI 在同一个主循环里跑

## 技术选型

### 核心组合

| 组件 | 作用 | 来源 |
|---|---|---|
| **Dear ImGui** | 立即模式 GUI 框架,提供控件 + 2D DrawList | https://github.com/ocornut/imgui |
| **ImPlot** | ImGui 的图表扩展,折线/散点/热图/柱状图 | https://github.com/epitec/implot |
| **Win32 + DirectX 11 后端** | 窗口和渲染外壳 | ImGui 官方 examples |

### 为什么是这套

- 服务端没有现成渲染循环 → 用 ImGui 官方 Win32+DX11 示例当外壳,几十行模板代码
- 2D 绘图需求中等 → `ImGui::GetWindowDrawList()` 提供 Canvas 风格 API(AddLine/AddRect/AddCircle/AddImage 等),够画地图+实体
- 实时曲线 → ImPlot 直接 `PlotLine` 一行搞定
- 控件 + 绘图能混在同一窗口 → 这是 ImGui 相对网页方案(WebView2)的最大优势
- 无外部依赖,源码直接进项目,编译快,改起来方便
- 立即模式天然适合单线程:每帧直接读业务数据画出来,不用任何同步机制

### 备选方案(暂不采用)

- **WebView2 + HTML/JS**:UI 灵活但要起通信通道,对调试小工具偏重
- **Qt / wxWidgets**:功能完整但编译时间和依赖体积不划算

## 整体架构(单线程)

```
┌─────────────────────────────────────────────────┐
│ 服务端进程 - 单线程主循环                          │
│                                                  │
│  while (running) {                              │
│    ┌──────────────────────────────────────┐    │
│    │ 1. PumpWin32Messages()  (非阻塞)      │    │
│    └──────────────────────────────────────┘    │
│    ┌──────────────────────────────────────┐    │
│    │ 2. NetworkPoll()         网络收包     │    │
│    └──────────────────────────────────────┘    │
│    ┌──────────────────────────────────────┐    │
│    │ 3. GameTick(dt)          业务逻辑     │    │
│    └──────────────────────────────────────┘    │
│    ┌──────────────────────────────────────┐    │
│    │ 4. ImGui::NewFrame()                  │    │
│    │    DrawInspector()                    │    │
│    │    DrawCanvas()    ←直接读 world 数据 │    │
│    │    DrawMetrics()                      │    │
│    │    ImGui::Render() + DX11 Present     │    │
│    └──────────────────────────────────────┘    │
│    ┌──────────────────────────────────────┐    │
│    │ 5. SleepUntilNextFrame() 帧率控制     │    │
│    └──────────────────────────────────────┘    │
│  }                                              │
└─────────────────────────────────────────────────┘
```

### 关键点

- **没有线程同步**:UI 代码直接读 `world.entities` 这种业务数据,因为它和业务在同一个线程
- **没有快照拷贝**:省掉双缓冲、mutex、atomic 这些
- **UI 反向控制零成本**:按钮点击的回调直接修改业务对象,不需要消息队列
- **业务和 UI 共享一个时钟节奏**

### 帧率策略

两种思路,选一种:

**A. 业务和 UI 同频(简单)**
- 整个 loop 跑 60Hz,业务 tick 也是 60Hz
- 适合业务本身不需要更高频率的情况
- 直接 `Sleep` 或者用 `WaitForSingleObject` 凑帧间隔

**B. 业务高频 + UI 低频(更通用)**
- 业务按需要的频率 tick(比如 100Hz,或按事件驱动)
- UI 用一个计时器,每 ~16ms 才画一帧,其他时候跳过 ImGui 那段
- 网络收包始终调,保持响应

```cpp
auto last_ui = Clock::now();
while (running) {
    PumpWin32Messages();
    NetworkPoll();
    GameTick(dt);

    auto now = Clock::now();
    if (now - last_ui >= 16ms) {
        RenderDebugUI();   // ImGui NewFrame ... Render
        last_ui = now;
    }
    SleepUntilNextTick();
}
```

## 界面布局

三栏式:

```
┌─────────────────────────────────────────────────────────────┐
│ [菜单栏: File / View / Debug]                                │
├──────────────┬────────────────────────────┬─────────────────┤
│ 左侧属性面板  │ 中间 2D 场景画布            │ 右侧实时曲线     │
│              │                            │                 │
│ ▼ Players    │   ┌─────────────────┐     │ tick 耗时 (ms)  │
│   ▶ #1001    │   │  ●     ●        │     │ ╱╲    ╱╲       │
│   ▶ #1002    │   │     ●    ●  ●   │     │╱  ╲__╱  ╲___   │
│   ▶ #1003    │   │  ●       ●      │     │                 │
│ ▼ NPCs       │   │     ●           │     │ 消息队列长度     │
│   ▶ Boss#1   │   └─────────────────┘     │ ▁▂▃▅▇▅▃▂▁     │
│ ▼ Config     │   pan: 右键拖动             │                 │
│   slider...  │   zoom: 滚轮               │ 在线人数         │
│   toggle...  │   选中实体: 左键            │ ___／￣￣        │
└──────────────┴────────────────────────────┴─────────────────┘
```

### 左侧:属性面板

- `ImGui::Begin("Inspector")`
- 用 `TreeNode` 分组:Players / NPCs / Items / Config
- 每个实体展开后用 `Text` / `InputFloat` / `Checkbox` 显示属性
- 选中某个实体时,在中间画布高亮
- **可以直接修改业务对象**:`ImGui::SliderFloat("speed", &entity.speed, ...)` 直接绑业务变量(单线程下安全)

### 中间:2D 场景画布

- `ImGui::BeginChild("Canvas", ...)` 包一个子窗口
- 用 `GetWindowDrawList()` 拿到 DrawList,直接画
- 自己实现 `WorldToScreen(world_pos) -> ImVec2` 做坐标变换
- 鼠标交互:
  - 右键拖动 → 平移 camera
  - 滚轮 → 缩放
  - 左键点击 → 拾取实体(遍历实体做命中检测)
- 画的内容举例:
  - 网格背景(`AddLine` 循环)
  - 实体点(`AddCircleFilled`)
  - 碰撞体(`AddRect` / `AddCircle`)
  - 视野/范围(`AddCircle` 半透明)
  - 选中高亮(粗边框)
  - 实体 ID 文字(`AddText`)

### 右侧:实时曲线

- `ImGui::Begin("Metrics")`
- ImPlot 滚动缓冲区:维护几个固定长度的环形数组,业务 tick 完往里 push,UI 帧用 `PlotLine` 读
- 典型曲线:
  - tick 耗时
  - 每秒处理消息数
  - 在线人数
  - 内存占用

## 关键代码骨架(伪代码)

```cpp
// === 业务数据(直接用,不需要拷贝) ===
struct Entity {
    uint64_t id;
    float x, y;
    float radius;
    uint32_t color;
    // ... 业务字段
};

struct World {
    std::vector<Entity> entities;
    // ...
};

World g_world;

// === Metrics 环形缓冲(给曲线用) ===
struct Metrics {
    RingBuffer<float> tick_ms{300};      // 5 秒 @ 60Hz
    RingBuffer<float> msg_per_sec{300};
    RingBuffer<int>   online_count{300};
} g_metrics;

// === 主循环 ===
int WinMain(...) {
    // 1. 创建 Win32 窗口 + DX11 设备(抄 example_win32_directx11)
    InitWindow();
    InitD3D11();

    // 2. 初始化 ImGui / ImPlot
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, ctx);

    // 3. 业务初始化
    InitServer();

    Camera cam{ .offset = {0,0}, .zoom = 1.0f };
    auto last_ui = Clock::now();

    while (!g_quit) {
        // --- Win32 消息泵 ---
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_quit = true;
        }

        // --- 业务 ---
        auto t0 = Clock::now();
        NetworkPoll();
        GameTick(dt);
        auto tick_ms = duration_ms(Clock::now() - t0);
        g_metrics.tick_ms.push(tick_ms);

        // --- UI(限频) ---
        auto now = Clock::now();
        if (now - last_ui >= 16ms) {
            last_ui = now;

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            DrawInspector(g_world);
            DrawCanvas(g_world, cam);
            DrawMetrics(g_metrics);

            ImGui::Render();
            d3d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
            d3d_ctx->ClearRenderTargetView(rtv, clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            swapchain->Present(1, 0);
        }

        SleepUntilNextTick();
    }

    // cleanup ...
}

// === 2D 画布核心 ===
void DrawCanvas(const World& world, Camera& cam) {
    ImGui::Begin("Scene");
    ImGui::BeginChild("canvas", ImVec2(0,0), true);

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x+canvas_sz.x, canvas_p0.y+canvas_sz.y);

    HandleCameraInput(cam, canvas_p0, canvas_sz);

    auto* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvas_p0, canvas_p1, true);

    DrawGrid(dl, canvas_p0, canvas_sz, cam);

    for (auto& e : world.entities) {
        ImVec2 sp = WorldToScreen({e.x, e.y}, cam, canvas_p0, canvas_sz);
        dl->AddCircleFilled(sp, e.radius * cam.zoom, e.color);
        dl->AddText(sp, IM_COL32_WHITE, std::to_string(e.id).c_str());
    }

    dl->PopClipRect();
    ImGui::EndChild();
    ImGui::End();
}
```

## 实施步骤

1. **环境准备**
   - 把 imgui 仓库的核心源码拖进项目:`imgui.cpp` / `imgui_draw.cpp` / `imgui_tables.cpp` / `imgui_widgets.cpp` / `imgui_demo.cpp` + `imgui_impl_win32.cpp` / `imgui_impl_dx11.cpp` 以及对应头文件
   - 拖入 implot:`implot.cpp` / `implot_items.cpp` + 头文件
   - 链接 `d3d11.lib` / `d3dcompiler.lib`

2. **跑通最小窗口**
   - 直接复制 `examples/example_win32_directx11/main.cpp`
   - 确认能显示 ImGui demo 窗口和 ImPlot demo 窗口

3. **改造成"业务 + UI 单循环"**
   - 把 example 里的 main loop 改造成包含业务 tick 的结构
   - 加帧率控制(选同频或限频方案)

4. **接入业务数据**
   - UI 函数签名直接传业务对象引用
   - 先用 `ImGui::Text` 简单显示几个字段验证

5. **画 2D 场景**
   - 实现 Camera + WorldToScreen
   - 画背景网格 → 画实体点 → 加 pan/zoom 交互
   - 加选中高亮和 tooltip

6. **加曲线**
   - 实现 RingBuffer
   - 业务 tick 完往里 push 数据
   - ImPlot 画 2-3 条最关心的曲线

7. **完善**
   - 持久化窗口布局(ImGui 自带 ini)
   - 加暂停/继续、单步执行(暂停业务 tick,UI 继续刷)
   - 视情况加录制/回放

## 单线程要注意的点

- **UI 卡 = 业务卡**:画面板时不要做重活(比如全量遍历 100 万实体格式化字符串),要么分页要么用虚拟列表(ImGui 的 `ListClipper`)
- **业务卡 = UI 卡**:业务里别有阻塞调用(同步 IO、Sleep),否则窗口会假死。网络用非阻塞 socket 或者 IOCP/select poll 模式
- **窗口拖动时**:Win32 在用户拖动窗口标题栏时会进入"模态消息循环",主循环暂停。如果业务必须不停,要么用 `WM_ENTERSIZEMOVE` 起个定时器,要么干脆接受拖窗时业务暂停(调试场景一般可以接受)
- **Release 剔除**:用宏 `#ifdef ENABLE_DEBUG_UI` 把整段 UI 代码包起来,正式版本编出来零开销

## 待讨论/待确认

- [ ] 业务循环目前是什么形态?事件驱动 / 固定 tick / 异步?决定怎么塞进单循环
- [ ] 业务 tick 频率?(影响帧率策略选 A 还是 B)
- [ ] 实体规模?(决定 UI 里能不能直接遍历全量画出来)
- [ ] 网络层是否阻塞?如果是同步 socket,改造成 select/非阻塞才能塞进单循环
- [ ] 编译期开关:Release 完全剔除调试 UI?

## 参考资料

- Dear ImGui: https://github.com/ocornut/imgui
- ImGui Wiki(Getting Started): https://github.com/ocornut/imgui/wiki
- ImPlot: https://github.com/epitec/implot
- ImGui 官方示例目录: https://github.com/ocornut/imgui/tree/master/examples
- Win32 + DX11 后端入口: `examples/example_win32_directx11/main.cpp`
