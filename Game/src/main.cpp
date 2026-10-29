#include <filesystem>
#include <memory>
#include <string>
#include <windows.h>
#include <shellapi.h>
#include <wincodec.h>
#include <BufferHelpers.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <ScreenGrab.h>
#include "Environment.h"
#include "Graphics.h"
#include "Log.h"
#include "Player.h"
#include "SkinnedModel.h"
#include "ThirdPersonCamera.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;
namespace fs = std::filesystem;

namespace
{
    // 调试用命令行参数：
    //   --auto-run              模拟一直按住 W
    //   --camera-yaw <度>       初始相机朝向
    //   --capture <png> <帧号>  在指定帧截图后退出（固定 60 FPS 步长）
    struct Options
    {
        bool autoRun = false;
        float cameraYawDeg = 0.0f;
        std::wstring capturePath;
        int captureFrame = -1;
    };

    Options ParseOptions()
    {
        Options opt;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i < argc; ++i)
        {
            std::wstring arg = argv[i];
            if (arg == L"--auto-run")
                opt.autoRun = true;
            else if (arg == L"--camera-yaw" && i + 1 < argc)
                opt.cameraYawDeg = std::stof(argv[++i]);
            else if (arg == L"--capture" && i + 2 < argc)
            {
                opt.capturePath = argv[++i];
                opt.captureFrame = std::stoi(argv[++i]);
            }
        }
        LocalFree(argv);
        return opt;
    }

    class Game
    {
    public:
        Game()
            : m_keyboard(std::make_unique<Keyboard>()), m_mouse(std::make_unique<Mouse>())
        {
        }

        bool Initialize(HWND hwnd, int width, int height, const Options& options)
        {
            m_options = options;
            if (!m_graphics.Initialize(hwnd, width, height))
                return false;
            m_mouse->SetWindow(hwnd);

            ID3D11Device* device = m_graphics.Device();
            m_frameCB.Create(device);
            m_objectCB.Create(device);

            const fs::path root = SEKIRO_ROOT;
            const fs::path shaders = root / "Game" / "shaders";
            const fs::path player = root / "Assets" / "Player";

            SkinnedModelDesc desc;
            desc.fbxPath = (player / "Models" / "DogWarrior.fbx").string();
            desc.baseColorPath = (player / "Textures" / "DogWarrior_BaseColor.png").wstring();
            desc.normalPath = (player / "Textures" / "DogWarrior_Normal.png").wstring();
            desc.metallicSmoothnessPath = (player / "Textures" / "DogWarrior_MetallicSmoothness.png").wstring();
            desc.shaderPath = (shaders / "Skinned.hlsl").wstring();
            if (!m_playerModel.Load(device, m_graphics.Context(), desc))
                return false;

            m_idleClip = m_playerModel.FindClip("Idle");
            m_runClip = m_playerModel.FindClip("Run");
            Log("Idle clip %d, Run clip %d", m_idleClip, m_runClip);

            if (!m_environment.Initialize(device, (shaders / "Lit.hlsl").wstring()))
                return false;

            m_camera.SetYaw(XMConvertToRadians(m_options.cameraYawDeg));
            QueryPerformanceFrequency(&m_frequency);
            QueryPerformanceCounter(&m_lastTime);
            m_initialized = true;
            return true;
        }

        void OnResize(int width, int height)
        {
            if (m_initialized)
                m_graphics.Resize(width, height);
        }

        bool WantsQuit() const { return m_quit; }

        void Tick()
        {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float dt = static_cast<float>(now.QuadPart - m_lastTime.QuadPart) / static_cast<float>(m_frequency.QuadPart);
            m_lastTime = now;
            dt = m_options.captureFrame >= 0 ? 1.0f / 60.0f : std::min(dt, 0.1f);

            Update(dt);
            Render();
            ++m_frame;
        }

    private:
        void Update(float dt)
        {
            Keyboard::State keys = m_keyboard->GetState();
            Mouse::State mouse = m_mouse->GetState();
            m_keyTracker.Update(keys);
            m_mouseTracker.Update(mouse);

            // 点击锁定鼠标，Esc 解锁；未锁定时 Esc 退出
            if (m_keyTracker.pressed.Escape)
            {
                if (mouse.positionMode == Mouse::MODE_RELATIVE)
                    m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
                else
                    m_quit = true;
            }
            if (m_mouseTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
                m_mouse->SetMode(Mouse::MODE_RELATIVE);

            // --auto-run 时沿世界 +Z 跑，方便从任意相机角度截图
            m_player.Update(dt, keys, m_options.autoRun ? 0.0f : m_camera.Yaw(), m_options.autoRun);
            m_camera.Update(dt, mouse, m_player.Position());
            m_playerModel.UpdatePose(m_idleClip, m_player.IdleTime(), m_runClip, m_player.RunTime(), m_player.RunBlend());
        }

        void Render()
        {
            const float clear[4] = { 0.55f, 0.62f, 0.72f, 1.0f };   // 与雾颜色一致
            m_graphics.BeginFrame(clear);
            ID3D11DeviceContext* context = m_graphics.Context();

            const float aspect = static_cast<float>(m_graphics.Width()) / static_cast<float>(m_graphics.Height());
            FrameConstants frame = {};
            XMStoreFloat4x4(&frame.view, XMMatrixTranspose(m_camera.View()));
            XMStoreFloat4x4(&frame.proj, XMMatrixTranspose(Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(60.0f), aspect, 0.05f, 500.0f)));
            Vector3 lightDir(0.4f, -1.0f, 0.5f);
            lightDir.Normalize();
            frame.lightDir = lightDir;
            frame.cameraPos = m_camera.Position();
            m_frameCB.SetData(context, frame);

            ID3D11Buffer* frameCB = m_frameCB.GetBuffer();
            context->VSSetConstantBuffers(0, 1, &frameCB);
            context->PSSetConstantBuffers(0, 1, &frameCB);

            m_environment.Draw(context, m_objectCB);
            m_playerModel.Draw(context, m_player.World(), m_objectCB);

            if (m_frame == m_options.captureFrame)
            {
                HRESULT hr = SaveWICTextureToFile(context, m_graphics.BackBuffer(), GUID_ContainerFormatPng, m_options.capturePath.c_str());
                Log("Captured frame %d to %ls (0x%08X)", m_frame, m_options.capturePath.c_str(), static_cast<unsigned>(hr));
                m_quit = true;
            }
            m_graphics.Present();
        }

        std::unique_ptr<Keyboard> m_keyboard;
        std::unique_ptr<Mouse> m_mouse;
        Keyboard::KeyboardStateTracker m_keyTracker;
        Mouse::ButtonStateTracker m_mouseTracker;

        Graphics m_graphics;
        ConstantBuffer<FrameConstants> m_frameCB;
        ConstantBuffer<ObjectConstants> m_objectCB;
        SkinnedModel m_playerModel;
        Environment m_environment;
        Player m_player;
        ThirdPersonCamera m_camera;
        int m_idleClip = -1;
        int m_runClip = -1;

        Options m_options;
        LARGE_INTEGER m_frequency = {};
        LARGE_INTEGER m_lastTime = {};
        int m_frame = 0;
        bool m_initialized = false;
        bool m_quit = false;
    };

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto* game = reinterpret_cast<Game*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        switch (msg)
        {
        case WM_CREATE:
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                             reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams));
            return 0;
        case WM_SIZE:
            if (game && wParam != SIZE_MINIMIZED)
                game->OnResize(LOWORD(lParam), HIWORD(lParam));
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
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int showCmd)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return 1;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const Options options = ParseOptions();
    auto game = std::make_unique<Game>();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SekiroDogWindow";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"SekiroDog", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                                nullptr, nullptr, instance, game.get());
    ShowWindow(hwnd, showCmd);

    GetClientRect(hwnd, &rc);
    if (!game->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top, options))
    {
        MessageBoxW(hwnd, L"初始化失败，详情见 SekiroDog.log", L"SekiroDog", MB_ICONERROR);
        return 1;
    }

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            game->Tick();
            if (game->WantsQuit())
            {
                DestroyWindow(hwnd);
                break;
            }
        }
    }

    CoUninitialize();
    return msg.message == WM_QUIT ? static_cast<int>(msg.wParam) : 0;
}
