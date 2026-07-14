#include "ProgramIndexer.h"
#include "Utils.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <algorithm>
#include <unordered_set>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    // A small denylist of filename fragments we never want to surface,
    // even if they technically match "*.exe" - installers, uninstallers,
    // helper/updater binaries clutter results without being something
    // the user meant to launch.
    bool IsNoiseExecutable(const std::wstring& lowerName)
    {
        static const wchar_t* kNoise[] = {
            L"uninstall", L"uninst", L"setup", L"unins0", L"update.exe",
            L"crashpad", L"helper.exe", L"installer.exe"
        };
        for (const wchar_t* fragment : kNoise)
        {
            if (lowerName.find(fragment) != std::wstring::npos) return true;
        }
        return false;
    }

    // Known non-app folder names to skip while walking Program Files,
    // both to keep scans fast and to avoid indexing internal tooling.
    bool IsSkippableFolder(const std::wstring& lowerName)
    {
        static const wchar_t* kSkip[] = {
            L"$recycle.bin", L"windowsapps", L"common files", L"redist",
            L"drivers", L"debug", L"symbols", L".git"
        };
        for (const wchar_t* skip : kSkip)
        {
            if (lowerName == skip) return true;
        }
        return false;
    }
}

// ----------------------------- IconCache -----------------------------

IconCache::~IconCache()
{
    Clear();
}

void IconCache::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [path, icon] : m_cache)
    {
        if (icon) DestroyIcon(icon);
    }
    m_cache.clear();
}

HICON IconCache::GetIcon(const std::wstring& path)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_cache.find(path);
        if (it != m_cache.end())
        {
            return it->second;
        }
    }

    // Extract outside the lock - SHGetFileInfo can be slow (it may hit
    // disk / the shell icon cache), and we don't want to block other
    // threads reading from the cache while we do it.
    SHFILEINFOW info{};
    HICON icon = nullptr;
    if (SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES) != 0)
    {
        icon = info.hIcon;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(path);
    if (it != m_cache.end())
    {
        // Another thread beat us to it - keep the existing one, discard ours.
        if (icon) DestroyIcon(icon);
        return it->second;
    }
    m_cache.emplace(path, icon);
    return icon;
}

// --------------------------- ProgramIndexer ---------------------------

ProgramIndexer::ProgramIndexer() = default;

ProgramIndexer::~ProgramIndexer()
{
    WaitForIndexing();
}

void ProgramIndexer::BeginIndexAsync(std::function<void()> onComplete)
{
    WaitForIndexing();
    m_indexing = true;

    m_worker = std::thread([this, onComplete]()
    {
        // COM is required for IShellLink (.lnk resolution).
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool comInitialized = SUCCEEDED(hr);

        IndexWorker();

        if (comInitialized) CoUninitialize();

        m_indexing = false;
        if (onComplete) onComplete();
    });
}

void ProgramIndexer::WaitForIndexing()
{
    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

std::vector<AppEntry> ProgramIndexer::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_index;
}

void ProgramIndexer::IndexWorker()
{
    std::vector<AppEntry> results;
    results.reserve(512);

    wchar_t path[MAX_PATH];

    // Start Menu (per-user and all-users) - primary, highest quality source.
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, path)))
    {
        ScanDirectoryForShortcuts(path, results, 0);
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, path)))
    {
        ScanDirectoryForShortcuts(path, results, 0);
    }

    // Desktop (per-user and all-users) shortcuts.
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path)))
    {
        ScanDirectoryForShortcuts(path, results, 0);
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, 0, path)))
    {
        ScanDirectoryForShortcuts(path, results, 0);
    }

    // Program Files / Program Files (x86) - catches apps that don't
    // register a Start Menu shortcut.
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, path)))
    {
        ScanProgramFilesForExecutables(path, results, 0);
    }
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, 0, path)))
    {
        ScanProgramFilesForExecutables(path, results, 0);
    }

    // WindowsApps (UWP/Store apps) - best-effort, often access-restricted.
    ScanWindowsApps(results);

    DeduplicateEntries(results);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_index = std::move(results);
}

void ProgramIndexer::ScanDirectoryForShortcuts(const std::wstring& directory, std::vector<AppEntry>& out, int depth)
{
    if (depth > 6) return; // guard against pathological folder nesting / symlink loops

    std::wstring searchPattern = directory + L"\\*";
    WIN32_FIND_DATAW findData{};
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring fullPath = directory + L"\\" + name;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ScanDirectoryForShortcuts(fullPath, out, depth + 1);
            continue;
        }

        std::wstring lowerName = Utils::ToLower(name);
        if (lowerName.size() >= 4 && lowerName.compare(lowerName.size() - 4, 4, L".lnk") == 0)
        {
            std::wstring target = ResolveShortcutTarget(fullPath);
            if (target.empty()) continue;

            std::wstring lowerTarget = Utils::ToLower(target);
            if (IsNoiseExecutable(lowerTarget)) continue;

            AppEntry entry;
            entry.displayName = Utils::GetFileNameWithoutExtension(name);
            entry.targetPath = target;
            entry.sourcePath = target;
            out.push_back(std::move(entry));
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

void ProgramIndexer::ScanProgramFilesForExecutables(const std::wstring& directory, std::vector<AppEntry>& out, int depth)
{
    if (depth > 3) return; // Program Files trees are deep; cap to keep startup fast

    std::wstring searchPattern = directory + L"\\*";
    WIN32_FIND_DATAW findData{};
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do
    {
        std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring fullPath = directory + L"\\" + name;
        std::wstring lowerName = Utils::ToLower(name);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (IsSkippableFolder(lowerName)) continue;
            ScanProgramFilesForExecutables(fullPath, out, depth + 1);
            continue;
        }

        if (lowerName.size() >= 4 && lowerName.compare(lowerName.size() - 4, 4, L".exe") == 0)
        {
            if (IsNoiseExecutable(lowerName)) continue;

            AppEntry entry;
            entry.displayName = Utils::GetFileNameWithoutExtension(name);
            entry.targetPath = fullPath;
            entry.sourcePath = fullPath;
            out.push_back(std::move(entry));
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

void ProgramIndexer::ScanWindowsApps(std::vector<AppEntry>& out)
{
    // WindowsApps is ACL-locked for most packages; we only pick up
    // entries we can actually enumerate without elevation. This is a
    // best-effort source - UWP apps are more reliably launched via the
    // Start Menu shortcuts scan above, which Windows already generates
    // for most packaged apps.
    wchar_t programFiles[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, programFiles))) return;

    std::wstring windowsApps = std::wstring(programFiles) + L"\\WindowsApps\\*";
    WIN32_FIND_DATAW findData{};
    HANDLE hFind = FindFirstFileW(windowsApps.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return; // access denied is expected/common here
    FindClose(hFind);
    // Intentionally not deep-scanning further: package folder names are
    // not human-friendly and resolving them to real display names
    // requires the Package Manager APIs. Left as a documented extension
    // point (see README "Future Features").
}

std::wstring ProgramIndexer::ResolveShortcutTarget(const std::wstring& lnkPath)
{
    IShellLinkW* shellLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr)) return L"";

    std::wstring result;
    IPersistFile* persistFile = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile));
    if (SUCCEEDED(hr))
    {
        hr = persistFile->Load(lnkPath.c_str(), STGM_READ);
        if (SUCCEEDED(hr))
        {
            wchar_t targetPath[MAX_PATH]{};
            WIN32_FIND_DATAW findData{};
            if (SUCCEEDED(shellLink->GetPath(targetPath, MAX_PATH, &findData, SLGP_RAWPATH)) && targetPath[0] != L'\0')
            {
                result = targetPath;
            }
        }
        persistFile->Release();
    }
    shellLink->Release();
    return result;
}

void ProgramIndexer::DeduplicateEntries(std::vector<AppEntry>& entries)
{
    std::unordered_set<std::wstring> seen;
    std::vector<AppEntry> unique;
    unique.reserve(entries.size());

    for (auto& entry : entries)
    {
        std::wstring key = Utils::ToLower(entry.targetPath);
        if (seen.insert(key).second)
        {
            unique.push_back(std::move(entry));
        }
    }

    entries = std::move(unique);
}
