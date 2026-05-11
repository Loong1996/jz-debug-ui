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
