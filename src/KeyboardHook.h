#pragma once
//
// KeyboardHook.h
// Wraps a WH_KEYBOARD_LL low-level keyboard hook that detects the
// Tab+P chord (both keys down, in either press order) and invokes a
// callback on match. Low-level hooks run on the thread that installed
// them and must return quickly - the actual "show window" work is
// dispatched via PostMessage from the callback rather than done inline.
//
#include <windows.h>
#include <functional>

class KeyboardHook
{
public:
    KeyboardHook();
    ~KeyboardHook();

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    // Installs the hook on the calling thread. The calling thread must
    // run a Windows message loop for the hook to receive events - this
    // is true of our main thread, which owns the hidden message-only
    // pump alongside the launcher window.
    bool Install(std::function<void()> onActivationChord);

    // Removes the hook. Safe to call even if Install() was never called
    // or already failed.
    void Uninstall();

    bool IsInstalled() const { return m_hook != nullptr; }

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    HHOOK m_hook = nullptr;
    std::function<void()> m_callback;

    // The low-level hook procedure must be a free/static function
    // (Windows calls it directly), so we stash the single active
    // instance here to route the callback. Only one KeyboardHook is
    // ever installed at a time in this application.
    static KeyboardHook* s_instance;

    bool m_tabDown = false;
    bool m_pDown = false;
};
