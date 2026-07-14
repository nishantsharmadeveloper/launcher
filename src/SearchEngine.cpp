#include "SearchEngine.h"
#include "Utils.h"
#include <algorithm>

SearchEngine::MatchTier SearchEngine::ClassifyMatch(const std::wstring& query, const AppEntry& entry, int& fuzzyScoreOut) const
{
    fuzzyScoreOut = 0;

    if (Utils::EqualsCaseInsensitive(entry.displayName, query))
    {
        return MatchTier::Exact;
    }
    if (Utils::StartsWithCaseInsensitive(entry.displayName, query))
    {
        return MatchTier::StartsWith;
    }
    if (Utils::ContainsCaseInsensitive(entry.displayName, query))
    {
        return MatchTier::Contains;
    }
    int score = 0;
    if (Utils::FuzzyMatch(entry.displayName, query, score))
    {
        fuzzyScoreOut = score;
        return MatchTier::Fuzzy;
    }
    return MatchTier::None;
}

std::vector<SearchResult> SearchEngine::Search(const std::wstring& query,
                                                const std::vector<AppEntry>& index,
                                                IconCache& iconCache,
                                                size_t maxResults)
{
    std::vector<SearchResult> results;
    if (query.empty())
    {
        return results;
    }

    std::vector<RankedEntry> ranked;
    ranked.reserve(index.size());

    for (const AppEntry& entry : index)
    {
        int fuzzyScore = 0;
        MatchTier tier = ClassifyMatch(query, entry, fuzzyScore);
        if (tier == MatchTier::None) continue;

        int launchCount = 0;
        auto it = m_launchCounts.find(entry.targetPath);
        if (it != m_launchCounts.end()) launchCount = it->second;

        ranked.push_back(RankedEntry{ &entry, tier, fuzzyScore, launchCount });
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedEntry& a, const RankedEntry& b)
    {
        if (a.tier != b.tier) return a.tier < b.tier;
        // Within the same match quality tier, prefer apps launched more
        // often (tier 5 in the spec: "frequently launched apps").
        if (a.launchCount != b.launchCount) return a.launchCount > b.launchCount;
        if (a.tier == MatchTier::Fuzzy && a.fuzzyScore != b.fuzzyScore) return a.fuzzyScore < b.fuzzyScore;
        return Utils::ToLower(a.entry->displayName) < Utils::ToLower(b.entry->displayName);
    });

    size_t count = std::min(maxResults, ranked.size());
    results.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        const AppEntry* entry = ranked[i].entry;
        SearchResult result;
        result.displayName = entry->displayName;
        result.subtitlePath = entry->sourcePath;
        result.targetPath = entry->targetPath;
        result.icon = iconCache.GetIcon(entry->targetPath);
        results.push_back(std::move(result));
    }

    return results;
}

void SearchEngine::RecordLaunch(const std::wstring& targetPath)
{
    m_launchCounts[targetPath]++;
}
