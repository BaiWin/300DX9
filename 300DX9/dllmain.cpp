#include <Windows.h>
#include <stdio.h>
#include <d3d9.h>

#include "MinHook.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "menu.h"
#include "300.h"

#pragma comment(lib, "d3d9.lib")

// ================================
// Present
// ================================
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(
    IDirect3DDevice9*,
    const RECT*,
    const RECT*,
    HWND,
    const RGNDATA*
    );

Present_t oPresent = nullptr;

// ================================
// Reset
// ================================
typedef HRESULT(STDMETHODCALLTYPE* Reset_t)(
    IDirect3DDevice9*,
    D3DPRESENT_PARAMETERS*
    );

Reset_t oReset = nullptr;

// ================================
// Global
// ================================
IDirect3DDevice9* g_Device = nullptr;
HWND g_hWnd = nullptr;
bool g_ImGuiInitialized = false;

WNDPROC oWndProc = nullptr;

bool g_MenuOpen = true;

HRESULT STDMETHODCALLTYPE hkReset(
    IDirect3DDevice9* device,
    D3DPRESENT_PARAMETERS* params)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();

    HRESULT result = oReset(
        device,
        params
    );

    if (SUCCEEDED(result))
    {
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    return result;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK hkWndProc(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    if (msg == WM_KEYUP && wParam == VK_INSERT)
    {
        g_MenuOpen = !g_MenuOpen;

        return 0;
    }

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
    {
        if (g_activeHero)
        {
            if (!g_activeHero->OnKeyDown(wParam))
            {
                return 0;
            }
        }
        if (wParam == 'D' && g_summonnerSkillSlotInfo.slot1_Hiden_D_CD == 0) 
        {
            key_flags |= 1 << 5;
            //return 0;               //优先使用隐藏技能，治疗就和原技能一起瞬发了
        }
        else if (wParam == 'F' && g_summonnerSkillSlotInfo.slot2_Hiden_F_CD == 0 && g_summonnerSkillSlotInfo.slot2_Hiden_F_Start < 0.9) //防止连点，把两次闪现都用了
        {
            key_flags |= 1 << 6;
            return 0;
        }

        if (wParam == VK_SPACE)
        {
            //bSpace = true;
        }
    }

    if (msg == WM_KEYUP || wParam == WM_SYSKEYUP)
    {
        if (g_activeHero)
        {
            if (!g_activeHero->OnKeyUp(wParam))
            {
                return 0;
            }
        }

        if (wParam == 'D')
        {
            key_flags &= ~(1 << 5);
        }
        else if (wParam == 'F')
        {
            key_flags &= ~(1 << 6);
        }

        if (wParam == VK_SPACE)
        {
            //bSpace = false;
        }
    }

    if (g_MenuOpen)
    {
        ImGui_ImplWin32_WndProcHandler(
            hWnd,
            msg,
            wParam,
            lParam
        );

        ImGuiIO& io = ImGui::GetIO();

        if (io.WantCaptureMouse)
        {
            switch (msg)
            {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                return 0;
            }
        }

        if (io.WantCaptureKeyboard)
        {
            switch (msg)
            {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_SYSCHAR:
                return 0;
            }
        }
    }

    return CallWindowProc(
        oWndProc,
        hWnd,
        msg,
        wParam,
        lParam
    );
}

void InitImGui(IDirect3DDevice9* device)
{
    if (g_ImGuiInitialized)
        return;

    D3DDEVICE_CREATION_PARAMETERS params{};

    if (FAILED(device->GetCreationParameters(&params)))
        return;

    HWND hwnd = params.hFocusWindow;

    if (!hwnd)
        return;

    g_Device = device;
    g_hWnd = hwnd;

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    MenuStyle();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(device);

    oWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(
            hwnd,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(hkWndProc)
        )
        );

    g_ImGuiInitialized = true;

    /*OutputDebugStringA(
        "ImGui initialized successfully!\n"
    );*/
}

extern void LogicUpdate();

HRESULT STDMETHODCALLTYPE hkPresent(
    IDirect3DDevice9* device,
    const RECT* src,
    const RECT* dst,
    HWND hwnd,
    const RGNDATA* dirty
)
{
    if (!g_ImGuiInitialized)
    {
        InitImGui(device);
    }

    if (g_ImGuiInitialized)
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawMenu();

        ImGui::Render();

        ImGui_ImplDX9_RenderDrawData(
            ImGui::GetDrawData()
        );
    }

    if (g_ImGuiInitialized)
    {
        LogicUpdate(); 
    }

    return oPresent(
        device,
        src,
        dst,
        hwnd,
        dirty
    );
}

IDirect3DDevice9* CreateDummyDevice()
{
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);

    if (!d3d)
        return nullptr;

    HWND hwnd = CreateWindowExA(
        0,
        "STATIC",
        "DummyWindow",
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        100,
        100,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (!hwnd)
    {
        d3d->Release();
        return nullptr;
    }

    D3DPRESENT_PARAMETERS pp{};

    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* device = nullptr;

    HRESULT hr = d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp,
        &device
    );

    DestroyWindow(hwnd);
    d3d->Release();

    if (FAILED(hr))
        return nullptr;

    return device;
}

void TestD3D9()
{
    IDirect3DDevice9* device = CreateDummyDevice();

    if (!device)
    {
        MessageBoxA(
            nullptr,
            "CreateDevice failed",
            "Test",
            MB_OK
        );

        return;
    }

    void** vtable =
        *reinterpret_cast<void***>(device);

    void* presentAddress = vtable[17];

    char buffer[128];

    sprintf_s(
        buffer,
        "Device = %p\nPresent = %p",
        device,
        presentAddress
    );

    MessageBoxA(
        nullptr,
        buffer,
        "DX9",
        MB_OK
    );

    device->Release();
}

void InstallHook()
{
    IDirect3DDevice9* device =
        CreateDummyDevice();

    if (!device)
        return;

    void** vtable =
        *reinterpret_cast<void***>(device);

    void* resetAddress = vtable[16];
    void* presentAddress = vtable[17];

    if (MH_Initialize() != MH_OK)
    {
        device->Release();
        return;
    }

    if (MH_CreateHook(
        resetAddress,
        &hkReset,
        reinterpret_cast<void**>(&oReset)
    ) != MH_OK)
    {
        device->Release();
        return;
    }

    if (MH_CreateHook(
        presentAddress,
        &hkPresent,
        reinterpret_cast<void**>(&oPresent)
    ) != MH_OK)
    {
        device->Release();
        return;
    }

    MH_EnableHook(resetAddress);
    MH_EnableHook(presentAddress);

    device->Release();

    /*OutputDebugStringA(
        "DX9 hooks installed!\n"
    );*/
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    GameMain();
    //TestD3D9();
    InstallHook();

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        CreateThread(
            nullptr,
            0,
            MainThread,
            nullptr,
            0,
            nullptr
        );
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

