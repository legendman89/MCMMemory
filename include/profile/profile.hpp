#pragma once

#include "profile/types.hpp"

namespace MCMMemory
{
    using Profile = std::vector<CapturedSetting>;

    class ProfileStorage
    {
    public:

        // Reads the selected profile into memory.
        static bool Load(Profile& a_profile);

        static bool Load(std::string_view a_name, Profile& a_profile);

        // Adds or replaces one captured setting in the selected profile.
        static bool UpdateSetting(const CapturedSetting& a_setting);

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
