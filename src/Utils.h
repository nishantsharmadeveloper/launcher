#pragma once
//
// Utils.h
// Small stateless helper functions shared across the launcher:
// string conversion, case-insensitive search helpers, DPI helpers,
// and a very small fuzzy-matching routine used by SearchEngine.
//
#include <windows.h>
#include <string>
#include <vector>

namespace Utils
{
    // Converts a UTF-8 std::string to a UTF-16 std::wstring using the
    // Windows API (MultiByteToWideChar). Returns an empty wstring on
    // failure rather than throwing, since this is used in hot paths.
    std::wstring Utf8ToWide(const std::string& utf8);

    // Converts a UTF-16 std::wstring to a UTF-8 std::string.
    std::string WideToUtf8(const std::wstring& wide);

    // Case-insensitive substring search. Returns true if `haystack`
    // contains `needle`, ignoring case. Both strings are treated as
    // UTF-16 (wchar_t) since that's the native Windows string type.
    bool ContainsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle);

    // Returns true if `haystack` starts with `needle`, ignoring case.
    bool StartsWithCaseInsensitive(const std::wstring& haystack, const std::wstring& needle);

    // Returns true if `haystack` equals `needle` exactly, ignoring case.
    bool EqualsCaseInsensitive(const std::wstring& haystack, const std::wstring& needle);

    // Lowercases a wide string using the current locale-independent
    // Win32 API (CharLowerW), safe for search normalization.
    std::wstring ToLower(const std::wstring& input);

    // Very small subsequence-based fuzzy matcher: returns true if every
    // character in `needle` appears in `haystack` in order (not
    // necessarily contiguous). Used as the last-resort ranking tier.
    // `outScore` is filled with a rough quality score (lower = better)
    // based on how spread out the matched characters are.
    bool FuzzyMatch(const std::wstring& haystack, const std::wstring& needle, int& outScore);

    // Extracts the file name (without directory) from a full path.
    std::wstring GetFileNameWithoutExtension(const std::wstring& path);

    // Returns true if the given path has one of the extensions we
    // consider "launchable" (.exe, .lnk).
    bool HasLaunchableExtension(const std::wstring& path);

    // Returns the DPI scale factor (96 DPI == 1.0) for the monitor
    // nearest to the given window. Used to scale window/layout metrics.
    float GetDpiScaleForWindow(HWND hwnd);

    // Returns the monitor work-area rectangle that contains the current
    // cursor position (i.e. "the monitor the user is currently on").
    RECT GetActiveMonitorWorkArea();
}
