// main.cpp
// WCI – See-Through "Dark Lens" Glass Viewport
// Lightweight, zero-lag, floating see-through glass window that dims glare on slides and lectures.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <string>
#include <algorithm>
#include <stdexcept>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ---------------------------------------------------------------------------
// Hotkey IDs
// ---------------------------------------------------------------------------
constexpr int HOTKEY_CLICK_THROUGH = 101;
constexpr int HOTKEY_FULLSCREEN    = 102;
constexpr int HOTKEY_TINT_UP       = 103;
constexpr int HOTKEY_TINT_DOWN     = 104;

// ---------------------------------------------------------------------------
// Application State
// ---------------------------------------------------------------------------
static HWND g_hWnd = nullptr;
static UINT g_Width  = 960;
static UINT g_Height = 540;

static bool  g_ClickThrough       = false;
static bool  g_IsFullscreen       = false;
static RECT  g_SavedWindowRect    = {};
static DWORD g_SavedWindowStyle   = 0;
static DWORD g_SavedWindowExStyle = 0;

// Glass tint opacity: 0 = completely clear, 255 = completely opaque
// Default 155 (~60% dark tint, ideal for reducing blinding white glare while keeping slides 100% visible)
static BYTE g_GlassAlpha = 155;

static HBRUSH g_TintBrush = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void UpdateTitleBar()
{
    if (!g_hWnd) return;
    int tintPercent = static_cast<int>((g_GlassAlpha / 255.0f) * 100.0f);
    wchar_t title[256];
    swprintf_s(title,
        L"Dark Lens [See-Through Glass] | Tint: %d%% (Ctrl+\x2191/\x2193) | %s [Ctrl+F1] [F11] [Esc: Exit]",
        tintPercent,
        g_ClickThrough ? L"[CLICK-THROUGH ON]" : L"[Normal Mode]");
    SetWindowTextW(g_hWnd, title);
}

static void ApplyGlassAlpha()
{
    if (!g_hWnd) return;
    SetLayeredWindowAttributes(g_hWnd, 0, g_GlassAlpha, LWA_ALPHA);
    UpdateTitleBar();
}

static void ToggleClickThrough()
{
    g_ClickThrough = !g_ClickThrough;
    LONG_PTR exStyle = GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE);
    if (g_ClickThrough)
        exStyle |= WS_EX_TRANSPARENT;
    else
        exStyle &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE, exStyle);
    UpdateTitleBar();
}

static void ToggleFullscreen()
{
    if (!g_IsFullscreen)
    {
        // Save current placement
        g_SavedWindowStyle   = GetWindowLongW(g_hWnd, GWL_STYLE);
        g_SavedWindowExStyle = GetWindowLongW(g_hWnd, GWL_EXSTYLE);
        GetWindowRect(g_hWnd, &g_SavedWindowRect);

        HMONITOR hMon = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(hMon, &mi);

        SetWindowLongW(g_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_hWnd, HWND_TOPMOST,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        g_IsFullscreen = true;
    }
    else
    {
        SetWindowLongW(g_hWnd, GWL_STYLE, g_SavedWindowStyle);
        SetWindowLongW(g_hWnd, GWL_EXSTYLE, g_SavedWindowExStyle);
        SetWindowPos(g_hWnd, HWND_TOPMOST,
            g_SavedWindowRect.left, g_SavedWindowRect.top,
            g_SavedWindowRect.right - g_SavedWindowRect.left,
            g_SavedWindowRect.bottom - g_SavedWindowRect.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        g_IsFullscreen = false;
    }
}

// ---------------------------------------------------------------------------
// Window Procedure
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_TintBrush);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Prevent flicker

    case WM_HOTKEY:
    {
        switch (wParam)
        {
        case HOTKEY_CLICK_THROUGH:
            ToggleClickThrough();
            break;

        case HOTKEY_FULLSCREEN:
            ToggleFullscreen();
            break;

        case HOTKEY_TINT_UP:
            // Increase tint darkness (+15)
            g_GlassAlpha = static_cast<BYTE>(std::min(240, static_cast<int>(g_GlassAlpha) + 15));
            ApplyGlassAlpha();
            break;

        case HOTKEY_TINT_DOWN:
            // Decrease tint darkness (-15)
            g_GlassAlpha = static_cast<BYTE>(std::max(30, static_cast<int>(g_GlassAlpha) - 15));
            ApplyGlassAlpha();
            break;
        }
        return 0;
    }

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    try
    {
        // Dark slate charcoal tint brush for the glass
        g_TintBrush = CreateSolidBrush(RGB(15, 17, 20));

        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"WCI_SeeThroughGlass_Class";
        RegisterClassExW(&wc);

        // WS_EX_TOPMOST keeps it on top of presentations/videos.
        // WS_EX_LAYERED enables true see-through glass transparency.
        DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED;
        DWORD style   = WS_OVERLAPPEDWINDOW;

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int posX    = (screenW - static_cast<int>(g_Width))  / 2;
        int posY    = (screenH - static_cast<int>(g_Height)) / 2;

        RECT rc = { 0, 0, static_cast<LONG>(g_Width), static_cast<LONG>(g_Height) };
        AdjustWindowRectEx(&rc, style, FALSE, exStyle);

        g_hWnd = CreateWindowExW(
            exStyle,
            L"WCI_SeeThroughGlass_Class",
            L"Dark Lens [See-Through Glass]",
            style,
            posX, posY,
            rc.right - rc.left,
            rc.bottom - rc.top,
            nullptr, nullptr, hInstance, nullptr);

        if (!g_hWnd) throw std::runtime_error("CreateWindowExW failed");

        // Apply see-through glass transparency
        ApplyGlassAlpha();

        // Register Global Hotkeys
        RegisterHotKey(g_hWnd, HOTKEY_CLICK_THROUGH, MOD_CONTROL, VK_F1);
        RegisterHotKey(g_hWnd, HOTKEY_FULLSCREEN,    0,           VK_F11);
        RegisterHotKey(g_hWnd, HOTKEY_TINT_UP,       MOD_CONTROL, VK_UP);
        RegisterHotKey(g_hWnd, HOTKEY_TINT_DOWN,     MOD_CONTROL, VK_DOWN);

        UpdateTitleBar();

        ShowWindow(g_hWnd, SW_SHOW);
        UpdateWindow(g_hWnd);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Cleanup
        UnregisterHotKey(g_hWnd, HOTKEY_CLICK_THROUGH);
        UnregisterHotKey(g_hWnd, HOTKEY_FULLSCREEN);
        UnregisterHotKey(g_hWnd, HOTKEY_TINT_UP);
        UnregisterHotKey(g_hWnd, HOTKEY_TINT_DOWN);

        if (g_TintBrush) DeleteObject(g_TintBrush);

        return static_cast<int>(msg.wParam);
    }
    catch (std::exception const& ex)
    {
        MessageBoxA(nullptr, ex.what(), "Dark Lens Error", MB_OK | MB_ICONERROR);
        return -1;
    }
}
