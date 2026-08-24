#pragma once

#include "menu/ui.hpp"
#include "profile/activity.hpp"

namespace MCMMemory::Menu
{
    struct ActivityColumn
    {
        std::string_view label;

        float width{};
    };

    // Singleton class that renders the activity menu.
    class ActivityMenu
    {
    public:

        static ActivityMenu* GetSingleton()
        {
            static ActivityMenu singleton;
            return std::addressof(singleton);
        }

        void Render();

    private:

        std::string FormatSummary(const ActivityEntry& a_entry) const;

        std::string FormatRelativeTime(const ActivityEntry& a_entry) const;

        std::string FormatExactTime(const ActivityEntry& a_entry) const;

        void RenderDetails(const std::vector<ActivityEntry>& a_entries);

        bool BeginModTable(const ActivityEntry& a_entry, const char* a_id, const std::array<ActivityColumn, 3>& a_columns) const;

        void RenderModName(const ActivityModResult& a_mod) const;

        void RenderValue(uint32_t a_value) const;

        void RenderStatus(uint32_t a_failedCount) const;

        void RenderBackupMods(const ActivityEntry& a_entry) const;

        void RenderRestoreMods(const ActivityEntry& a_entry) const;

        uint64_t selectedID{};

        bool detailsOpen{};
    };

    inline void __stdcall RenderActivity()
    {
        ActivityMenu::GetSingleton()->Render();
    }
}
