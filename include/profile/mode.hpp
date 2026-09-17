#pragma once

#include "utils/helper.hpp"

#define FOREACH_PROFILE_MODE(MODE) \
    MODE(Value, "value") \
    MODE(Action, "action")

namespace MCMMemory
{
    // Value keeps the final value of each control, Action keeps the order the player changed them in.
    enum class ProfileMode
    {
#define DECLARE_PROFILE_MODE(name, text) name,
        FOREACH_PROFILE_MODE(DECLARE_PROFILE_MODE)
#undef DECLARE_PROFILE_MODE
        Count
    };

    inline constexpr std::array<std::string_view, ToIndex(ProfileMode::Count)> profileModeNames
    {
#define DECLARE_PROFILE_MODE_NAME(name, text) text,
        FOREACH_PROFILE_MODE(DECLARE_PROFILE_MODE_NAME)
#undef DECLARE_PROFILE_MODE_NAME
    };

    inline std::string_view ProfileModeName(ProfileMode a_mode)
    {
        return profileModeNames[ToIndex(a_mode)];
    }

    // An unknown mode falls back to Value so an older version can't confuse a newer profile.
    inline ProfileMode ParseProfileMode(std::string_view a_name)
    {
        for (size_t index = 0; index < profileModeNames.size(); ++index) {
            if (a_name == profileModeNames[index]) {
                return static_cast<ProfileMode>(index);
            }
        }
        return ProfileMode::Value;
    }

    using ProfileModeMap = std::unordered_map< std::string, ProfileMode, StringHash, std::equal_to<> >;
}