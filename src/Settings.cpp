#include "Settings.h"

SettingsManager::SettingsManager() = default;

std::wstring SettingsManager::GetExecutablePath() const
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

bool SettingsManager::IsLaunchOnStartupEnabled() const
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return false;
    }

    wchar_t value[MAX_PATH]{};
    DWORD size = sizeof(value);
    DWORD type = 0;
    bool exists = (RegQueryValueExW(key, kRunValueName, nullptr, &type, reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS);
    RegCloseKey(key);
    return exists;
}

bool SettingsManager::SetLaunchOnStartup(bool enabled)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
    {
        return false;
    }

    LONG result;
    if (enabled)
    {
        std::wstring exePath = GetExecutablePath();
        // Quote the path so it launches correctly even if it contains
        // spaces (e.g. "C:\Program Files\Launcher\Launcher.exe").
        std::wstring quoted = L"\"" + exePath + L"\"";
        result = RegSetValueExW(key, kRunValueName, 0, REG_SZ,
                                 reinterpret_cast<const BYTE*>(quoted.c_str()),
                                 static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
    }
    else
    {
        result = RegDeleteValueW(key, kRunValueName);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS; // already absent is fine
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void SettingsManager::SetIntPreference(const std::wstring& name, int value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kAppKeyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
    {
        return;
    }
    DWORD dwValue = static_cast<DWORD>(value);
    RegSetValueExW(key, name.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof(dwValue));
    RegCloseKey(key);
}

int SettingsManager::GetIntPreference(const std::wstring& name, int defaultValue) const
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS)
    {
        return defaultValue;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    LONG result = RegQueryValueExW(key, name.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS || type != REG_DWORD) return defaultValue;
    return static_cast<int>(value);
}
