#pragma once

#include "types.hpp"

namespace MCMMemory
{
    using Profile = std::vector<CapturedSetting>;

    class ProfileStorage
    {
    public:

        // Reads Profile.json into memory.
        static bool Load(Profile& a_profile);

        // Adds or replaces one captured setting in Profile.json.
        static bool UpdateSetting(const CapturedSetting& a_setting);

        // Writes the complete profile to disk.
        static bool Save(const Profile& a_profile);

        // Returns the full path to Profile.json.
        static inline std::filesystem::path Path() { return GetPluginDataPath() / "Profile.json"; }

    private:

        // Converts one JSON setting into a CapturedSetting.
        static bool FromJson(const nlohmann::json& a_document, CapturedSetting& a_setting);
        
        // Converts the in-memory profile into its JSON layout.
        static nlohmann::json ToJson(const Profile& a_profile);

    };
}
