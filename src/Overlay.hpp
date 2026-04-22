#pragma once
#include <windows.h>
#include <d3d11.h>
#include "../include/imgui/imgui.h"
#include "../include/imgui/imgui_impl_win32.h"
#include "../include/imgui/imgui_impl_dx11.h"

class Overlay {
private:
    WNDCLASSEX wc;
    HWND hwnd;

    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    IDXGISwapChain* g_pSwapChain = nullptr;
    ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();

public:
    int screenWidth;
    int screenHeight;
    bool isRunning = true;
    bool showMenu = true;
    ImFont* espFont = nullptr;

    Overlay();
    ~Overlay();

    bool Initialize();
    bool RenderBegin();
    void RenderEnd();
    void Terminate();
};
