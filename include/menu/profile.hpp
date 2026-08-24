#pragma once

#include "mcm/mcm_registry.hpp"
#include "menu/menu.hpp"
#include "profile/types.hpp"

namespace MCMMemory::Menu
{
    struct ProfileMCMRow
    {
        MCMIdentity identity;

        uint32_t settingCount{};

        bool available{};

        bool selected{};
    };

    struct SelectedMCMFilters
    {
        MCMFilter backup;

        MCMFilter restore;
    };

    class ProfileMenu
    {
    public:

        static ProfileMenu* GetSingleton()
        {
            static ProfileMenu singleton;
            return std::addressof(singleton);
        }

        void Render();

    private:

        void Refresh();

        bool NeedsRefresh() const;

        void RenderOperationButtons();

        void RenderAutomation();

        void RenderMCMs();

        void RenderMCMTable(bool a_operationAvailable);

        void RenderMCMCounts(size_t a_registeredMCMCount, size_t a_selectedMCMCount) const;

        bool MatchesSearch(const ProfileMCMRow& a_mcm) const;

        bool IsVisible(const ProfileMCMRow& a_mcm) const
        {
            return MatchesSearch(a_mcm) && (!hideUnavailable || a_mcm.available);
        }

        ProfileMCMRow& FindOrAddMCM(const MCMIdentity& a_identity, const MCMFilter& a_selectedMCMs);

        SelectedMCMFilters ReadSelectedMCMs() const;

        void SelectVisibleMCMs(bool a_selected);

        std::vector<ProfileMCMRow> mcms;

        RegistryWait registryWait;

        std::filesystem::file_time_type profileWriteTime{};

        std::chrono::steady_clock::time_point nextRegistryRefresh{};

        std::array<char, 128> search{};

        bool loaded{};

        bool profileAvailable{};

        bool gameLoaded{};

        bool registrySettled{};

        bool hideUnavailable{};
    };

    inline void __stdcall RenderProfile()
    {
        ProfileMenu::GetSingleton()->Render();
    }
}
