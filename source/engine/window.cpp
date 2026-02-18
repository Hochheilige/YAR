#include "window.h"
#include "imgui_layer.h"
#include <imgui.h>

#include <Windows.h>
#include <glad/glad.h>

#include <iostream>

// WGL extension function pointers
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef BOOL(WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int);

// WGL constants
#define WGL_DRAW_TO_WINDOW_ARB         0x2001
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DOUBLE_BUFFER_ARB          0x2011
#define WGL_PIXEL_TYPE_ARB             0x2013
#define WGL_TYPE_RGBA_ARB              0x202B
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023
#define WGL_ACCELERATION_ARB           0x2003
#define WGL_FULL_ACCELERATION_ARB      0x2027

#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB   0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_FLAGS_ARB          0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB      0x00000001

static HWND g_hwnd = nullptr;
static HDC g_hdc = nullptr;
static HGLRC g_hglrc = nullptr;
static bool g_should_close = false;
static WindowDimensions dims;
static const wchar_t* WINDOW_CLASS_NAME = L"YAR_WindowClass";

static PFNWGLSWAPINTERVALEXTPROC g_wglSwapIntervalEXT = nullptr;

static mouse_move_callback g_mouse_callback = nullptr;
static scroll_wheel_callback g_scroll_callback = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_CLOSE:
        g_should_close = true;
        return 0;

    case WM_SIZE:
        dims.width = LOWORD(lParam);
        dims.height = HIWORD(lParam);
        return 0;

    case WM_MOUSEMOVE:
        if (g_mouse_callback)
        {
            double xpos = static_cast<double>(LOWORD(lParam));
            double ypos = static_cast<double>(HIWORD(lParam));
            g_mouse_callback(xpos, ypos);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (g_scroll_callback)
        {
            double yoffset = static_cast<double>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            g_scroll_callback(0.0, yoffset);
        }
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

bool init_window(const std::function<void()>& imgui_layer)
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    if (!RegisterClassExW(&wc))
        return false;

    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);

    // Create fullscreen popup window
    g_hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"Yet Another Renderer",
        WS_POPUP,
        0, 0,
        screen_width, screen_height,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!g_hwnd)
        return false;

    g_hdc = GetDC(g_hwnd);
    if (!g_hdc)
        return false;

    // --- WGL bootstrap: temp context to load extensions ---
    PIXELFORMATDESCRIPTOR tmp_pfd = {};
    tmp_pfd.nSize = sizeof(tmp_pfd);
    tmp_pfd.nVersion = 1;
    tmp_pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    tmp_pfd.iPixelType = PFD_TYPE_RGBA;
    tmp_pfd.cColorBits = 32;
    tmp_pfd.cDepthBits = 24;
    tmp_pfd.cStencilBits = 8;

    int tmp_format = ChoosePixelFormat(g_hdc, &tmp_pfd);
    if (!tmp_format)
        return false;
    SetPixelFormat(g_hdc, tmp_format, &tmp_pfd);

    HGLRC tmp_ctx = wglCreateContext(g_hdc);
    if (!tmp_ctx)
        return false;
    wglMakeCurrent(g_hdc, tmp_ctx);

    // Load WGL extensions
    auto wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
        wglGetProcAddress("wglCreateContextAttribsARB");
    auto wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)
        wglGetProcAddress("wglChoosePixelFormatARB");

    if (!wglCreateContextAttribsARB || !wglChoosePixelFormatARB)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tmp_ctx);
        return false;
    }

    // Destroy temp context
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tmp_ctx);

    // Need to re-create the window to set a new pixel format
    ReleaseDC(g_hwnd, g_hdc);
    DestroyWindow(g_hwnd);

    g_hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"Yet Another Renderer",
        WS_POPUP,
        0, 0,
        screen_width, screen_height,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!g_hwnd)
        return false;

    g_hdc = GetDC(g_hwnd);

    // Choose proper pixel format with ARB extension
    int pixel_format_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
        WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,     32,
        WGL_DEPTH_BITS_ARB,     24,
        WGL_STENCIL_BITS_ARB,   8,
        WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
        0
    };

    int pixel_format;
    UINT num_formats;
    if (!wglChoosePixelFormatARB(g_hdc, pixel_format_attribs, nullptr, 1, &pixel_format, &num_formats) || num_formats == 0)
        return false;

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(g_hdc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(g_hdc, pixel_format, &pfd))
        return false;

    // Create OpenGL 4.6 core profile context
    int context_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#if _DEBUG
        WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
        0
    };

    g_hglrc = wglCreateContextAttribsARB(g_hdc, nullptr, context_attribs);
    if (!g_hglrc)
        return false;

    if (!wglMakeCurrent(g_hdc, g_hglrc))
        return false;

    // Load wglSwapIntervalEXT and disable vsync
    g_wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (g_wglSwapIntervalEXT)
        g_wglSwapIntervalEXT(0);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    dims.width = screen_width;
    dims.height = screen_height;

    imgui_init(g_hwnd, imgui_layer);

    return true;
}

bool update_window()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !g_should_close;
}

void terminate_window()
{
    imgui_terminate();

    if (g_hglrc)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_hglrc);
        g_hglrc = nullptr;
    }
    if (g_hdc && g_hwnd)
    {
        ReleaseDC(g_hwnd, g_hdc);
        g_hdc = nullptr;
    }
    if (g_hwnd)
    {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
    UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandle(nullptr));
}

swap_buffers get_swap_buffers_func()
{
    static HDC captured_hdc = g_hdc;
    auto win32_swap_buffers = [](void*) {
        SwapBuffers(captured_hdc);
    };
    return (swap_buffers)win32_swap_buffers;
}

swap_interval get_swap_interval_func()
{
    auto win32_swap_interval = [](bool vsync) {
        if (g_wglSwapIntervalEXT)
            g_wglSwapIntervalEXT(vsync ? 1 : 0);
    };
    return (swap_interval)win32_swap_interval;
}

void* get_window()
{
    return static_cast<void*>(g_hwnd);
}

const WindowDimensions& get_window_dims()
{
    return dims;
}

void register_mouse_callback(mouse_move_callback cb)
{
    g_mouse_callback = cb;
}

void register_scroll_callback(scroll_wheel_callback cb)
{
    g_scroll_callback = cb;
}
