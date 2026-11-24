#include "overlay.h"
#include "log.h"
#include <windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <wrl/client.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")

using Microsoft::WRL::ComPtr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace overlay {

static HWND  g_hwnd = nullptr;
static bool  g_menu_open = false;
static int   g_w = 0, g_h = 0;
static const wchar_t* kClass = L"hi_ov_cls_x7Q";

static ComPtr<ID3D11Device>           g_dev;
static ComPtr<ID3D11DeviceContext>    g_ctx;
static ComPtr<IDXGISwapChain>         g_sc;
static ComPtr<ID3D11RenderTargetView> g_rtv;

static bool CreateRT() {
    ComPtr<ID3D11Texture2D> bb;
    HRESULT hr = g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)bb.GetAddressOf());
    if (FAILED(hr)) { hilog::Err("sc->GetBuffer hr=0x%08lX", (unsigned long)hr); return false; }
    hr = g_dev->CreateRenderTargetView(bb.Get(), nullptr, &g_rtv);
    if (FAILED(hr)) { hilog::Err("CreateRTV hr=0x%08lX", (unsigned long)hr); return false; }
    return true;
}
static void ReleaseRT() { g_rtv.Reset(); }

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, msg, w, l)) return true;
    switch (msg) {
        case WM_SIZE:
            if (g_dev && w != SIZE_MINIMIZED) {
                ReleaseRT();
                g_sc->ResizeBuffers(0, LOWORD(l), HIWORD(l), DXGI_FORMAT_UNKNOWN, 0);
                CreateRT();
                g_w = LOWORD(l); g_h = HIWORD(l);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

bool Init() {
    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    int sx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int sy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_w    = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_h    = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST
      | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kClass, L"", WS_POPUP,
        sx, sy, g_w, g_h,
        nullptr, nullptr, hi, nullptr);
    if (!g_hwnd) { hilog::Err("CreateWindowExW gle=%lu", GetLastError()); return false; }

    MARGINS m{ -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &m);
    SetWindowDisplayAffinity(g_hwnd, WDA_EXCLUDEFROMCAPTURE);

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hwnd);

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount         = 2;
    scd.BufferDesc.Format   = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width    = g_w;
    scd.BufferDesc.Height   = g_h;
    scd.BufferUsage         = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow        = g_hwnd;
    scd.SampleDesc.Count    = 1;
    scd.Windowed            = TRUE;
    scd.SwapEffect          = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        fls, 1, D3D11_SDK_VERSION, &scd,
        &g_sc, &g_dev, nullptr, &g_ctx);
    if (FAILED(hr)) { hilog::Err("D3D11CreateDeviceAndSwapChain hr=0x%08lX", (unsigned long)hr); return false; }
    if (!CreateRT()) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    if (!ImGui_ImplWin32_Init(g_hwnd))                     { hilog::Err("ImGui_ImplWin32_Init"); return false; }
    if (!ImGui_ImplDX11_Init(g_dev.Get(), g_ctx.Get()))    { hilog::Err("ImGui_ImplDX11_Init"); return false; }

    hilog::Info("overlay init %dx%d hwnd=%p", g_w, g_h, (void*)g_hwnd);
    return true;
}

void Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    ReleaseRT();
    g_sc.Reset(); g_ctx.Reset(); g_dev.Reset();
    if (g_hwnd) DestroyWindow(g_hwnd);
    UnregisterClassW(kClass, GetModuleHandleW(nullptr));
}

void BeginFrame() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EndFrame() {
    ImGui::Render();
    const float clear[4] = { 0, 0, 0, 0 };
    g_ctx->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);
    g_ctx->ClearRenderTargetView(g_rtv.Get(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_sc->Present(1, 0);
}

void AssertTopmost() {
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
}

HWND Hwnd() { return g_hwnd; }
bool MenuOpen() { return g_menu_open; }
void GetSize(int& w, int& h) { w = g_w; h = g_h; }

void SetMenuOpen(bool open) {
    if (open == g_menu_open) return;
    g_menu_open = open;
    LONG ex = GetWindowLongW(g_hwnd, GWL_EXSTYLE);
    if (open) ex &= ~WS_EX_TRANSPARENT;
    else      ex |=  WS_EX_TRANSPARENT;
    SetWindowLongW(g_hwnd, GWL_EXSTYLE, ex);
}

} // namespace overlay
