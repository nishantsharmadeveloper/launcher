#pragma once
//
// Window.h
// Owns the launcher's borderless, dark-themed, rounded, always-on-top
// popup window: the search edit control and the owner-drawn results
// list. Purely UI - it has no knowledge of how results are produced;
// it asks its owner (Application) via callbacks and just renders what
// it's given.
//
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "SearchEngine.h"

class Window
{
public:
    // `onQueryChanged` is invoked on every keystroke with the current
    // query text; it returns the ranked results to display.
    // `onLaunchRequested` is invoked when the user activates a result
    // (Enter or double-click).
    // `onVisibilityChanged` is invoked with true/false whenever the
    // window is shown or hidden, so Application can pause/resume work.
    struct Callbacks
    {
        std::function<std::vector<SearchResult>(const std::wstring&)> onQueryChanged;
        std::function<void(const SearchResult&)> onLaunchRequested;
        std::function<void(bool)> onVisibilityChanged;
    };

    Window();
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Create(HINSTANCE instance, Callbacks callbacks);

    // Shows the launcher centered on the active monitor, clears the
    // search box, focuses it, and fades in.
    void ShowLauncher();

    // Fades out and hides the launcher.
    void HideLauncher();

    bool IsVisible() const { return m_visible; }

    HWND GetHandle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                              UINT_PTR subclassId, DWORD_PTR refData);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void OnCreate();
    void OnPaint();
    void OnQueryChanged();
    void OnEditKeyDown(WPARAM key);
    void MoveSelection(int delta);
    void ActivateSelection();
    void ApplyRoundedCorners();
    void PositionOnActiveMonitor();
    int HitTestResultRow(int y) const; // returns index or -1
    void RegisterWindowClass(HINSTANCE instance);

private:
    HWND m_hwnd = nullptr;
    HWND m_editControl = nullptr;
    HINSTANCE m_instance = nullptr;
    Callbacks m_callbacks;

    bool m_visible = false;
    std::vector<SearchResult> m_results;
    int m_selectedIndex = -1;

    // Layout metrics (logical/96-DPI px; scaled at draw time).
    static constexpr int kWindowWidth = 700;
    static constexpr int kWindowHeight = 420;
    static constexpr int kSearchBoxHeight = 60;
    static constexpr int kRowHeight = 56;
    static constexpr int kMaxVisibleRows = 6;
    static constexpr int kIconSize = 32;

    // Dark theme palette.
    static constexpr COLORREF kColorBackground = RGB(30, 30, 34);
    static constexpr COLORREF kColorSearchBg = RGB(40, 40, 45);
    static constexpr COLORREF kColorBorder = RGB(60, 60, 66);
    static constexpr COLORREF kColorText = RGB(235, 235, 240);
    static constexpr COLORREF kColorSubtext = RGB(150, 150, 158);
    static constexpr COLORREF kColorSelection = RGB(60, 90, 160);
    static constexpr COLORREF kColorPlaceholder = RGB(120, 120, 128);

    HBRUSH m_brushBackground = nullptr;
    HBRUSH m_brushSearchBg = nullptr;
    HBRUSH m_brushSelection = nullptr;
    HFONT m_fontSearch = nullptr;
    HFONT m_fontResultName = nullptr;
    HFONT m_fontResultPath = nullptr;

    static constexpr wchar_t kClassName[] = L"LauncherMainWindowClass";
};
