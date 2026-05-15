#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <chrono>
#include <functional>
#include <imgui.h>

namespace dui {

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Self-hosted path: creates its own window and D3D11 device.
    bool Init(int width, int height, const wchar_t* title);

    // Injection path: attaches to a game's existing window and device.
    // The caller is responsible for:
    //   - forwarding WndProc messages to ImGui_ImplWin32_WndProcHandler
    //   - calling Present after EndFrame (EndFrame skips Present in this mode)
    //   - calling RebuildRenderTarget() after a swap chain resize
    // Call SetFontPath() before Attach() to load a custom font.
    bool Attach(HWND hwnd,
                ID3D11Device*        device,
                ID3D11DeviceContext* ctx,
                IDXGISwapChain*      swapchain);

    // Configure font before Init/Attach. Pass nullptr to use the ImGui built-in font.
    void SetFontPath(const char* ttf_path, float pixel_size = 18.f);

    // Override the default first-run dock layout. Pass a lambda that receives the
    // dockspace ID and calls DockBuilder* to arrange windows. Pass an empty function
    // to disable automatic layout entirely. Must be called before the first Tick().
    void SetDockLayoutFn(std::function<void(ImGuiID)> fn);

    // Rebuild the render-target view from the current swap chain back buffer.
    // Call this after a swap chain resize when using Attach mode.
    void RebuildRenderTarget();

    void Shutdown();
    bool PumpMessages();   // returns false when WM_QUIT received
    void BeginFrame();
    void EndFrame();       // ImGui::Render + DX11 draw; Present only in Init mode

    // Convenience: PumpMessages + BeginFrame + draw_fn() + EndFrame.
    // Only valid in Init (self-hosted) mode.
    bool Tick(const std::function<void()>&     draw_fn);
    bool Tick(const std::function<void(float)>& draw_fn);  // dt variant — built-in clock, clamped to 50ms

    float GetDt() const { return dt_; }  // last frame's dt (seconds, clamped)

private:
    HWND                    hwnd_      = nullptr;
    ID3D11Device*           device_    = nullptr;
    ID3D11DeviceContext*    ctx_       = nullptr;
    IDXGISwapChain*         swapchain_ = nullptr;
    ID3D11RenderTargetView* rtv_       = nullptr;

    bool owns_device_   = false;
    bool owns_window_   = false;
    bool layout_inited_ = false;
    std::chrono::steady_clock::time_point last_tick_ = std::chrono::steady_clock::now();
    float dt_ = 0.f;
    std::function<void(ImGuiID)> custom_layout_fn_;
    char  font_path_[512] = {};
    float font_size_      = 18.f;

    void InitImGui();
    void ApplyBuiltinLayout(ImGuiID dsid);
    bool CreateDeviceD3D(HWND hwnd);
    void CreateRenderTarget();
    void DestroyRenderTarget();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};

} // namespace dui
