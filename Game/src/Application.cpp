#include "Application.h"

#include <algorithm>
#include <filesystem>
#include <shellapi.h>
#include <wincodec.h>
#include <ScreenGrab.h>
#include "Log.h"

using namespace DirectX;
namespace fs = std::filesystem;

namespace
{
    AppOptions ParseOptions()
    {
        AppOptions opt;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i < argc; ++i)
        {
            std::wstring arg = argv[i];
            if (arg == L"--auto-run")
                opt.game.autoRun = true;
            else if (arg == L"--camera-yaw" && i + 1 < argc)
                opt.game.cameraYawDeg = std::stof(argv[++i]);
            else if (arg == L"--capture" && i + 2 < argc)
            {
                opt.capturePath = argv[++i];
                opt.captureFrame = std::stoi(argv[++i]);
            }
        }
        LocalFree(argv);
        return opt;
    }

    // 从 exe 所在目录向上查找包含 Assets/ 和 Game/shaders/ 的工程根目录
    fs::path FindProjectRoot()
    {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        for (const fs::path& start : { fs::path(exePath).parent_path(), fs::current_path() })
            for (fs::path dir = start; !dir.empty(); dir = dir.parent_path())
            {
                if (fs::exists(dir / "Assets" / "Player") && fs::exists(dir / "Game" / "shaders"))
                    return dir;
                if (dir == dir.root_path())
                    break;
            }
        return fs::current_path();
    }
}

Application::Application()
    : m_keyboard(std::make_unique<Keyboard>()), m_mouse(std::make_unique<Mouse>())
{
}

int Application::Run(HINSTANCE instance, int showCmd)
{
    int exitCode = 1;
    if (Initialize(instance, showCmd))
    {
        // 消息循环：空闲时跑一帧
        MSG msg = {};
        while (msg.message != WM_QUIT && !m_quit)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
                Tick();
        }
        exitCode = msg.message == WM_QUIT ? static_cast<int>(msg.wParam) : 0;
    }
    else if (m_hwnd)
        MessageBoxW(m_hwnd, L"初始化失败，详情见 SekiroDog.log", L"SekiroDog", MB_ICONERROR);

    Finalize();
    return exitCode;
}

bool Application::Initialize(HINSTANCE instance, int showCmd)
{
    m_options = ParseOptions();
    if (!CreateMainWindow(instance, showCmd))
        return false;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    if (!m_graphics.Initialize(m_hwnd, rc.right - rc.left, rc.bottom - rc.top))
        return false;
    m_mouse->SetWindow(m_hwnd);

    const fs::path root = FindProjectRoot();
    Log("Project root: %ls", root.c_str());
    const InitArgs args{ m_graphics.Device(), m_graphics.Context(), root / "Game" / "shaders", root / "Assets" };
    if (!m_game.Initialize(args, m_options.game))
        return false;

    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_lastTime);
    m_initialized = true;
    return true;
}

void Application::Tick()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = static_cast<float>(now.QuadPart - m_lastTime.QuadPart) / static_cast<float>(m_frequency.QuadPart);
    m_lastTime = now;
    dt = m_options.captureFrame >= 0 ? 1.0f / 60.0f : std::min(dt, 0.1f);

    // 输入
    const Keyboard::State keys = m_keyboard->GetState();
    const Mouse::State mouse = m_mouse->GetState();
    m_keyTracker.Update(keys);
    m_mouseTracker.Update(mouse);
    UpdateMouseLock();

    // 更新与描画
    m_game.Update({ dt, keys, mouse });

    m_graphics.BeginFrame(m_game.ClearColor());
    const float aspect = static_cast<float>(m_graphics.Width()) / static_cast<float>(m_graphics.Height());
    m_game.Draw(m_graphics.Context(), aspect);

    if (m_frame == m_options.captureFrame)
    {
        HRESULT hr = SaveWICTextureToFile(m_graphics.Context(), m_graphics.BackBuffer(), GUID_ContainerFormatPng,
                                          m_options.capturePath.c_str());
        Log("Captured frame %d to %ls (0x%08X)", m_frame, m_options.capturePath.c_str(), static_cast<unsigned>(hr));
        m_quit = true;
    }
    m_graphics.Present();
    ++m_frame;
}

void Application::Finalize()
{
    m_initialized = false;
    m_game.Finalize();
    m_graphics.Finalize();
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// 点击锁定鼠标，Q 或 Esc 解锁；未锁定时 Esc 退出
void Application::UpdateMouseLock()
{
    const bool locked = m_mouse->GetState().positionMode == Mouse::MODE_RELATIVE;
    if (m_keyTracker.pressed.Q && locked)
        m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
    if (m_keyTracker.pressed.Escape)
    {
        if (locked)
            m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
        else
            m_quit = true;
    }
    if (m_mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
        m_mouse->SetMode(Mouse::MODE_RELATIVE);
}

// 创建客户区为 ClientWidth x ClientHeight 的窗口并居中；屏幕放不下时最大化
bool Application::CreateMainWindow(HINSTANCE instance, int showCmd)
{
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SekiroDogWindow";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, ClientWidth, ClientHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    const int windowWidth = rc.right - rc.left;
    const int windowHeight = rc.bottom - rc.top;
    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int workWidth = workArea.right - workArea.left;
    const int workHeight = workArea.bottom - workArea.top;
    const bool fits = windowWidth <= workWidth && windowHeight <= workHeight;

    m_hwnd = CreateWindowExW(0, wc.lpszClassName, L"SekiroDog", WS_OVERLAPPEDWINDOW,
                             workArea.left + std::max(0, (workWidth - windowWidth) / 2),
                             workArea.top + std::max(0, (workHeight - windowHeight) / 2),
                             windowWidth, windowHeight, nullptr, nullptr, instance, this);
    if (!m_hwnd)
        return false;
    ShowWindow(m_hwnd, fits ? showCmd : SW_SHOWMAXIMIZED);
    return true;
}

LRESULT CALLBACK Application::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg)
    {
    case WM_CREATE:
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams));
        return 0;
    case WM_SIZE:
        if (app && app->m_initialized && wParam != SIZE_MINIMIZED)
            app->m_graphics.Resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
        Keyboard::ProcessMessage(msg, wParam, lParam);
        Mouse::ProcessMessage(msg, wParam, lParam);
        return 0;
    case WM_INPUT:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_MOUSEHOVER:
        Mouse::ProcessMessage(msg, wParam, lParam);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Keyboard::ProcessMessage(msg, wParam, lParam);
        return 0;
    case WM_SYSKEYDOWN:
        Keyboard::ProcessMessage(msg, wParam, lParam);
        break;
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATEANDEAT;   // 点击激活窗口时不触发游戏内点击
    case WM_DESTROY:
        if (app)
            app->m_hwnd = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
