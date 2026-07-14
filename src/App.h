#pragma once
//
// App.h
// The Application class is the composition root: it owns the Window,
// KeyboardHook, ProgramIndexer, SearchEngine, TrayIcon, and
// SettingsManager, and wires their callbacks together. main.cpp only
// creates an Application and runs the message loop.
//
#include <windows.h>
#include <memory>
#include "Window.h"
#include "KeyboardHook.h"
#include "ProgramIndexer.h"
#include "SearchEngine.h"
#include "TrayIcon.h"
#include "Settings.h"

class Application
{
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Initializes every subsystem: registers the message-only owner
    // window used for the tray icon, creates the launcher Window,
    // installs the keyboard hook, and kicks off background indexing.
    // Returns false if any required subsystem fails to initialize.
    bool Initialize(HINSTANCE instance);

    // Runs the standard Win32 message loop until WM_QUIT. Returns the
    // process exit code.
    int Run();

private:
    static LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleHiddenWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void RegisterHiddenWindowClass(HINSTANCE instance);
    bool CreateHiddenOwnerWindow(HINSTANCE instance);

    // Callback wiring.
    void OnHotkeyActivated();
    std::vector<SearchResult> OnQueryChanged(const std::wstring& query);
    void OnLaunchRequested(const SearchResult& result);
    void OnRefreshIndexRequested();
    void OnToggleStartup(bool enable);
    void OnAboutRequested();
    void OnExitRequested();

private:
    HINSTANCE m_instance = nullptr;
    HWND m_hiddenWnd = nullptr;

    std::unique_ptr<Window> m_window;
    std::unique_ptr<KeyboardHook> m_keyboardHook;
    std::unique_ptr<ProgramIndexer> m_indexer;
    std::unique_ptr<SearchEngine> m_searchEngine;
    std::unique_ptr<TrayIcon> m_trayIcon;
    std::unique_ptr<SettingsManager> m_settings;

    // WM_APP+2: posted from the (background-thread) indexer completion
    // callback so we finish on the UI thread instead of touching UI
    // state from a worker thread.
    static constexpr UINT WM_APP_INDEX_COMPLETE = WM_APP + 2;
    // Posted by the low-level keyboard hook callback (which must return
    // immediately) to defer the actual "show window" work onto the
    // normal message queue.
    static constexpr UINT WM_APP_HOTKEY = WM_APP + 3;

    static constexpr wchar_t kHiddenClassName[] = L"LauncherHiddenOwnerWindowClass";
};
