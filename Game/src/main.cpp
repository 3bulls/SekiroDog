#include <windows.h>
#include "Application.h"

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int showCmd)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return 1;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int exitCode = 0;
    {
        Application app;
        exitCode = app.Run(instance, showCmd);
    }
    CoUninitialize();
    return exitCode;
}
