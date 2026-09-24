#pragma once

#include "profile/types.hpp"
#include "profile/mode.hpp"
#include "profile/page_exclusions.hpp"

namespace MCMMemory
{
    struct Profile
    {
        // Separated settings from activations.
        std::vector<CapturedSetting> settings;

        std::vector<MCMActivation> activations;

        // Only holds mods that differ from the default,
        // a missing mod implies ProfileMode::Value.
        ProfileModeMap mods;

        PageExclusionMap pageExclusions;

        inline void Clear()
        {
            mods.clear();
            settings.clear();
            activations.clear();
            pageExclusions.clear();
        }

        // Value or Action per mod?
        inline ProfileMode ModeFor(std::string_view a_modID) const
        {
            const auto found = mods.find(a_modID);
            return found != mods.end() ? found->second : ProfileMode::Value;
        }

        inline bool IsActionMode(std::string_view a_modID) const
        {
            return ModeFor(a_modID) == ProfileMode::Action;
        }

        inline void SetMode(std::string_view a_modID, ProfileMode a_mode)
        {
            const auto found = mods.find(a_modID);
            if (found != mods.end()) {
                found->second = a_mode;
                return;
            }

            // A mod only tracked iff it stops using the default mode (Value).
            if (a_mode != ProfileMode::Value) {
                mods.emplace(std::string(a_modID), a_mode);
            }
        }

        inline const MCMActivation* FindActivation(std::string_view a_modID) const
        {
            for (const auto& activation : activations) {
                if (activation.selection.identity.modID == a_modID) {
                    return std::addressof(activation);
                }
            }
            return nullptr;
        }

        inline void SetActivation(const MCMActivation& a_activation)
        {
            auto found = activations.begin();
            while (found != activations.end() && found->selection.identity.modID != a_activation.selection.identity.modID) {
                ++found;
            }
            if (found != activations.end()) {
                *found = a_activation;
            }
            else {
                activations.push_back(a_activation);
            }
        }

        inline void RemoveActivation(std::string_view a_modID)
        {
            auto found = activations.begin();
            while (found != activations.end() && found->selection.identity.modID != a_modID) {
                ++found;
            }
            if (found != activations.end()) {
                activations.erase(found);
            }
        }
    };

    class ProfileStorage
    {
    public:

        // Reads the selected profile into memory.
        static bool Load(Profile& a_profile);

        static bool Load(std::string_view a_name, Profile& a_profile);

        // Adds or replaces a captured setting in memory until the journal closes.
        static bool UpdateSetting(std::string_view a_name, const CapturedSetting& a_setting);

        // Remembers whether the player allowed a staged MCM to start automatically.
        static bool UpdateActivation(std::string_view a_name, const MCMActivation& a_activation, bool a_enabled);

        // Saves page choices without replacing settings captured since the pages window opened.
        static bool SavePageExclusions(std::string_view a_name, std::string_view a_modID, const std::vector<MCMPageExclusion>& a_pages);

        // Saves pending profiles. Failed writes remain in memory for retry.
        static bool FlushPending();

        // Removes every saved setting, activation, mode and page exclusion for these MCMs from the named profile.
        static bool ForgetMCMs(std::string_view a_name, const MCMFilter& a_modIDs, size_t& a_settingCount);

        // Writes the complete profile to disk.
        static bool Save(const Profile& a_profile);

        static bool Save(std::string_view a_name, const Profile& a_profile);

        static std::filesystem::path Path();

        static std::filesystem::path Path(std::string_view a_name);

    private:

        static bool LoadFile(std::string_view a_name, Profile& a_profile);

        static bool SaveFile(std::string_view a_name, const Profile& a_profile);

        static Profile* GetPendingProfile(std::string_view a_name);

        // Converts one JSON setting into a CapturedSetting.
        static bool FromJson(const nlohmann::json& a_document, CapturedSetting& a_setting);
        
        // Converts the in-memory profile into its JSON format.
        static nlohmann::ordered_json ToJson(const Profile& a_profile);

        inline static std::mutex profileMutex;

        // Holds profiles that have been changed in memory but not yet saved to disk.
        inline static std::unordered_map<std::string, Profile, StringHash, std::equal_to<>> pendingProfiles;

    };
}
