# Debug UI Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone C++14 debug UI demo (Win32 + DX11 + ImGui + ImPlot) with three panels: Inspector, 2D Canvas, and Metrics charts, driven by mock bouncing-entity data.

**Architecture:** Single-thread main loop alternates between a business tick (TickMockWorld) and a rate-limited UI render (~60 Hz). All UI code lives in the `dui` namespace; all source files carry the `dui_` prefix to avoid collisions when merged into a real project.

**Tech Stack:** C++14, CMake 3.14+, Dear ImGui (Win32+DX11 backends), ImPlot, DirectX 11, MSVC on Windows.

---

## File Map

| File | Role |
|------|------|
| `CMakeLists.txt` | Root build: two static libs + one WIN32 exe + one test exe |
| `third_party/imgui/CMakeLists.txt` | Static lib `imgui` |
| `third_party/implot/CMakeLists.txt` | Static lib `implot` (links imgui) |
| `src/dui_world.h` | Header-only: `Entity`, `World`, `MakeMockWorld`, `TickMockWorld` |
| `src/dui_metrics.h` | `RingBuffer<T,N>` template + `Metrics` struct + `DrawMetrics` decl |
| `src/dui_metrics.cpp` | `DrawMetrics` — ImPlot scrolling charts |
| `src/dui_app.h` | `App` class declaration (Win32 + D3D11 + ImGui) |
| `src/dui_app.cpp` | `App` implementation |
| `src/dui_canvas.h` | `Camera`, `WorldToScreen`, `DrawCanvas` decls |
| `src/dui_canvas.cpp` | `DrawCanvas` implementation |
| `src/dui_inspector.h` | `DrawInspector` decl |
| `src/dui_inspector.cpp` | `DrawInspector` implementation |
| `src/main.cpp` | `WinMain` — creates App, runs main loop |
| `tests/test_ring_buffer.cpp` | Assertion-based RingBuffer unit test (no GUI deps) |

---

## Task 1: Fetch third-party sources

**Files:**
- Create: `third_party/imgui/` (populated from GitHub)
- Create: `third_party/imgui/CMakeLists.txt`
- Create: `third_party/implot/` (populated from GitHub)
- Create: `third_party/implot/CMakeLists.txt`

- [ ] **Step 1: Clone ImGui and copy needed files**

Run in PowerShell from `D:\Code\debug-ui-demo`:
```powershell
git clone --depth 1 https://github.com/ocornut/imgui.git "$env:TEMP\imgui_src"
$dst = "third_party\imgui"
$src = "$env:TEMP\imgui_src"
Copy-Item "$src\imgui.cpp"           $dst
Copy-Item "$src\imgui_draw.cpp"      $dst
Copy-Item "$src\imgui_tables.cpp"    $dst
Copy-Item "$src\imgui_widgets.cpp"   $dst
Copy-Item "$src\imgui.h"             $dst
Copy-Item "$src\imgui_internal.h"    $dst
Copy-Item "$src\imstb_rectpack.h"    $dst
Copy-Item "$src\imstb_textedit.h"    $dst
Copy-Item "$src\imstb_truetype.h"    $dst
Copy-Item "$src\backends\imgui_impl_win32.cpp" "$dst\backends"
Copy-Item "$src\backends\imgui_impl_win32.h"   "$dst\backends"
Copy-Item "$src\backends\imgui_impl_dx11.cpp"  "$dst\backends"
Copy-Item "$src\backends\imgui_impl_dx11.h"    "$dst\backends"
```

Expected: no errors, files appear under `third_party/imgui/` and `third_party/imgui/backends/`.

- [ ] **Step 2: Create `third_party/imgui/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.14)

add_library(imgui STATIC
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    backends/imgui_impl_win32.cpp
    backends/imgui_impl_dx11.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi)
```

- [ ] **Step 3: Clone ImPlot and copy needed files**

```powershell
git clone --depth 1 https://github.com/epezent/implot.git "$env:TEMP\implot_src"
$dst2 = "third_party\implot"
$src2 = "$env:TEMP\implot_src"
Copy-Item "$src2\implot.cpp"          $dst2
Copy-Item "$src2\implot_items.cpp"    $dst2
Copy-Item "$src2\implot.h"            $dst2
Copy-Item "$src2\implot_internal.h"   $dst2
```

Expected: four files appear under `third_party/implot/`.

- [ ] **Step 4: Create `third_party/implot/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.14)

add_library(implot STATIC
    implot.cpp
    implot_items.cpp
)

target_include_directories(implot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(implot PUBLIC imgui)
```

---

## Task 2: Root CMakeLists.txt

**Files:**
- Create: `CMakeLists.txt`

- [ ] **Step 1: Write `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.14)
project(debug_ui_demo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)

add_subdirectory(third_party/imgui)
add_subdirectory(third_party/implot)

# ---- unit test (no GUI deps) ----
add_executable(test_ring_buffer tests/test_ring_buffer.cpp)
target_include_directories(test_ring_buffer PRIVATE src)

# ---- main demo ----
add_executable(debug_ui_demo WIN32
    src/main.cpp
    src/dui_app.cpp
    src/dui_canvas.cpp
    src/dui_inspector.cpp
    src/dui_metrics.cpp
)

target_include_directories(debug_ui_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot
)

target_link_libraries(debug_ui_demo PRIVATE
    imgui
    implot
    d3d11
    dxgi
    d3dcompiler
)
```

---

## Task 3: dui_world.h

**Files:**
- Create: `src/dui_world.h`

- [ ] **Step 1: Write `src/dui_world.h`**

```cpp
#pragma once
#include <vector>
#include <cstdint>
#include <cstdio>

namespace dui {

struct Entity {
    uint64_t id;
    float x, y;
    float vx, vy;
    float radius;
    uint32_t color;  // ABGR (IM_COL32 format: (A<<24)|(B<<16)|(G<<8)|R)
    char label[16];
};

struct World {
    std::vector<Entity> entities;
    int selected_id = -1;
};

inline World MakeMockWorld() {
    World w;
    unsigned seed = 0xDEADBEEFu;
    auto lcg = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 16) / 65535.f;
    };
    auto rng = [&](float lo, float hi) { return lo + lcg() * (hi - lo); };
    auto col32 = [](uint8_t r, uint8_t g, uint8_t b) -> uint32_t {
        return (220u << 24) | (static_cast<uint32_t>(b) << 16)
             | (static_cast<uint32_t>(g) << 8) | r;
    };
    const uint32_t palette[5] = {
        col32(255, 80,  80 ),
        col32(80,  200, 80 ),
        col32(80,  120, 255),
        col32(255, 200, 50 ),
        col32(200, 80,  255),
    };
    for (int i = 0; i < 15; ++i) {
        Entity e;
        e.id     = static_cast<uint64_t>(1000 + i);
        e.x      = rng(-10.f, 10.f);
        e.y      = rng(-10.f, 10.f);
        e.vx     = rng(-3.f,  3.f);
        e.vy     = rng(-3.f,  3.f);
        e.radius = rng(0.3f,  1.0f);
        e.color  = palette[i % 5];
        std::snprintf(e.label, sizeof(e.label), "#%d", static_cast<int>(e.id));
        w.entities.push_back(e);
    }
    return w;
}

inline void TickMockWorld(World& w, float dt) {
    const float bounds = 15.f;
    for (auto& e : w.entities) {
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        if (e.x >  bounds) { e.x =  bounds; e.vx = -e.vx; }
        if (e.x < -bounds) { e.x = -bounds; e.vx = -e.vx; }
        if (e.y >  bounds) { e.y =  bounds; e.vy = -e.vy; }
        if (e.y < -bounds) { e.y = -bounds; e.vy = -e.vy; }
    }
}

} // namespace dui
```

---

## Task 4: dui_metrics — RingBuffer + charts (TDD)

**Files:**
- Create: `src/dui_metrics.h`
- Create: `src/dui_metrics.cpp`
- Create: `tests/test_ring_buffer.cpp`

- [ ] **Step 1: Write the failing test `tests/test_ring_buffer.cpp`**

```cpp
#include <cassert>
#include <cstdio>
#include "dui_metrics.h"

int main() {
    using namespace dui;

    // empty
    RingBuffer<float, 4> rb;
    assert(rb.size() == 0);
    assert(rb.plot_count() == 0);
    assert(rb.plot_offset() == 0);

    // partial fill: 2 of 4
    rb.push(1.f);
    rb.push(2.f);
    assert(rb.size() == 2);
    assert(rb.plot_count() == 2);
    assert(rb.plot_offset() == 0);
    assert(rb.data()[0] == 1.f);
    assert(rb.data()[1] == 2.f);

    // exactly full: 4 of 4, head wraps to 0
    rb.push(3.f);
    rb.push(4.f);
    assert(rb.size() == 4);
    assert(rb.plot_count() == 4);
    assert(rb.plot_offset() == 0);

    // overflow: 5th push overwrites slot 0, head moves to 1
    rb.push(5.f);
    assert(rb.size() == 4);
    assert(rb.plot_count() == 4);
    assert(rb.plot_offset() == 1);
    assert(rb.data()[0] == 5.f);

    std::puts("RingBuffer: all assertions passed");
    return 0;
}
```

- [ ] **Step 2: Run test to confirm it fails to compile** (dui_metrics.h not yet written)

```powershell
cmake -B build -A x64
cmake --build build --config Debug --target test_ring_buffer
```

Expected: compile error — `dui_metrics.h` not found.

- [ ] **Step 3: Write `src/dui_metrics.h`**

```cpp
#pragma once
#include <array>

namespace dui {

template<typename T, int N>
class RingBuffer {
public:
    RingBuffer() : head_(0), count_(0) { buf_.fill(T{}); }

    void push(T v) {
        buf_[head_] = v;
        head_ = (head_ + 1) % N;
        if (count_ < N) ++count_;
    }

    const T* data()    const { return buf_.data(); }
    int      capacity() const { return N; }
    int      size()    const { return count_; }
    // For ImPlot scrolling ring-buffer display
    int plot_count()   const { return (count_ == N) ? N     : count_; }
    int plot_offset()  const { return (count_ == N) ? head_ : 0; }

private:
    std::array<T, N> buf_;
    int head_;
    int count_;
};

struct Metrics {
    RingBuffer<float, 300> tick_ms;
    RingBuffer<float, 300> entity_count;
};

void DrawMetrics(const Metrics& m);

} // namespace dui
```

- [ ] **Step 4: Build and run the test**

```powershell
cmake --build build --config Debug --target test_ring_buffer
.\build\Debug\test_ring_buffer.exe
```

Expected output:
```
RingBuffer: all assertions passed
```

- [ ] **Step 5: Write `src/dui_metrics.cpp`**

```cpp
#include "dui_metrics.h"
#include <imgui.h>
#include <implot.h>

namespace dui {

void DrawMetrics(const Metrics& m) {
    ImGui::Begin("Metrics");

    if (ImPlot::BeginPlot("Tick Time (ms)", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("frame", "ms");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 5.0, ImPlotCond_Always);
        ImPlot::PlotLine("tick_ms",
            m.tick_ms.data(),
            m.tick_ms.plot_count(),
            1.0, 0.0, 0,
            m.tick_ms.plot_offset());
        ImPlot::EndPlot();
    }

    if (ImPlot::BeginPlot("Entity Count", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("frame", "count");
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 20.0, ImPlotCond_Always);
        ImPlot::PlotLine("entities",
            m.entity_count.data(),
            m.entity_count.plot_count(),
            1.0, 0.0, 0,
            m.entity_count.plot_offset());
        ImPlot::EndPlot();
    }

    ImGui::End();
}

} // namespace dui
```

---

## Task 5: dui_app — Win32 + D3D11 + ImGui lifecycle

**Files:**
- Create: `src/dui_app.h`
- Create: `src/dui_app.cpp`

- [ ] **Step 1: Write `src/dui_app.h`**

```cpp
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

namespace dui {

class App {
public:
    App();
    ~App();

    bool Init(int width, int height, const wchar_t* title);
    void Shutdown();
    bool PumpMessages();   // returns false when WM_QUIT received
    void BeginFrame();
    void EndFrame();       // ImGui::Render + DX11 Present

private:
    HWND                    hwnd_      = nullptr;
    ID3D11Device*           device_    = nullptr;
    ID3D11DeviceContext*    ctx_       = nullptr;
    IDXGISwapChain*         swapchain_ = nullptr;
    ID3D11RenderTargetView* rtv_       = nullptr;

    bool CreateDeviceD3D(HWND hwnd);
    void DestroyDeviceD3D();
    void CreateRenderTarget();
    void DestroyRenderTarget();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};

} // namespace dui
```

- [ ] **Step 2: Write `src/dui_app.cpp`**

```cpp
#include "dui_app.h"
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <implot.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

namespace dui {

static App* g_app_instance = nullptr;

App::App()  = default;
App::~App() { Shutdown(); }

bool App::Init(int width, int height, const wchar_t* title) {
    g_app_instance = this;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DuiWindow";
    RegisterClassExW(&wc);

    RECT r = { 0, 0, width, height };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd_ = CreateWindowExW(
        0, L"DuiWindow", title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (!hwnd_ || !CreateDeviceD3D(hwnd_)) return false;

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = "debug_ui.ini";

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, ctx_);
    return true;
}

void App::Shutdown() {
    if (!hwnd_) return;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    DestroyDeviceD3D();
    DestroyWindow(hwnd_);
    UnregisterClassW(L"DuiWindow", GetModuleHandleW(nullptr));
    hwnd_ = nullptr;
    g_app_instance = nullptr;
}

bool App::PumpMessages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) return false;
    }
    return true;
}

void App::BeginFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void App::EndFrame() {
    ImGui::Render();
    const float clear[4] = { 0.08f, 0.08f, 0.10f, 1.0f };
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    ctx_->ClearRenderTargetView(rtv_, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapchain_->Present(1, 0);
}

bool App::CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION,
        &sd, &swapchain_, &device_, &fl, &ctx_);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}

void App::DestroyDeviceD3D() {
    DestroyRenderTarget();
    if (swapchain_) { swapchain_->Release(); swapchain_ = nullptr; }
    if (ctx_)       { ctx_->Release();       ctx_       = nullptr; }
    if (device_)    { device_->Release();    device_    = nullptr; }
}

void App::CreateRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    swapchain_->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        device_->CreateRenderTargetView(back, nullptr, &rtv_);
        back->Release();
    }
}

void App::DestroyRenderTarget() {
    if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return TRUE;
    switch (msg) {
    case WM_SIZE:
        if (g_app_instance && g_app_instance->device_ && wp != SIZE_MINIMIZED) {
            g_app_instance->DestroyRenderTarget();
            g_app_instance->swapchain_->ResizeBuffers(
                0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            g_app_instance->CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace dui
```

---

## Task 6: dui_canvas — 2D scene viewport

**Files:**
- Create: `src/dui_canvas.h`
- Create: `src/dui_canvas.cpp`

- [ ] **Step 1: Write `src/dui_canvas.h`**

```cpp
#pragma once
#include <imgui.h>
#include "dui_world.h"

namespace dui {

struct Camera {
    float offset_x = 0.f;
    float offset_y = 0.f;
    float zoom = 20.f;  // pixels per world unit
};

ImVec2 WorldToScreen(float wx, float wy, const Camera& cam,
                     ImVec2 origin, ImVec2 size);

void DrawCanvas(World& world, Camera& cam);

} // namespace dui
```

- [ ] **Step 2: Write `src/dui_canvas.cpp`**

```cpp
#include "dui_canvas.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace dui {

ImVec2 WorldToScreen(float wx, float wy, const Camera& cam,
                     ImVec2 origin, ImVec2 size) {
    return ImVec2(
        origin.x + size.x * 0.5f + (wx - cam.offset_x) * cam.zoom,
        origin.y + size.y * 0.5f - (wy - cam.offset_y) * cam.zoom
    );
}

void DrawCanvas(World& world, Camera& cam) {
    ImGui::Begin("Scene");

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 50.f) sz.x = 50.f;
    if (sz.y < 50.f) sz.y = 50.f;
    ImVec2 p1 = ImVec2(p0.x + sz.x, p0.y + sz.y);

    ImGui::InvisibleButton("canvas_area", sz,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();

    // Pan: right-drag
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.f)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        cam.offset_x -= delta.x / cam.zoom;
        cam.offset_y += delta.y / cam.zoom;
    }

    // Zoom: scroll wheel
    if (hovered && ImGui::GetIO().MouseWheel != 0.f) {
        float factor = (ImGui::GetIO().MouseWheel > 0) ? 1.1f : 0.9f;
        cam.zoom *= factor;
        if (cam.zoom < 1.f)   cam.zoom = 1.f;
        if (cam.zoom > 500.f) cam.zoom = 500.f;
    }

    // Select entity: left-click (pixel threshold 20px)
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        world.selected_id = -1;
        float best = 20.f;
        for (const auto& e : world.entities) {
            ImVec2 sp = WorldToScreen(e.x, e.y, cam, p0, sz);
            float dx = mouse.x - sp.x, dy = mouse.y - sp.y;
            float d  = sqrtf(dx * dx + dy * dy);
            if (d < best) { best = d; world.selected_id = static_cast<int>(e.id); }
        }
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(28, 28, 32, 255));
    dl->PushClipRect(p0, p1, true);

    // Grid
    float half_w = sz.x * 0.5f / cam.zoom;
    float half_h = sz.y * 0.5f / cam.zoom;
    float left   = cam.offset_x - half_w, right  = cam.offset_x + half_w;
    float bottom = cam.offset_y - half_h, top    = cam.offset_y + half_h;

    float grid_step = 1.f;
    if (cam.zoom < 5.f)  grid_step = 5.f;
    if (cam.zoom < 1.5f) grid_step = 10.f;

    float xs = floorf(left   / grid_step) * grid_step;
    float ys = floorf(bottom / grid_step) * grid_step;

    for (float wx = xs; wx <= right; wx += grid_step) {
        ImVec2 a = WorldToScreen(wx, top,    cam, p0, sz);
        ImVec2 b = WorldToScreen(wx, bottom, cam, p0, sz);
        dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
    }
    for (float wy = ys; wy <= top; wy += grid_step) {
        ImVec2 a = WorldToScreen(left,  wy, cam, p0, sz);
        ImVec2 b = WorldToScreen(right, wy, cam, p0, sz);
        dl->AddLine(a, b, IM_COL32(55, 55, 65, 255));
    }

    // Axes (slightly brighter)
    dl->AddLine(WorldToScreen(left,  0.f, cam, p0, sz),
                WorldToScreen(right, 0.f, cam, p0, sz),
                IM_COL32(80, 80, 100, 255));
    dl->AddLine(WorldToScreen(0.f, bottom, cam, p0, sz),
                WorldToScreen(0.f, top,    cam, p0, sz),
                IM_COL32(80, 80, 100, 255));

    // Entities
    for (const auto& e : world.entities) {
        ImVec2 sp = WorldToScreen(e.x, e.y, cam, p0, sz);
        float r   = e.radius * cam.zoom;
        if (r < 3.f) r = 3.f;
        dl->AddCircleFilled(sp, r, e.color);
        if (static_cast<int>(e.id) == world.selected_id)
            dl->AddCircle(sp, r + 4.f, IM_COL32(255, 230, 0, 255), 0, 2.5f);
        dl->AddText(ImVec2(sp.x + r + 3.f, sp.y - 7.f),
                    IM_COL32(220, 220, 220, 255), e.label);
    }

    dl->PopClipRect();

    // Hover tooltip
    if (hovered) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        for (const auto& e : world.entities) {
            ImVec2 sp = WorldToScreen(e.x, e.y, cam, p0, sz);
            float dx = mouse.x - sp.x, dy = mouse.y - sp.y;
            if (sqrtf(dx*dx + dy*dy) < e.radius * cam.zoom + 4.f) {
                char buf[64];
                std::snprintf(buf, sizeof(buf),
                    "%s  (%.2f, %.2f)", e.label, e.x, e.y);
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(buf);
                ImGui::EndTooltip();
                break;
            }
        }
    }

    ImGui::End();
}

} // namespace dui
```

---

## Task 7: dui_inspector — property panel

**Files:**
- Create: `src/dui_inspector.h`
- Create: `src/dui_inspector.cpp`

- [ ] **Step 1: Write `src/dui_inspector.h`**

```cpp
#pragma once
#include "dui_world.h"

namespace dui {
    void DrawInspector(World& world);
}
```

- [ ] **Step 2: Write `src/dui_inspector.cpp`**

```cpp
#include "dui_inspector.h"
#include <imgui.h>

namespace dui {

void DrawInspector(World& world) {
    ImGui::Begin("Inspector");

    ImGui::Text("Entities: %d", static_cast<int>(world.entities.size()));
    ImGui::Separator();

    if (ImGui::TreeNodeEx("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& e : world.entities) {
            bool selected = (static_cast<int>(e.id) == world.selected_id);
            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_Leaf |
                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                (selected ? ImGuiTreeNodeFlags_Selected : 0);
            ImGui::PushID(static_cast<int>(e.id));
            ImGui::TreeNodeEx(e.label, flags);
            if (ImGui::IsItemClicked())
                world.selected_id = selected ? -1 : static_cast<int>(e.id);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    if (world.selected_id != -1) {
        ImGui::Separator();
        for (auto& e : world.entities) {
            if (static_cast<int>(e.id) != world.selected_id) continue;
            ImGui::PushID(static_cast<int>(e.id));
            ImGui::Text("Selected: %s", e.label);
            ImGui::InputFloat("x",       &e.x,      0.1f, 1.f, "%.3f");
            ImGui::InputFloat("y",       &e.y,      0.1f, 1.f, "%.3f");
            ImGui::SliderFloat("radius", &e.radius, 0.1f, 3.f, "%.2f");
            ImGui::PopID();
            break;
        }
    }

    ImGui::End();
}

} // namespace dui
```

---

## Task 8: main.cpp + final build

**Files:**
- Create: `src/main.cpp`

- [ ] **Step 1: Write `src/main.cpp`**

```cpp
#include <windows.h>
#include <chrono>
#include "dui_app.h"
#include "dui_world.h"
#include "dui_canvas.h"
#include "dui_inspector.h"
#include "dui_metrics.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    dui::App app;
    if (!app.Init(1280, 720, L"Debug UI Demo")) return 1;

    dui::World   world   = dui::MakeMockWorld();
    dui::Camera  cam;
    dui::Metrics metrics;

    using Clock = std::chrono::steady_clock;
    auto last_ui   = Clock::now();
    auto last_tick = Clock::now();

    while (app.PumpMessages()) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - last_tick).count();
        if (dt > 0.05f) dt = 0.05f;
        last_tick = now;

        auto t0 = Clock::now();
        dui::TickMockWorld(world, dt);
        float tick_ms = std::chrono::duration<float>(
            Clock::now() - t0).count() * 1000.f;
        metrics.tick_ms.push(tick_ms);
        metrics.entity_count.push(
            static_cast<float>(world.entities.size()));

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

- [ ] **Step 2: Full build**

```powershell
cmake -B build -A x64
cmake --build build --config Debug
```

Expected — no errors, two binaries produced:
```
Build succeeded.
    0 Warning(s)
    0 Error(s)
```
Binaries at:
- `build\Debug\test_ring_buffer.exe`
- `build\Debug\debug_ui_demo.exe`

- [ ] **Step 3: Run ring buffer test**

```powershell
.\build\Debug\test_ring_buffer.exe
```

Expected:
```
RingBuffer: all assertions passed
```

- [ ] **Step 4: Run and visually verify the demo**

```powershell
.\build\Debug\debug_ui_demo.exe
```

Verify the following in the running window:
- [ ] Three panels appear: **Inspector** (left), **Scene** (center), **Metrics** (right)
- [ ] 15 colored circles move and bounce in Scene panel
- [ ] Scroll wheel zooms in/out; right-drag pans the canvas
- [ ] Left-clicking a circle selects it (yellow highlight ring)
- [ ] Selecting in Inspector highlights the entity in Scene and vice-versa
- [ ] Inspector shows editable x/y/radius for selected entity — drag a slider and confirm circle changes size live
- [ ] Both ImPlot charts scroll continuously
- [ ] Window resize works without crash (swapchain resize path)
