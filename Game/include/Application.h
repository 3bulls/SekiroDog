#pragma once
#include <memory>
#include <string>
#include <windows.h>
#include <Keyboard.h>
#include <Mouse.h>
#include "Game.h"
#include "Graphics.h"

// 调试用命令行参数：
//   --auto-run              模拟一直按住 W
//   --camera-yaw <度>       初始相机朝向
//   --capture <png> <帧号>  在指定帧截图后退出（固定 60 FPS 步长）
struct AppOptions
{
    GameOptions game;
    std::wstring capturePath;
    int captureFrame = -1;
};

// 应用程序：窗口、消息循环、图形设备、键鼠输入与帧计时；游戏逻辑交给 Game
class Application
{
public:
    static constexpr int ClientWidth = 1920;
    static constexpr int ClientHeight = 1080;

    Application();

    int Run(HINSTANCE instance, int showCmd);

private:
    bool Initialize(HINSTANCE instance, int showCmd);
    void Tick();
    void Finalize();

    bool CreateMainWindow(HINSTANCE instance, int showCmd);
    void UpdateMouseLock();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    AppOptions m_options;
    HWND m_hwnd = nullptr;
    Graphics m_graphics;
    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;
    DirectX::Keyboard::KeyboardStateTracker m_keyTracker;
    DirectX::Mouse::ButtonStateTracker m_mouseTracker;
    Game m_game;

    LARGE_INTEGER m_frequency = {};
    LARGE_INTEGER m_lastTime = {};
    int m_frame = 0;
    bool m_initialized = false;
    bool m_quit = false;
};
