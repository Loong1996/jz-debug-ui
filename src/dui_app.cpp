#include "dui_app.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <implot.h>
#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace dui {

static App* g_app_instance = nullptr;

App::App()  = default;
App::~App() { Shutdown(); }

bool App::Init(int width, int height, const wchar_t* title) {
    owns_device_ = true;
    owns_window_ = true;
    g_app_instance = this;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DuiWindow";
    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            owns_device_ = owns_window_ = false;
            g_app_instance = nullptr;
            return false;
        }
    }

    RECT r = { 0, 0, width, height };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd_ = CreateWindowExW(
        0, L"DuiWindow", title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (!hwnd_ || !CreateDeviceD3D(hwnd_)) {
        owns_device_ = owns_window_ = false;
        if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
        UnregisterClassW(L"DuiWindow", GetModuleHandleW(nullptr));
        g_app_instance = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    {
        ImGuiIO& io    = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename  = "debug_ui.ini";
    }

    InitImGui();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, ctx_);
    return true;
}

bool App::Attach(HWND hwnd,
                 ID3D11Device*        device,
                 ID3D11DeviceContext* ctx,
                 IDXGISwapChain*      swapchain) {
    if (!hwnd || !device || !ctx || !swapchain) return false;
    if (device_) return false;  // already initialized

    owns_device_ = false;
    owns_window_ = false;
    hwnd_      = hwnd;
    device_    = device;
    ctx_       = ctx;
    swapchain_ = swapchain;
    g_app_instance = this;

    CreateRenderTarget();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    {
        ImGuiIO& io    = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename  = "debug_ui.ini";
    }

    InitImGui();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, ctx_);
    return true;
}

void App::SetFontPath(const char* ttf_path, float pixel_size) {
    font_size_ = pixel_size;
    if (ttf_path && ttf_path[0])
        std::snprintf(font_path_, sizeof(font_path_), "%s", ttf_path);
    else
        font_path_[0] = '\0';
}

void App::InitImGui() {
    ImGuiIO& io = ImGui::GetIO();
    // In Init (self-hosted) mode, fall back to bundled demo font if no path was set.
    const char* fp = font_path_[0] ? font_path_
                   : (owns_window_ ? "assets/fonts/LXGWWenKai-Regular.ttf" : nullptr);
    if (fp) {
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            fp, font_size_, nullptr,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (font) return;
    }
    io.Fonts->AddFontDefault();
}

void App::Shutdown() {
    if (!device_) return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    DestroyRenderTarget();

    if (owns_device_) {
        if (swapchain_) { swapchain_->Release(); swapchain_ = nullptr; }
        if (ctx_)       { ctx_->Release();       ctx_       = nullptr; }
        device_->Release();                       device_    = nullptr;
    } else {
        swapchain_ = nullptr;
        ctx_       = nullptr;
        device_    = nullptr;
    }

    if (owns_window_) {
        if (hwnd_) DestroyWindow(hwnd_);  // may be null if WM_DESTROY already fired
        UnregisterClassW(L"DuiWindow", GetModuleHandleW(nullptr));
    }
    hwnd_          = nullptr;
    g_app_instance = nullptr;
}

bool App::PumpMessages() {
    MSG msg;
    if (owns_window_) {
        // Standard standalone loop: WM_QUIT is on the thread queue (hwnd=NULL),
        // so we must pump with nullptr to catch it. Check it before dispatch so
        // we don't call BeginFrame/EndFrame after the window is gone.
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    } else {
        // Embedded in a host app: filter to our hwnd only — avoids stealing
        // messages from the host's message loop (e.g. MFC) on the same thread.
        while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        // WM_QUIT is posted to the thread queue (no HWND), check separately.
        if (PeekMessageW(&msg, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) return false;
    }
    return true;
}

void App::BeginFrame() {
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
}

void App::EndFrame() {
    ImGui::Render();
    const float clear[4] = { 0.08f, 0.08f, 0.10f, 1.0f };
    ctx_->OMSetRenderTargets(1, &rtv_, nullptr);
    ctx_->ClearRenderTargetView(rtv_, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (owns_device_) swapchain_->Present(1, 0);
}

bool App::Tick(const std::function<void()>& draw_fn) {
    if (!PumpMessages()) return false;
    BeginFrame();
    draw_fn();
    EndFrame();
    return true;
}

void App::RebuildRenderTarget() {
    DestroyRenderTarget();
    CreateRenderTarget();
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
        if (g_app_instance) g_app_instance->hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace dui
