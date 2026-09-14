#include "profile/action.hpp"

namespace MCMMemory
{

    // Adds one recorded action. Changing a control again overwrites its record in place so it keeps
    // its first position, but never across a dropdown, page rebuild or config reopen.
    void AppendAction(std::vector<CapturedSetting>& a_settings, CapturedSetting a_setting)
    {
        a_setting.recorded = true;
        const auto& modID = a_setting.selection.identity.modID;

        // Track dependencies and intervening settings for this mod while scanning backwards.
        bool settingAfter{};
        bool orderedStepAfter{};
        for (auto existing = a_settings.rbegin(); existing != a_settings.rend(); ++existing) {
            // Scanned settings have no position to keep, so there is nothing to overwrite.
            if (!existing->recorded || existing->selection.identity.modID != modID) {
                continue;
            }
            if (a_setting.command) {
                break;
            }
            if (existing->IsSameSetting(a_setting)) {
                // Preserve dependencies on either version of a control, including a newly captured rebuild.
                if (existing->command || orderedStepAfter || a_setting.reopensConfig || ((existing->RequiresOrderedReplay() || a_setting.RequiresOrderedReplay()) && settingAfter)) {
                    break;
                }
                const int sequence = existing->sequence;
                // Replacing the value must keep the open/close actions attached to this position.
                a_setting.reopensConfig = a_setting.reopensConfig || existing->reopensConfig;
                a_setting.rebuildsPage = a_setting.rebuildsPage || existing->rebuildsPage;
                *existing = std::move(a_setting);
                existing->sequence = sequence;
                return;
            }
            if (existing->RequiresOrderedReplay()) {
                orderedStepAfter = true;
            }
            settingAfter = true;
        }

        // Drop any scanned copy of this control (due to manual backup). 
        // Its value is invalidated and its position means nothing,
        // and leaving it would let it overwrite what we just recorded.
        auto scanned = a_settings.begin();
        while (scanned != a_settings.end()) {
            if (!scanned->recorded && scanned->selection.identity.modID == modID && scanned->IsSameSetting(a_setting)) {
                scanned = a_settings.erase(scanned);
            }
            else {
                ++scanned;
            }
        }

        a_setting.sequence = NextActionSequence(a_settings, modID);
        a_settings.push_back(std::move(a_setting));
    }

    // True when the profile recorded the player changing the activation control itself, so the
    // replay order already enables this MCM. A rebuild on any other control does not count.
    bool IsActivationRecorded(const std::vector<CapturedSetting>& a_settings, const MCMActivation& a_activation)
    {
        for (const auto& setting : a_settings) {
            if (!setting.recorded || !setting.rebuildsPage || setting.selection.identity.modID != a_activation.selection.identity.modID) {
                continue;
            }
            // A state name survives page rebuilds.
            if (!setting.stateName.empty() && !a_activation.stateName.empty()) {
                if (setting.stateName == a_activation.stateName) {
                    return true;
                }
                continue;
            }
            if (setting.selection.pageName == a_activation.selection.pageName && setting.selection.optionIndex == a_activation.selection.optionIndex) {
                return true;
            }
        }
        return false;
    }
}
