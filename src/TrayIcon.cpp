#include "TrayIcon.h"
#include <shellapi.h>
#include <windowsx.h>

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon()
{
    Destroy();
}

bool TrayIcon::Create(HWND owner, HINSTANCE instance, int iconResourceId, const std::wstring& tooltip, Callbacks callbacks)
{
    m_callbacks = std::move(callbacks);

    ZeroMemory(&m_iconData, sizeof(m_iconData));
    m_iconData.cbSize = sizeof(NOTIFYICONDATAW);
    m_iconData.hWnd = owner;
    m_iconData.uID = 1;
    m_iconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_iconData.uCallbackMessage = WM_APP_TRAYICON;
    m_iconData.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(iconResourceId), IMAGE_ICON,
                                                       GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wcsncpy_s(m_iconData.szTip, tooltip.c_str(), _TRUNCATE);

    m_created = Shell_NotifyIconW(NIM_ADD, &m_iconData) != FALSE;

    // Ask for the modern callback version so we get proper mouse
    // coordinates (WM_APP_TRAYICON + LOWORD/HIWORD) rather than legacy semantics.
    m_iconData.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_iconData);

    return m_created;
}

void TrayIcon::Destroy()
{
    if (m_created)
    {
        Shell_NotifyIconW(NIM_DELETE, &m_iconData);
        m_created = false;
    }
    if (m_iconData.hIcon)
    {
        DestroyIcon(m_iconData.hIcon);
        m_iconData.hIcon = nullptr;
    }
}

void TrayIcon::ShowContextMenu(HWND hwnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, MenuId_Show, L"Show Launcher");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuId_Refresh, L"Refresh App Index");
    AppendMenuW(menu, MF_STRING | (m_startupChecked ? MF_CHECKED : MF_UNCHECKED), MenuId_Startup, L"Launch on Startup");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuId_About, L"About");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MenuId_Exit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);

    // Required so the menu dismisses correctly if the user clicks
    // elsewhere; see TrackPopupMenu docs ("owner window must be the
    // foreground window").
    SetForegroundWindow(hwnd);
    UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                    cursor.x, cursor.y, 0, hwnd, nullptr);
    PostMessage(hwnd, WM_NULL, 0, 0); // work around a menu-not-closing edge case per Microsoft guidance
    DestroyMenu(menu);

    switch (selected)
    {
    case MenuId_Show:
        if (m_callbacks.onShowLauncher) m_callbacks.onShowLauncher();
        break;
    case MenuId_Refresh:
        if (m_callbacks.onRefreshIndex) m_callbacks.onRefreshIndex();
        break;
    case MenuId_Startup:
        if (m_callbacks.onToggleStartup) m_callbacks.onToggleStartup(!m_startupChecked);
        break;
    case MenuId_About:
        if (m_callbacks.onAbout) m_callbacks.onAbout();
        break;
    case MenuId_Exit:
        if (m_callbacks.onExit) m_callbacks.onExit();
        break;
    default:
        break; // menu dismissed without a selection
    }
}

bool TrayIcon::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_APP_TRAYICON)
    {
        UINT eventMsg = LOWORD(lParam);
        if (eventMsg == WM_LBUTTONUP || eventMsg == WM_LBUTTONDBLCLK)
        {
            if (m_callbacks.onShowLauncher) m_callbacks.onShowLauncher();
            return true;
        }
        if (eventMsg == WM_RBUTTONUP || eventMsg == WM_CONTEXTMENU)
        {
            ShowContextMenu(hwnd);
            return true;
        }
    }
    return false;
}
