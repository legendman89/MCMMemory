#pragma once

#include "profile/types.hpp"

namespace MCMMemory
{
    // Adds one recorded action. Changing a control again overwrites its record in place so it keeps
    // its first position, but never across a dropdown, page rebuild or config reopen.
    void AppendAction(std::vector<CapturedSetting>& a_settings, CapturedSetting a_setting);

    // True when the profile recorded the player changing the activation control itself, so the
    // replay order already enables this MCM. A rebuild on any other control does not count.
    bool IsActivationRecorded(const std::vector<CapturedSetting>& a_settings, const MCMActivation& a_activation);

    // Only recorded settings are numbered; scanned ones have no position to keep.
    inline int NextActionSequence(const std::vector<CapturedSetting>& a_settings, std::string_view a_modID)
    {
        int next{};
        for (const auto& setting : a_settings) {
            if (setting.recorded && setting.selection.identity.modID == a_modID && setting.sequence >= next) {
                next = setting.sequence + 1;
            }
        }
        return next;
    }
}
