#include "KeyboardHook.h"

KeyboardHook* KeyboardHook::s_instance = nullptr;

KeyboardHook::KeyboardHook() = default;

KeyboardHook::~KeyboardHook()
{
    Uninstall();
}

bool KeyboardHook::Install(std::function<void()> onActivationChord)
{
    if (m_hook != nullptr)
    {
        return true; // already installed
    }

    m_callback = std::move(onActivationChord);
    s_instance = this;

    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    if (m_hook == nullptr)
    {
        s_instance = nullptr;
        return false;
    }
    return true;
}

void KeyboardHook::Uninstall()
{
    if (m_hook != nullptr)
    {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (s_instance == this)
    {
        s_instance = nullptr;
    }
}

LRESULT CALLBACK KeyboardHook::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && s_instance != nullptr)
    {
        const KBDLLHOOKSTRUCT* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (kb->vkCode == VK_TAB)
        {
            if (isDown) s_instance->m_tabDown = true;
            else if (isUp) s_instance->m_tabDown = false;
        }
        else if (kb->vkCode == 'P')
        {
            if (isDown) s_instance->m_pDown = true;
            else if (isUp) s_instance->m_pDown = false;
        }

        // Fire once per chord completion, on whichever keydown makes
        // both true, so it works regardless of press order (Tab-then-P
        // or P-then-Tab).
        if (isDown && s_instance->m_tabDown && s_instance->m_pDown)
        {
            if (s_instance->m_callback)
            {
                s_instance->m_callback();
            }
            // Swallow the 'P' keydown so "Tab+P" doesn't also type a
            // literal 'p' into whatever had focus, or move focus via
            // Tab, in the foreground application.
            return 1;
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
