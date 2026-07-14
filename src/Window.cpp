#include "Window.h"
#include "Utils.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace
{
    // DWM_WINDOW_CORNER_PREFERENCE isn't in older SDK headers; define
    // locally so this compiles against SDKs that predate Windows 11.
    constexpr DWORD kDwmwaWindowCornerPreference = 33;
    constexpr int kDwmwcpRound = 2;

    constexpr UINT_PTR kEditSubclassId = 1;
}

Window::Window() = default;

Window::~Window()
{
    if (m_brushBackground) DeleteObject(m_brushBackground);
    if (m_brushSearchBg) DeleteObject(m_brushSearchBg);
    if (m_brushSelection) DeleteObject(m_brushSelection);
    if (m_fontSearch) DeleteObject(m_fontSearch);
    if (m_fontResultName) DeleteObject(m_fontResultName);
    if (m_fontResultPath) DeleteObject(m_fontResultPath);
}

void Window::RegisterWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    // CS_DROPSHADOW gives popup windows a subtle native drop shadow
    // without us having to hand-roll one.
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // we paint the background ourselves
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

bool Window::Create(HINSTANCE instance, Callbacks callbacks)
{
    m_instance = instance;
    m_callbacks = std::move(callbacks);

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    RegisterWindowClass(instance);

    m_brushBackground = CreateSolidBrush(kColorBackground);
    m_brushSearchBg = CreateSolidBrush(kColorSearchBg);
    m_brushSelection = CreateSolidBrush(kColorSelection);

    m_fontSearch = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_fontResultName = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_fontResultPath = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // WS_EX_LAYERED enables the AnimateWindow fade; WS_EX_TOPMOST keeps
    // it above other windows while visible; WS_EX_TOOLWINDOW keeps it
    // out of the taskbar and Alt+Tab, matching Spotlight/Raycast behavior.
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    DWORD style = WS_POPUP;

    m_hwnd = CreateWindowExW(exStyle, kClassName, L"Launcher", style,
                              0, 0, kWindowWidth, kWindowHeight,
                              nullptr, nullptr, instance, this);
    if (!m_hwnd) return false;

    ApplyRoundedCorners();
    return true;
}

void Window::ApplyRoundedCorners()
{
    // Prefer the native DWM rounded-corner API (Windows 11+); silently
    // no-ops on older systems that don't recognize the attribute.
    BOOL preference = kDwmwcpRound;
    DwmSetWindowAttribute(m_hwnd, kDwmwaWindowCornerPreference, &preference, sizeof(preference));

    // Fallback region-based rounding for older Windows versions, so the
    // window still looks rounded even without DWM support.
    HRGN region = CreateRoundRectRgn(0, 0, kWindowWidth + 1, kWindowHeight + 1, 16, 16);
    SetWindowRgn(m_hwnd, region, TRUE); // ownership transfers to the window
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window* self = nullptr;
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self)
    {
        return self->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        OnCreate();
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_ERASEBKGND:
        return 1; // avoid flicker; OnPaint fully repaints the client area

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_CHANGE && reinterpret_cast<HWND>(lParam) == m_editControl)
        {
            OnQueryChanged();
        }
        return 0;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        int y = GET_Y_LPARAM(lParam);
        int index = HitTestResultRow(y);
        if (index >= 0)
        {
            m_selectedIndex = index;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            if (message == WM_LBUTTONDBLCLK)
            {
                ActivateSelection();
            }
        }
        return 0;
    }

    case WM_ACTIVATE:
        // Hide when the launcher loses activation (click-outside / alt-tab
        // away), matching Spotlight/Raycast "focus-follows-dismiss" behavior.
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            HideLauncher();
        }
        return 0;

    case WM_CLOSE:
        // "Close button hides instead of exiting" - there's no visible
        // title bar/close button on this borderless window, but any
        // WM_CLOSE (e.g. from Alt+F4) is treated the same way.
        HideLauncher();
        return 0;

    case WM_DESTROY:
        return 0;

    default:
        return DefWindowProcW(m_hwnd, message, wParam, lParam);
    }
}

void Window::OnCreate()
{
    RECT client{};
    GetClientRect(m_hwnd, &client);

    m_editControl = CreateWindowExW(0, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
                                     20, 14, kWindowWidth - 40, kSearchBoxHeight - 28,
                                     m_hwnd, nullptr, m_instance, nullptr);
    SendMessageW(m_editControl, WM_SETFONT, reinterpret_cast<WPARAM>(m_fontSearch), TRUE);
    SendMessageW(m_editControl, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search apps..."));

    // Subclass so we can intercept Up/Down/Enter/Esc while the edit
    // control has focus (standard EDIT controls consume most keys).
    SetWindowSubclass(m_editControl, EditSubclassProc, kEditSubclassId, reinterpret_cast<DWORD_PTR>(this));
}

LRESULT CALLBACK Window::EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                           UINT_PTR /*subclassId*/, DWORD_PTR refData)
{
    Window* self = reinterpret_cast<Window*>(refData);

    if (message == WM_KEYDOWN)
    {
        switch (wParam)
        {
        case VK_UP:
        case VK_DOWN:
        case VK_RETURN:
        case VK_ESCAPE:
        case VK_TAB:
            self->OnEditKeyDown(wParam);
            return 0; // handled - don't let the edit control process navigation keys
        default:
            break;
        }
    }
    else if (message == WM_GETDLGCODE)
    {
        // Ensure VK_RETURN/VK_TAB reach WM_KEYDOWN above rather than being
        // swallowed by dialog-navigation semantics.
        return DLGC_WANTALLKEYS;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void Window::OnEditKeyDown(WPARAM key)
{
    switch (key)
    {
    case VK_UP:
        MoveSelection(-1);
        break;
    case VK_DOWN:
    case VK_TAB: // "Tab: move through search results (optional)"
        MoveSelection(1);
        break;
    case VK_RETURN:
        ActivateSelection();
        break;
    case VK_ESCAPE:
        HideLauncher();
        break;
    default:
        break;
    }
}

void Window::MoveSelection(int delta)
{
    if (m_results.empty()) return;
    int count = static_cast<int>(m_results.size());
    m_selectedIndex = ((m_selectedIndex + delta) % count + count) % count;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Window::ActivateSelection()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_results.size())) return;
    if (m_callbacks.onLaunchRequested)
    {
        m_callbacks.onLaunchRequested(m_results[m_selectedIndex]);
    }
    HideLauncher();
}

void Window::OnQueryChanged()
{
    wchar_t buffer[512]{};
    GetWindowTextW(m_editControl, buffer, 512);
    std::wstring query = buffer;

    if (m_callbacks.onQueryChanged)
    {
        m_results = m_callbacks.onQueryChanged(query);
    }
    else
    {
        m_results.clear();
    }

    m_selectedIndex = m_results.empty() ? -1 : 0;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

int Window::HitTestResultRow(int y) const
{
    int listTop = kSearchBoxHeight;
    if (y < listTop) return -1;

    int index = (y - listTop) / kRowHeight;
    if (index < 0 || index >= static_cast<int>(m_results.size())) return -1;
    return index;
}

void Window::OnPaint()
{
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(m_hwnd, &ps);

    RECT client{};
    GetClientRect(m_hwnd, &client);

    // Double-buffer to avoid flicker from the icon/text-heavy list redraw.
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(memDC, memBitmap));

    FillRect(memDC, &client, m_brushBackground);

    // Search box background + separator line.
    RECT searchRect{ 0, 0, client.right, kSearchBoxHeight };
    FillRect(memDC, &searchRect, m_brushBackground);
    HPEN borderPen = CreatePen(PS_SOLID, 1, kColorBorder);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, borderPen));
    MoveToEx(memDC, 0, kSearchBoxHeight, nullptr);
    LineTo(memDC, client.right, kSearchBoxHeight);

    // Results.
    SetBkMode(memDC, TRANSPARENT);
    int y = kSearchBoxHeight;
    int visibleRows = min(static_cast<int>(m_results.size()), kMaxVisibleRows);

    for (int i = 0; i < visibleRows; ++i)
    {
        const SearchResult& result = m_results[i];
        RECT rowRect{ 0, y, client.right, y + kRowHeight };

        if (i == m_selectedIndex)
        {
            FillRect(memDC, &rowRect, m_brushSelection);
        }

        if (result.icon)
        {
            int iconY = y + (kRowHeight - kIconSize) / 2;
            DrawIconEx(memDC, 16, iconY, result.icon, kIconSize, kIconSize, 0, nullptr, DI_NORMAL);
        }

        RECT nameRect{ 16 + kIconSize + 12, y + 6, client.right - 16, y + 6 + 20 };
        SelectObject(memDC, m_fontResultName);
        SetTextColor(memDC, kColorText);
        DrawTextW(memDC, result.displayName.c_str(), -1, &nameRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);

        RECT pathRect{ 16 + kIconSize + 12, y + 28, client.right - 16, y + 28 + 18 };
        SelectObject(memDC, m_fontResultPath);
        SetTextColor(memDC, kColorSubtext);
        DrawTextW(memDC, result.subtitlePath.c_str(), -1, &pathRect, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS | DT_PATH_ELLIPSIS);

        y += kRowHeight;
    }

    if (m_results.empty())
    {
        wchar_t buffer[512]{};
        GetWindowTextW(m_editControl, buffer, 512);
        if (buffer[0] != L'\0')
        {
            RECT emptyRect{ 0, kSearchBoxHeight, client.right, client.bottom };
            SelectObject(memDC, m_fontResultPath);
            SetTextColor(memDC, kColorPlaceholder);
            DrawTextW(memDC, L"No matching applications", -1, &emptyRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }
    }

    BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(m_hwnd, &ps);
}

void Window::PositionOnActiveMonitor()
{
    // The window is a fixed 700x420 logical size per spec; we only
    // reposition it here (not resize), so the rounded-corner region and
    // child control layout established at creation time stay valid.
    RECT workArea = Utils::GetActiveMonitorWorkArea();

    int monitorWidth = workArea.right - workArea.left;
    int monitorHeight = workArea.bottom - workArea.top;

    int x = workArea.left + (monitorWidth - kWindowWidth) / 2;
    int y = workArea.top + (monitorHeight - kWindowHeight) / 3; // slightly above vertical center, Spotlight-style

    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, kWindowWidth, kWindowHeight, SWP_NOACTIVATE | SWP_NOSIZE);
}

void Window::ShowLauncher()
{
    PositionOnActiveMonitor();

    SetWindowTextW(m_editControl, L"");
    m_results.clear();
    m_selectedIndex = -1;

    ShowWindow(m_hwnd, SW_SHOW);
    // AnimateWindow + AW_BLEND gives a smooth native fade-in without us
    // hand-rolling a timer-driven alpha ramp.
    AnimateWindow(m_hwnd, 120, AW_BLEND);

    SetForegroundWindow(m_hwnd);
    SetFocus(m_editControl);

    m_visible = true;
    if (m_callbacks.onVisibilityChanged) m_callbacks.onVisibilityChanged(true);
}

void Window::HideLauncher()
{
    if (!m_visible) return;

    AnimateWindow(m_hwnd, 100, AW_BLEND | AW_HIDE);
    m_visible = false;

    if (m_callbacks.onVisibilityChanged) m_callbacks.onVisibilityChanged(false);
}
