//
// main.cpp
// Process entry point. Keeps main() minimal: create the Application,
// initialize it, run the message loop. All real logic lives in App.cpp
// and the subsystem classes it owns.
//
#include <windows.h>
#include "App.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    // Prevent a second instance from installing a duplicate keyboard
    // hook / tray icon. If Launcher is already running, just exit.
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\LauncherSingleInstanceMutex");
    if (singleInstanceMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
        return 0;
    }

    Application app;
    if (!app.Initialize(hInstance))
    {
        MessageBoxW(nullptr, L"Launcher failed to initialize.", L"Launcher", MB_OK | MB_ICONERROR);
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    int exitCode = app.Run();

    CloseHandle(singleInstanceMutex);
    return exitCode;
}
