#pragma once
#include "Strategy.h"
#include "ConfigLoader.h"
#include <memory>
#include <string>
#include <imgui.h>
#include <d3d11.h>

class ImGuiMonitor {
public:
    ImGuiMonitor(Strategy* strategy, std::shared_ptr<ConfigLoader> config);
    ~ImGuiMonitor();

    // Blocking — runs Win32 message loop on the calling thread (main thread)
    void Run();

private:
    void RenderFrame();
    void BuildDefaultDockLayout(ImGuiID id);

    bool InitD3D(HWND hwnd);
    void CleanupD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();

    Strategy*                       strategy_;
    std::shared_ptr<ConfigLoader>   config_;

    // DX11 state
    ID3D11Device*                   pd3dDevice_           = nullptr;
    ID3D11DeviceContext*            pd3dDeviceContext_    = nullptr;
    IDXGISwapChain*                 pSwapChain_           = nullptr;
    ID3D11RenderTargetView*         pMainRenderTargetView_= nullptr;

    bool dockLayoutBuilt_ = false;
};
