#include "ImGuiMonitor.h"
#include <iostream>
#include <tchar.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

ImGuiMonitor::ImGuiMonitor(Strategy* strategy, std::shared_ptr<ConfigLoader> config)
    : strategy_(strategy), config_(config) {
}

ImGuiMonitor::~ImGuiMonitor() {
}

bool ImGuiMonitor::InitD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &pSwapChain_, &pd3dDevice_, &featureLevel, &pd3dDeviceContext_);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void ImGuiMonitor::CleanupD3D() {
    CleanupRenderTarget();
    if (pSwapChain_) { pSwapChain_->Release(); pSwapChain_ = nullptr; }
    if (pd3dDeviceContext_) { pd3dDeviceContext_->Release(); pd3dDeviceContext_ = nullptr; }
    if (pd3dDevice_) { pd3dDevice_->Release(); pd3dDevice_ = nullptr; }
}

void ImGuiMonitor::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    pSwapChain_->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    pd3dDevice_->CreateRenderTargetView(pBackBuffer, nullptr, &pMainRenderTargetView_);
    pBackBuffer->Release();
}

void ImGuiMonitor::CleanupRenderTarget() {
    if (pMainRenderTargetView_) { pMainRenderTargetView_->Release(); pMainRenderTargetView_ = nullptr; }
}

void ImGuiMonitor::Run() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("Hermes Monitor"), nullptr };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("HermesStrategyShell - MONITOR"), WS_OVERLAPPEDWINDOW, 100, 100, 1440, 900, nullptr, nullptr, wc.hInstance, nullptr);

    if (!InitD3D(hwnd)) {
        CleanupD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(pd3dDevice_, pd3dDeviceContext_);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // Resize handling
        RECT rect;
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        static int prev_width = 0;
        static int prev_height = 0;
        if (width != prev_width || height != prev_height) {
            if (pSwapChain_) {
                CleanupRenderTarget();
                pSwapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            prev_width = width;
            prev_height = height;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderFrame();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        pd3dDeviceContext_->OMSetRenderTargets(1, &pMainRenderTargetView_, nullptr);
        pd3dDeviceContext_->ClearRenderTargetView(pMainRenderTargetView_, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        pSwapChain_->Present(1, 0); // VSync enabled
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
}

void ImGuiMonitor::BuildDefaultDockLayout(ImGuiID id) {
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->Size);
    ImGui::DockBuilderDockWindow("Dashboard", id);
    ImGui::DockBuilderFinish(id);
}

void ImGuiMonitor::RenderFrame() {
    // Fullscreen invisible host window
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockSpace", nullptr,
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar();

    ImGuiID dsid = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dsid, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    if (!dockLayoutBuilt_) { BuildDefaultDockLayout(dsid); dockLayoutBuilt_ = true; }
    ImGui::End();

    if (ImGui::Begin("Dashboard")) {
        ImGui::Text("HermesStrategyShell UI Template");
        ImGui::Separator();
        ImGui::Text("Implement your UI monitoring components here.");
    }
    ImGui::End();
}
