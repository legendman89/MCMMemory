#pragma once

#include "profile/types.hpp"

namespace MCMMemory
{
    struct Profile
    {
        // Separated settings from activations.
        std::vector<CapturedSetting> settings;

        std::vector<MCMActivation> activations;

        inline void Clear()
        {
            settings.clear();
            activations.clear();
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

        // Adds or replaces one captured setting in the selected profile.
        static bool UpdateSetting(const CapturedSetting& a_setting);

        // Remembers whether the player allowed a staged MCM to start automatically.
        static bool UpdateActivation(const MCMActivation& a_activation, bool a_enabled);

        // Writes the complete profile to disk.
        static bool Save(const Profile& a_profile);

        static bool Save(std::string_view a_name, const Profile& a_profile);

        static std::filesystem::path Path();

        static std::filesystem::path Path(std::string_view a_name);

    private:

        // Converts one JSON setting into a CapturedSetting.
        static bool FromJson(const nlohmann::json& a_document, CapturedSetting& a_setting);
        
        // Converts the in-memory profile into its JSON layout.
        static nlohmann::json ToJson(const Profile& a_profile);

    };
}
