#include "App.h"
#include "Utils.h"
#include "../resources/resource.h"
#include <shellapi.h>

Application::Application() = default;
Application::~Application()
{
    if (m_keyboardHook) m_keyboardHook->Uninstall();
    if (m_trayIcon) m_trayIcon->Destroy();
    if (m_indexer) m_indexer->WaitForIndexing();
}

void Application::RegisterHiddenWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = HiddenWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kHiddenClassName;
    RegisterClassExW(&wc);
}

bool Application::CreateHiddenOwnerWindow(HINSTANCE instance)
{
    RegisterHiddenWindowClass(instance);

    // This window is never shown. It exists purely as:
    //  (a) the owner HWND for the tray icon (Shell_NotifyIcon needs one), and
    //  (b) a stable message-only pump target we can PostMessage to from
    //      background threads / the keyboard hook.
    m_hiddenWnd = CreateWindowExW(0, kHiddenClassName, L"LauncherHiddenOwner", WS_OVERLAPPED,
                                   0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this);
    return m_hiddenWnd != nullptr;
}

LRESULT CALLBACK Application::HiddenWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Application* self = nullptr;
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Application*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
    {
        return self->HandleHiddenWindowMessage(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Application::HandleHiddenWindowMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (m_trayIcon && m_trayIcon->HandleMessage(hwnd, message, wParam, lParam))
    {
        return 0;
    }

    switch (message)
    {
    case WM_APP_HOTKEY:
        if (m_window)
        {
            if (m_window->IsVisible()) m_window->HideLauncher();
            else m_window->ShowLauncher();
        }
        return 0;

    case WM_APP_INDEX_COMPLETE:
        // Nothing UI-visible to refresh proactively - the next keystroke
        // will pick up the new index snapshot automatically. This
        // message exists as an extension point (e.g. a "index ready"
        // tray balloon) and to keep indexing completion marshalled onto
        // the UI thread rather than a worker thread.
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

bool Application::Initialize(HINSTANCE instance)
{
    m_instance = instance;

    m_settings = std::make_unique<SettingsManager>();
    m_indexer = std::make_unique<ProgramIndexer>();
    m_searchEngine = std::make_unique<SearchEngine>();
    m_window = std::make_unique<Window>();
    m_keyboardHook = std::make_unique<KeyboardHook>();
    m_trayIcon = std::make_unique<TrayIcon>();

    if (!CreateHiddenOwnerWindow(instance))
    {
        return false;
    }

    Window::Callbacks windowCallbacks;
    windowCallbacks.onQueryChanged = [this](const std::wstring& query) { return OnQueryChanged(query); };
    windowCallbacks.onLaunchRequested = [this](const SearchResult& result) { OnLaunchRequested(result); };
    if (!m_window->Create(instance, windowCallbacks))
    {
        return false;
    }

    TrayIcon::Callbacks trayCallbacks;
    trayCallbacks.onShowLauncher = [this]() { if (m_window) m_window->ShowLauncher(); };
    trayCallbacks.onRefreshIndex = [this]() { OnRefreshIndexRequested(); };
    trayCallbacks.onToggleStartup = [this](bool enable) { OnToggleStartup(enable); };
    trayCallbacks.onAbout = [this]() { OnAboutRequested(); };
    trayCallbacks.onExit = [this]() { OnExitRequested(); };

    if (!m_trayIcon->Create(m_hiddenWnd, instance, IDI_APP_ICON, L"Launcher", trayCallbacks))
    {
        return false;
    }
    m_trayIcon->SetStartupChecked(m_settings->IsLaunchOnStartupEnabled());

    if (!m_keyboardHook->Install([this]()
    {
        // Low-level hook callbacks run on this thread's hook chain and
        // must return quickly - defer the real work to the message queue.
        PostMessageW(m_hiddenWnd, WM_APP_HOTKEY, 0, 0);
    }))
    {
        return false;
    }

    HWND hiddenWnd = m_hiddenWnd;
    m_indexer->BeginIndexAsync([hiddenWnd]()
    {
        PostMessageW(hiddenWnd, WM_APP_INDEX_COMPLETE, 0, 0);
    });

    return true;
}

int Application::Run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

std::vector<SearchResult> Application::OnQueryChanged(const std::wstring& query)
{
    if (!m_searchEngine || !m_indexer) return {};
    std::vector<AppEntry> snapshot = m_indexer->GetSnapshot();
    return m_searchEngine->Search(query, snapshot, m_indexer->GetIconCache());
}

void Application::OnLaunchRequested(const SearchResult& result)
{
    if (m_searchEngine)
    {
        m_searchEngine->RecordLaunch(result.targetPath);
    }

    // SEE_MASK_FLAG_NO_UI suppresses the blocking "can't find file"
    // dialog in favor of a silent failure, since a fire-and-forget
    // launcher shouldn't block on modal error UI.
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = result.targetPath.c_str();
    info.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&info);
}

void Application::OnRefreshIndexRequested()
{
    if (!m_indexer) return;
    HWND hiddenWnd = m_hiddenWnd;
    m_indexer->BeginIndexAsync([hiddenWnd]()
    {
        PostMessageW(hiddenWnd, WM_APP_INDEX_COMPLETE, 0, 0);
    });
}

void Application::OnToggleStartup(bool enable)
{
    if (!m_settings || !m_trayIcon) return;
    if (m_settings->SetLaunchOnStartup(enable))
    {
        m_trayIcon->SetStartupChecked(enable);
    }
}

void Application::OnAboutRequested()
{
    MessageBoxW(nullptr, L"Launcher\nA fast, native Windows app launcher.\nPress Tab + P to open.",
                L"About Launcher", MB_OK | MB_ICONINFORMATION);
}

void Application::OnExitRequested()
{
    PostQuitMessage(0);
}
