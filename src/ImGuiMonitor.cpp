#include "ImGuiMonitor.h"
#include "PositionState.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <iostream>
#include <map>
#include <tchar.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) return 0;
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
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

void ImGuiMonitor::Run() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Hermes UI"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("HermesFutCash - Arbitrage Monitor"), WS_OVERLAPPEDWINDOW, 100, 100, 1400, 900, NULL, NULL, wc.hInstance, NULL);

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
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(pd3dDevice_, pd3dDeviceContext_);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        if (!dockLayoutBuilt_) {
            BuildDefaultDockLayout(dockspace_id);
            dockLayoutBuilt_ = true;
        }

        RenderFrame();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        pd3dDeviceContext_->OMSetRenderTargets(1, &pMainRenderTargetView_, NULL);
        pd3dDeviceContext_->ClearRenderTargetView(pMainRenderTargetView_, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        pSwapChain_->Present(1, 0);
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
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->Size);
    
    ImGuiID dock_main_id = id;
    ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.5f, NULL, &dock_main_id);
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.5f, NULL, &dock_main_id);

    ImGui::DockBuilderDockWindow("Scanner", dock_id_top);
    ImGui::DockBuilderDockWindow("Active Positions", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Trade History", dock_id_bottom);

    ImGui::DockBuilderFinish(id);
}

void ImGuiMonitor::RenderFrame() {
    auto& snapshots = strategy_->GetSnapshots();

    if (ImGui::Begin("Scanner")) {
        if (ImGui::BeginTable("ScannerTable", 11, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("EQ Ask");
            ImGui::TableSetupColumn("FUT Bid");
            ImGui::TableSetupColumn("Spread");
            ImGui::TableSetupColumn("Gross PnL");
            ImGui::TableSetupColumn("Charges");
            ImGui::TableSetupColumn("Net PnL");
            ImGui::TableSetupColumn("ROI %");
            ImGui::TableSetupColumn("Gate");
            ImGui::TableHeadersRow();

            for (const auto& [sym, snap] : snapshots) {
                std::lock_guard<std::mutex> lk(snap->mtx);
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", sym.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", snap->cashAsk);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", snap->futBid);
                ImGui::TableSetColumnIndex(3); 
                if (snap->execSpread > 0) ImGui::TextColored(ImVec4(0, 1, 0, 1), "%.2f", snap->execSpread);
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "%.2f", snap->execSpread);
                
                ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", snap->grossPnl);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%.2f", snap->cashCharges + snap->futCharges);
                ImGui::TableSetColumnIndex(6); ImGui::Text("%.2f", snap->netPnl);
                
                ImGui::TableSetColumnIndex(7); 
                if (snap->roiPassed) ImGui::TextColored(ImVec4(0, 1, 0, 1), "%.2f%%", snap->roi);
                else ImGui::Text("%.2f%%", snap->roi);

                ImGui::TableSetColumnIndex(8);
                if (snap->signalValid) ImGui::TextColored(ImVec4(0, 1, 0, 1), "PASS");
                else ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "FAIL");
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();

    auto& posState = PositionState::Instance();
    auto lk = posState.Lock();
    auto& positions = posState.Positions();
    auto& history = posState.History();

    if (ImGui::Begin("Active Positions")) {
        if (ImGui::BeginTable("PosTable", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("Direction");
            ImGui::TableSetupColumn("Entry EQ");
            ImGui::TableSetupColumn("Entry FUT");
            ImGui::TableSetupColumn("Entry Spread");
            ImGui::TableSetupColumn("Lot Size");
            ImGui::TableSetupColumn("Charges");
            ImGui::TableSetupColumn("Action");
            ImGui::TableHeadersRow();

            for (auto& [sym, pos] : positions) {
                if (pos.legs.empty()) continue;
                auto& leg = pos.legs.front();
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", sym.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", pos.direction.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", leg.entry_cash_ask);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", leg.entry_fut_bid);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", leg.entry_spread);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%d", leg.lot_size);
                ImGui::TableSetColumnIndex(6); ImGui::Text("%.2f", leg.entry_charges);
                ImGui::TableSetColumnIndex(7);
                if (ImGui::Button(("Exit##" + sym).c_str())) {
                    strategy_->ForceExitPosition(sym);
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
    
    if (ImGui::Begin("Trade History")) {
        if (ImGui::BeginTable("HistTable", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("Entry EQ / FUT");
            ImGui::TableSetupColumn("Exit EQ / FUT");
            ImGui::TableSetupColumn("Charges");
            ImGui::TableSetupColumn("Net PnL");
            ImGui::TableSetupColumn("Reason");
            ImGui::TableHeadersRow();

            for (auto& rec : history) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", rec.symbol.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f / %.2f", rec.entry_cash_ask, rec.entry_fut_bid);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f / %.2f", rec.exit_cash_bid, rec.exit_fut_ask);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", rec.total_charges);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", rec.net_pnl);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%s", rec.exit_reason.c_str());
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
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
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &pSwapChain_, &pd3dDevice_, &featureLevel, &pd3dDeviceContext_) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void ImGuiMonitor::CleanupD3D() {
    CleanupRenderTarget();
    if (pSwapChain_) { pSwapChain_->Release(); pSwapChain_ = NULL; }
    if (pd3dDeviceContext_) { pd3dDeviceContext_->Release(); pd3dDeviceContext_ = NULL; }
    if (pd3dDevice_) { pd3dDevice_->Release(); pd3dDevice_ = NULL; }
}

void ImGuiMonitor::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    pSwapChain_->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    pd3dDevice_->CreateRenderTargetView(pBackBuffer, NULL, &pMainRenderTargetView_);
    pBackBuffer->Release();
}

void ImGuiMonitor::CleanupRenderTarget() {
    if (pMainRenderTargetView_) { pMainRenderTargetView_->Release(); pMainRenderTargetView_ = NULL; }
}
