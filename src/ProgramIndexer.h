#pragma once
//
// ProgramIndexer.h
// Builds an in-memory index of launchable applications by scanning
// Start Menu folders, the Desktop, Program Files, and WindowsApps.
// Indexing runs on a background thread so application startup stays
// responsive. Also owns the IconCache, which extracts and caches the
// shell icon (HICON) for each indexed entry.
//
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>

// A single indexed, launchable application.
struct AppEntry
{
    std::wstring displayName;   // "Google Chrome"
    std::wstring targetPath;    // Resolved .exe path used to launch and to extract the icon
    std::wstring sourcePath;    // Original path (may be a .lnk) - shown as the subtitle
};

// Caches HICON handles by file path so we only pay the cost of shell
// icon extraction once per unique executable. Icons are destroyed on
// cache destruction (RAII).
class IconCache
{
public:
    IconCache() = default;
    ~IconCache();

    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    // Returns a cached icon for `path`, extracting and caching it on
    // first request. Returns nullptr if extraction fails. The returned
    // HICON is owned by the cache - callers must not destroy it.
    HICON GetIcon(const std::wstring& path);

    void Clear();

private:
    std::mutex m_mutex;
    std::unordered_map<std::wstring, HICON> m_cache;
};

class ProgramIndexer
{
public:
    ProgramIndexer();
    ~ProgramIndexer();

    ProgramIndexer(const ProgramIndexer&) = delete;
    ProgramIndexer& operator=(const ProgramIndexer&) = delete;

    // Kicks off indexing on a background thread. `onComplete` is invoked
    // on that same background thread once indexing finishes; the caller
    // is responsible for marshalling back to the UI thread if needed
    // (Application does this via PostMessage).
    void BeginIndexAsync(std::function<void()> onComplete);

    // Blocks until any in-flight indexing thread has finished. Safe to
    // call from the destructor or before re-indexing.
    void WaitForIndexing();

    // Returns a thread-safe snapshot copy of the current index. A copy
    // is returned (rather than a reference) so SearchEngine can iterate
    // it without holding a lock while a re-index happens concurrently.
    std::vector<AppEntry> GetSnapshot() const;

    IconCache& GetIconCache() { return m_iconCache; }

private:
    void IndexWorker();
    void ScanDirectoryForShortcuts(const std::wstring& directory, std::vector<AppEntry>& out, int depth);
    void ScanProgramFilesForExecutables(const std::wstring& directory, std::vector<AppEntry>& out, int depth);
    void ScanWindowsApps(std::vector<AppEntry>& out);
    std::wstring ResolveShortcutTarget(const std::wstring& lnkPath);
    void DeduplicateEntries(std::vector<AppEntry>& entries);

private:
    mutable std::mutex m_mutex;
    std::vector<AppEntry> m_index;
    std::thread m_worker;
    std::atomic<bool> m_indexing{ false };
    IconCache m_iconCache;
};
