# Debug UI Framework Design

**Date:** 2026-05-11  
**Standard:** C++11/14  
**Build:** CMake  
**Dependencies:** ImGui + ImPlot (source copied into `third_party/`)

---

## Overview

A single-process, single-thread debug UI for Windows C++ servers. Embeds a Dear ImGui window (Win32 + DirectX 11 backend) into the server's main loop. UI draws directly from live business data — no snapshots, no mutexes, no message queues.

All public symbols live in the `dui` namespace. All source files carry the `dui_` prefix to prevent name collisions when this code is merged into an existing project.

---

## Directory Layout

```
debug-ui-demo/
├── CMakeLists.txt
├── third_party/
│   ├── imgui/          ← imgui core + impl_win32 + impl_dx11 + headers
│   └── implot/         ← implot.cpp + implot_items.cpp + headers
└── src/
    ├── main.cpp
    ├── dui_app.h
    ├── dui_app.cpp
    ├── dui_world.h
    ├── dui_canvas.h
    ├── dui_canvas.cpp
    ├── dui_inspector.h
    ├── dui_inspector.cpp
    ├── dui_metrics.h
    └── dui_metrics.cpp
```

---

## Module Contracts

### `dui_app` — Win32 + D3D11 + ImGui Lifecycle

```cpp
namespace dui {
    class App {
    public:
        bool Init(int width, int height, const char* title);
        void Shutdown();
        bool PumpMessages();   // returns false on WM_QUIT
        void BeginFrame();
        void EndFrame();       // Render() + Present()
        bool IsRunning() const;
    };
}
```

`App` owns the HWND, D3D11 device/context/swapchain/RTV, and ImGui/ImPlot context init/shutdown. `BeginFrame` calls `ImGui_ImplDX11_NewFrame` + `ImGui_ImplWin32_NewFrame` + `ImGui::NewFrame`. `EndFrame` calls `ImGui::Render` + `ImGui_ImplDX11_RenderDrawData` + `Present`.

### `dui_world` — Data Structures + Mock Data

```cpp
namespace dui {
    struct Entity {
        uint64_t id;
        float x, y;       // world coordinates
        float radius;
        uint32_t color;   // IM_COL32 format
        char label[16];
    };

    struct World {
        std::vector<Entity> entities;
        int selected_id = -1;   // -1 = none selected
    };

    World MakeMockWorld();              // returns 15 random entities
    void  TickMockWorld(World& w, float dt);  // jitter positions each tick
}
```

No business logic here. Real projects replace `MakeMockWorld` / `TickMockWorld` with their own data.

### `dui_canvas` — 2D Scene Viewport

```cpp
namespace dui {
    struct Camera {
        float offset_x = 0.f, offset_y = 0.f;
        float zoom = 1.f;
    };

    // Converts world-space point to screen-space ImVec2
    ImVec2 WorldToScreen(float wx, float wy,
                         const Camera& cam,
                         ImVec2 canvas_origin);

    // Full canvas panel: grid + entities + interaction
    void DrawCanvas(World& world, Camera& cam);
}
```

`DrawCanvas` handles:
- `ImGui::BeginChild` + clip rect
- Grid lines via `AddLine`
- Entities via `AddCircleFilled` + `AddText`
- Selected entity: thick border `AddCircle`
- Right-drag → pan, scroll wheel → zoom (C++11 `ImGui::IsMouseDragging` / `GetIO`)
- Left-click → hit-test entities → set `world.selected_id`

### `dui_inspector` — Property Panel

```cpp
namespace dui {
    void DrawInspector(World& world);
}
```

`TreeNode("Entities")` listing all entities. Clicking an entry sets `world.selected_id` and scrolls canvas. Selected entity shows editable fields (`InputFloat` for x/y, `SliderFloat` for radius). Direct mutation of `World` is safe in single-thread context.

### `dui_metrics` — Real-time Charts

```cpp
namespace dui {
    template<typename T, int N>
    class RingBuffer {
    public:
        void push(T v);
        const T* data() const;
        int size() const;
        int offset() const;   // current write head, for ImPlot scroll
    };

    struct Metrics {
        RingBuffer<float, 300> tick_ms;
        RingBuffer<float, 300> entity_count;
    };

    void DrawMetrics(const Metrics& m);
}
```

`DrawMetrics` uses `ImPlot::PlotLine` with the ring buffer's raw pointer + offset for scrolling display. Two charts: tick duration (ms) and entity count over time.

---

## Main Loop (main.cpp)

```cpp
int WINAPI WinMain(...) {
    dui::App app;
    if (!app.Init(1280, 720, "Debug UI")) return 1;

    dui::World  world   = dui::MakeMockWorld();
    dui::Camera cam;
    dui::Metrics metrics;

    using Clock = std::chrono::steady_clock;
    auto last_ui  = Clock::now();
    auto last_tick = Clock::now();

    while (app.PumpMessages()) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - last_tick).count();
        last_tick = now;

        // Business tick
        dui::TickMockWorld(world, dt);
        float tick_ms = std::chrono::duration<float>(
            Clock::now() - now).count() * 1000.f;
        metrics.tick_ms.push(tick_ms);
        metrics.entity_count.push(static_cast<float>(world.entities.size()));

        // UI at ~60 Hz
        if (now - last_ui >= std::chrono::milliseconds(16)) {
            last_ui = now;
            app.BeginFrame();
            dui::DrawInspector(world);
            dui::DrawCanvas(world, cam);
            dui::DrawMetrics(metrics);
            app.EndFrame();
        }
    }

    app.Shutdown();
    return 0;
}
```

---

## CMake Structure

- Single target: `debug_ui_demo` (WIN32 executable)
- `third_party/imgui/CMakeLists.txt` → static lib `imgui`
- `third_party/implot/CMakeLists.txt` → static lib `implot`
- Links: `imgui`, `implot`, `d3d11`, `dxgi`, `d3dcompiler`
- C++ standard: 14 (`set(CMAKE_CXX_STANDARD 14)`)
- Compile definition: `UNICODE`

---

## Third-Party Files Required

**ImGui** (from `ocornut/imgui` master):
```
imgui.cpp  imgui_draw.cpp  imgui_tables.cpp  imgui_widgets.cpp
backends/imgui_impl_win32.cpp  backends/imgui_impl_dx11.cpp
imgui.h  imgui_internal.h  imstb_*.h
backends/imgui_impl_win32.h  backends/imgui_impl_dx11.h
```

**ImPlot** (from `epezent/implot` master):
```
implot.cpp  implot_items.cpp
implot.h  implot_internal.h
```

---

## Out of Scope

- Pause/step/record-playback (plan doc §7 "完善")
- ImGui ini persistence (enabled by default, no code needed)
- `#ifdef ENABLE_DEBUG_UI` release strip (add later when integrating into real project)
- Multi-thread synchronization (single-thread by design)
