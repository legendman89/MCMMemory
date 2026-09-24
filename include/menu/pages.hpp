#pragma once

#include "menu/menu.hpp"
#include "profile/page_exclusions.hpp"

namespace MCMMemory::Menu
{
    
    inline constexpr std::array<std::string_view, ToIndex(PageExclusionMode::Count)> pageExclusionLabels
    {
#define DECLARE_PAGE_EXCLUSION_LABEL(name, text) "Profile.Pages.Mode." #name,
        FOREACH_PAGE_EXCLUSION_MODE(DECLARE_PAGE_EXCLUSION_LABEL)
#undef DECLARE_PAGE_EXCLUSION_LABEL
    };

    struct MCMPageRow : MCMPageExclusion
    {
        bool available{};
    };

    class MCMPagesWindow
    {
    public:

        void Open(const MCMIdentity& a_identity);

        void Render();

        inline bool IsOpen() const { return open; }

    private:

        void Refresh(bool a_loadChoices = false);

        void AddPage(std::string_view a_name, int a_index, bool a_available, bool a_uniqueName = false);

        void Apply();

        MCMIdentity identity;
        std::string profile;
        std::vector<MCMPageRow> pages;
        std::string error;
        bool open{};
    };
}
