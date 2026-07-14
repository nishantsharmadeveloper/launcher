#pragma once
//
// SearchEngine.h
// Given the current AppEntry index and a query string, produces a
// ranked list of SearchResult. Ranking tiers, from best to worst:
//   1. Exact match
//   2. Starts-with match
//   3. Contains match
//   4. Fuzzy (subsequence) match
// Ties within a tier are broken alphabetically. This class is stateless
// aside from a small launch-frequency table, so it's cheap to query on
// every keystroke.
//
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "ProgramIndexer.h"

struct SearchResult
{
    std::wstring displayName;
    std::wstring subtitlePath;
    std::wstring targetPath;
    HICON icon = nullptr; // borrowed from IconCache - do not destroy
};

class SearchEngine
{
public:
    SearchEngine() = default;

    // Runs a search against `index`, using `iconCache` to resolve each
    // result's icon. Returns at most `maxResults` results, ranked best
    // first. An empty query returns an empty result set (the launcher
    // shows a placeholder/empty state rather than the full app list).
    std::vector<SearchResult> Search(const std::wstring& query,
                                      const std::vector<AppEntry>& index,
                                      IconCache& iconCache,
                                      size_t maxResults = 10);

    // Records that `targetPath` was launched, so future searches can
    // weight frequently-launched apps higher (ranking tier 5 in the
    // spec). Persists only for the process lifetime; wiring this to
    // Settings/registry is a documented future extension.
    void RecordLaunch(const std::wstring& targetPath);

private:
    enum class MatchTier
    {
        Exact = 0,
        StartsWith = 1,
        Contains = 2,
        Fuzzy = 3,
        None = 4
    };

    struct RankedEntry
    {
        const AppEntry* entry;
        MatchTier tier;
        int fuzzyScore;
        int launchCount;
    };

    MatchTier ClassifyMatch(const std::wstring& query, const AppEntry& entry, int& fuzzyScoreOut) const;

private:
    std::unordered_map<std::wstring, int> m_launchCounts;
};
