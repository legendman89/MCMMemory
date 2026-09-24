#pragma once

#include "menu/menu.hpp"
#include "profile/types.hpp"

namespace MCMMemory::Menu
{
    
#define FOREACH_PAGE_EXCLUSION_MODE(X) \
    X(Include) \
    X(BackupCapture) \
    X(Restore) \
    X(All)

#define DECLARE_PAGE_EXCLUSION_MODE(name) name,
#define DECLARE_PAGE_EXCLUSION_LABEL(name) "Profile.Pages.Mode." #name,

    enum class PageExclusionMode
    {
        FOREACH_PAGE_EXCLUSION_MODE(DECLARE_PAGE_EXCLUSION_MODE)
        Count
    };

    inline constexpr std::array<std::string_view, ToIndex(PageExclusionMode::Count)> pageExclusionLabels
    {
        FOREACH_PAGE_EXCLUSION_MODE(DECLARE_PAGE_EXCLUSION_LABEL)
    };

#undef DECLARE_PAGE_EXCLUSION_MODE
#undef DECLARE_PAGE_EXCLUSION_LABEL
#undef FOREACH_PAGE_EXCLUSION_MODE

    struct MCMPageRow : MCMPage
    {
        PageExclusionMode mode{};
        bool available{};
    };

    struct MCMPageChoices
    {
        std::string profile;
        std::string modID;
        std::vector<MCMPageRow> pages;
    };

    class MCMPagesWindow
    {
    public:

        void Open(const MCMIdentity& a_identity);

        void Render();

        inline bool IsOpen() const { return open; }

    private:

        void Refresh();

        void AddPage(std::string_view a_name, int a_index, bool a_available, bool a_uniqueName = false);

        void Apply();

        MCMPageChoices* FindChoices();

        MCMIdentity identity;
        std::string profile;
        std::vector<MCMPageRow> pages;
        std::vector<MCMPageChoices> choices;
        bool open{};
    };
}
