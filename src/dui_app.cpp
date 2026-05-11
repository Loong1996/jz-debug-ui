#include "dui_app.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <implot.h>

// Forward declare the WndProc handler (declared in imgui_impl_win32.h but inside #if 0)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) return false;
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

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "debug_ui.ini";

    // Load bundled Chinese font; falls back to built-in if file is missing
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/LXGWWenKai-Regular.ttf", 18.0f, nullptr,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    if (!font)
        io.Fonts->AddFontDefault();

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

bool App::Tick(const std::function<void()>& draw_fn) {
    if (!PumpMessages()) return false;
    BeginFrame();
    draw_fn();
    EndFrame();
    return true;
}

} // namespace dui
