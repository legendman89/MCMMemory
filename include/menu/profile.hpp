#pragma once

#include "mcm/mcm_support.hpp"
#include "mcm/mcm_calls.hpp"
#include "menu/menu.hpp"
#include "profile/types.hpp"

namespace MCMMemory::Menu
{
    struct ProfileMCMRow
    {
        bool CanSelect() const
        {
            return available && !IsExcluded() && !IsUnresponsive();
        }

        bool IsExcluded() const { return !GetMCMExclusionReason(identity.modID).empty(); }

        bool IsUnresponsive() const { return unresponsive; }

        MCMIdentity identity;

        uint32_t settingCount{};

        bool available{};

        bool selected{};

        bool unresponsive{};
    };

    struct SelectedMCMFilters
    {
        MCMFilter backup;

        MCMFilter restore;
    };

    struct CreateProfileWindow
    {
        std::string sourceProfile;

        std::string error;

        std::array<char, 81> name{};

        bool open{};

        bool duplicate{};
    };

    struct DeleteProfileWindow
    {
        std::string profile;

        std::string error;

        bool open{};
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

        void RefreshProfileNames();

        void RenderProfileControls();

        void RenderProfileSelector(bool a_operationRunning);

        void RenderOperationButtons(float a_backupWidth, float a_restoreWidth);

        void RenderCreateProfileWindow();

        void RenderDeleteProfileWindow();

        void RenderAutomation();

        void RenderMCMs();

        void RenderMCMTable(bool a_operationAvailable);

        void RenderMCMCounts(size_t a_registeredMCMCount, size_t a_selectedMCMCount) const;

        bool MatchesSearch(const ProfileMCMRow& a_mcm) const;

        inline bool IsVisible(const ProfileMCMRow& a_mcm) const
        {
            return MatchesSearch(a_mcm) && (!hideUnavailable || a_mcm.available);
        }

        ProfileMCMRow& FindOrAddMCM(const MCMIdentity& a_identity, const MCMFilter& a_selectedMCMs);

        SelectedMCMFilters ReadSelectedMCMs() const;

        void SelectVisibleMCMs(bool a_selected);

        std::vector<ProfileMCMRow> mcms;

        std::vector<std::string> profileNames;

        CreateProfileWindow createProfileWindow;

        DeleteProfileWindow deleteProfileWindow;

        RegistryWait registryWait;

        std::filesystem::file_time_type profileWriteTime{};

        std::chrono::steady_clock::time_point nextRegistryRefresh{};

        uint64_t registryCacheGeneration{};

        uint64_t unavailableGeneration{};

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
