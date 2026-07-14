#include "Utils.h"
#include <shellapi.h>
#include <algorithm>
#include <cwctype>

namespace Utils
{
    std::wstring Utf8ToWide(const std::string& utf8)
    {
        if (utf8.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        if (size <= 0) return std::wstring();
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &result[0], size);
        return result;
    }

    std::string WideToUtf8(const std::wstring& wide)
    {
        if (wide.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return std::string();
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), &result[0], size, nullptr, nullptr);
        return result;
    }

    std::wstring ToLower(const std::wstring& input)
    {
        std::wstring result = input;
        // CharLowerW modifies the buffer in place and understands
        // locale-specific casing better than a naive tolower loop.
        CharLowerW(&result[0]);
        return result;
    }

    bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle)
    {
        if (needle.empty()) return true;
        std::wstring h = ToLower(haystack);
        std::wstring n = ToLower(needle);
        return h.find(n) != std::wstring::npos;
    }

    bool StartsWithCaseInsensitive(const std::wstring& haystack, const std::wstring& needle)
    {
        if (needle.size() > haystack.size()) return false;
        std::wstring h = ToLower(haystack.substr(0, needle.size()));
        std::wstring n = ToLower(needle);
        return h == n;
    }

    bool EqualsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle)
    {
        if (haystack.size() != needle.size()) return false;
        return ToLower(haystack) == ToLower(needle);
    }

    bool FuzzyMatch(const std::wstring& haystack, const std::wstring& needle, int& outScore)
    {
        if (needle.empty())
        {
            outScore = 0;
            return true;
        }

        std::wstring h = ToLower(haystack);
        std::wstring n = ToLower(needle);

        size_t hi = 0;
        size_t matchedStart = std::wstring::npos;
        size_t lastMatch = 0;
        size_t matches = 0;

        for (size_t ni = 0; ni < n.size(); ++ni)
        {
            bool found = false;
            for (; hi < h.size(); ++hi)
            {
                if (h[hi] == n[ni])
                {
                    if (matchedStart == std::wstring::npos) matchedStart = hi;
                    lastMatch = hi;
                    ++hi; // don't reuse this character for the next needle char
                    found = true;
                    ++matches;
                    break;
                }
            }
            if (!found)
            {
                return false;
            }
        }

        // Score = spread of the match (smaller spread == tighter, better
        // match) plus a penalty for how far into the string the match
        // starts, so "chr" matching near the front of "chrome" beats a
        // scattered match deep inside a long string.
        size_t spread = (matchedStart == std::wstring::npos) ? 0 : (lastMatch - matchedStart);
        outScore = static_cast<int>(spread * 2 + matchedStart);
        return true;
    }

    std::wstring GetFileNameWithoutExtension(const std::wstring& path)
    {
        size_t slash = path.find_last_of(L"\\/");
        std::wstring fileName = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
        size_t dot = fileName.find_last_of(L'.');
        if (dot != std::wstring::npos)
        {
            fileName = fileName.substr(0, dot);
        }
        return fileName;
    }

    bool HasLaunchableExtension(const std::wstring& path)
    {
        std::wstring lower = ToLower(path);
        return (lower.size() >= 4 &&
                (lower.compare(lower.size() - 4, 4, L".exe") == 0 ||
                 lower.compare(lower.size() - 4, 4, L".lnk") == 0));
    }

    float GetDpiScaleForWindow(HWND hwnd)
    {
        // GetDpiForWindow is available on Windows 10 1607+, which is a
        // reasonable minimum target for a modern launcher.
        UINT dpi = GetDpiForWindow(hwnd ? hwnd : GetDesktopWindow());
        if (dpi == 0) dpi = 96;
        return static_cast<float>(dpi) / 96.0f;
    }

    RECT GetActiveMonitorWorkArea()
    {
        POINT cursor{};
        GetCursorPos(&cursor);
        HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);

        MONITORINFO info{};
        info.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoW(monitor, &info))
        {
            return info.rcWork;
        }

        RECT fallback{ 0, 0, 1920, 1080 };
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &fallback, 0);
        return fallback;
    }
}
