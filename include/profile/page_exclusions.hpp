#pragma once

#include "profile/types.hpp"

#define FOREACH_PAGE_EXCLUSION_MODE(X) \
    X(Include, "include") \
    X(BackupCapture, "backupCapture") \
    X(Restore, "restore") \
    X(All, "all")

namespace MCMMemory
{
    enum class PageExclusionMode
    {
#define DECLARE_PAGE_EXCLUSION_MODE(name, text) name,
        FOREACH_PAGE_EXCLUSION_MODE(DECLARE_PAGE_EXCLUSION_MODE)
#undef DECLARE_PAGE_EXCLUSION_MODE
        Count
    };

    inline constexpr std::array<std::string_view, ToIndex(PageExclusionMode::Count)> pageExclusionModeNames
    {
#define DECLARE_PAGE_EXCLUSION_NAME(name, text) text,
        FOREACH_PAGE_EXCLUSION_MODE(DECLARE_PAGE_EXCLUSION_NAME)
#undef DECLARE_PAGE_EXCLUSION_NAME
    };

    inline PageExclusionMode ParsePageExclusionMode(std::string_view a_name)
    {
        for (size_t index = 0; index < pageExclusionModeNames.size(); ++index) {
            if (a_name == pageExclusionModeNames[index]) {
                return static_cast<PageExclusionMode>(index);
            }
        }
        return PageExclusionMode::Include;
    }

    struct MCMPageExclusion : MCMPage
    {
        PageExclusionMode mode{};
        bool matchIndex{};
    };

    using PageExclusionMap = std::unordered_map<std::string, std::vector<MCMPageExclusion>, StringHash, std::equal_to<>>;

    inline bool IsPageExcluded(const PageExclusionMap& a_exclusions, std::string_view a_modID, std::string_view a_pageName, int a_pageIndex, PageExclusionMode a_operation)
    {
        const auto found = a_exclusions.find(a_modID);
        if (found == a_exclusions.end()) {
            return false;
        }
        for (const auto& page : found->second) {
            if (page.name == a_pageName && (!(page.matchIndex || page.name.empty()) || page.index == a_pageIndex) &&
                (page.mode == PageExclusionMode::All || page.mode == a_operation)) {
                return true;
            }
        }
        return false;
    }

}
