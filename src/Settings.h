#pragma once
//
// Settings.h
// Small wrapper around the registry keys the launcher persists:
//   - Launch-on-startup, stored the standard way under
//     HKCU\Software\Microsoft\Windows\CurrentVersion\Run
//   - App-specific preferences under
//     HKCU\Software\Launcher
//
#include <windows.h>
#include <string>

class SettingsManager
{
public:
    SettingsManager();

    // Returns whether "launch on Windows startup" is currently enabled,
    // determined by checking for our Run-key registry value.
    bool IsLaunchOnStartupEnabled() const;

    // Enables or disables launch-on-startup by writing/removing our
    // value under the Run key, pointed at the current executable path.
    // Returns true on success.
    bool SetLaunchOnStartup(bool enabled);

    // Generic small-integer preference storage under HKCU\Software\Launcher,
    // used for things like "last window position" in future extensions.
    void SetIntPreference(const std::wstring& name, int value);
    int GetIntPreference(const std::wstring& name, int defaultValue) const;

private:
    std::wstring GetExecutablePath() const;

private:
    static constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr wchar_t kAppKeyPath[] = L"Software\\Launcher";
    static constexpr wchar_t kRunValueName[] = L"Launcher";
};
