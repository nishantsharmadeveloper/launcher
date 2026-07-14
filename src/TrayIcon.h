#pragma once
//
// TrayIcon.h
// Wraps the notification-area (system tray) icon and its right-click
// context menu: Show Launcher, Refresh App Index, Launch on Startup,
// About, Exit. Owns no window itself - it attaches to a message-only
// or hidden HWND supplied by the caller, which must forward the tray
// callback message (WM_APP_TRAYICON) to HandleMessage().
//
#include <windows.h>
#include <functional>
#include <string>

class TrayIcon
{
public:
    static constexpr UINT WM_APP_TRAYICON = WM_APP + 1;

    struct Callbacks
    {
        std::function<void()> onShowLauncher;
        std::function<void()> onRefreshIndex;
        std::function<void(bool)> onToggleStartup; // receives the *new* desired state
        std::function<void()> onAbout;
        std::function<void()> onExit;
    };

    TrayIcon();
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Creates the tray icon attached to `owner`. `iconResourceId` is the
    // resource ID of the .ico to display (see resources/resource.h).
    bool Create(HWND owner, HINSTANCE instance, int iconResourceId, const std::wstring& tooltip, Callbacks callbacks);

    void Destroy();

    // Updates the checked state shown next to "Launch on Startup".
    void SetStartupChecked(bool checked) { m_startupChecked = checked; }

    // Must be called from the owner window's WndProc when it receives
    // WM_APP_TRAYICON (mouse/keyboard events on the tray icon) or
    // WM_COMMAND for the context menu item IDs this class defines.
    // Returns true if the message was handled.
    bool HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    void ShowContextMenu(HWND hwnd);

private:
    NOTIFYICONDATAW m_iconData{};
    bool m_created = false;
    bool m_startupChecked = false;
    Callbacks m_callbacks;

    enum MenuId
    {
        MenuId_Show = 1001,
        MenuId_Refresh = 1002,
        MenuId_Startup = 1003,
        MenuId_About = 1004,
        MenuId_Exit = 1005,
    };
};
