#include "profile/restore.hpp"
#include "utils/helper.hpp"

namespace MCMMemory
{
    size_t Restore::GetOrAddMCM(const CapturedSetting& a_setting)
    {
        // All settings with the same stable ID share one restore session.
        for (size_t index = 0; index < restoreMCMs.size(); ++index) {
            if (restoreMCMs[index].identity.modID == a_setting.selection.identity.modID) {
                return index;
            }
        }

        RestoreMCM mcm;
        mcm.identity = a_setting.selection.identity;
        restoreMCMs.push_back(std::move(mcm));
        return restoreMCMs.size() - 1;
    }

    void Restore::AddPageAction(size_t a_mcmIndex, const MCMSelection& a_selection)
    {
        auto& mcm = restoreMCMs[a_mcmIndex];
        // Do not select the same page again between settings on that page.
        if (mcm.hasQueuedPage && mcm.queuedPageIndex == a_selection.pageIndex && mcm.queuedPageName == a_selection.pageName) {
            return;
        }

        mcm.settingActions.push_back(MakePageAction(a_mcmIndex, a_selection));
        mcm.queuedPageIndex = a_selection.pageIndex;
        mcm.queuedPageName = a_selection.pageName;
        mcm.hasQueuedPage = true;
    }

    bool Restore::AddSettingActions(const CapturedSetting& a_setting)
    {
        if (!a_setting.identityComplete) {
            return false;
        }

        RestoreAction requestAction;
        RestoreAction applyAction;
        // Used for MCM helper keymap changes.
        RestoreAction settingChangedAction;
        bool hasRequest{};
        bool hasSettingChangedAction{};

        // Check the saved value and build the calls needed by this control.
        switch (a_setting.type) {
            case ControlType::Option: {
                bool desiredValue{};
                if (a_setting.value.is_boolean()) {
                    desiredValue = a_setting.value.get<bool>();
                }
                else if (a_setting.value.is_number()) {
                    desiredValue = a_setting.value.get<float>() != 0.0F;
                }
                else {
                    logger::warn("Skipping toggle with a non-boolean value: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                applyAction = MakeToggleAction(0, a_setting.selection.optionIndex, desiredValue);
                break;
            }
            case ControlType::Slider:
                if (!a_setting.value.is_number()) {
                    logger::warn("Skipping slider with a non-numeric value: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                requestAction = MakeOptionAction(RestoreActionType::RequestSliderDialogData, 0, a_setting.selection.optionIndex);
                applyAction = MakeFloatAction(RestoreActionType::SetSliderValue, 0, a_setting.value.get<float>());
                hasRequest = true;
                break;
            case ControlType::Menu: {
                if (!a_setting.value.is_number()) {
                    logger::warn("Skipping menu with a non-integer value: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                int desiredIndex = a_setting.value.get<int>();
                if (desiredIndex < 0) {
                    logger::warn("Skipping menu with a negative index: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                requestAction = MakeOptionAction(RestoreActionType::RequestMenuDialogData, 0, a_setting.selection.optionIndex);
                applyAction = MakeIntegerAction(RestoreActionType::SetMenuIndex, 0, desiredIndex);
                hasRequest = true;
                break;
            }
            case ControlType::Color: {
                if (!a_setting.value.is_number()) {
                    logger::warn("Skipping color with a non-integer value: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                int desiredColor = a_setting.value.get<int>();
                if (desiredColor < 0 || desiredColor > 0xFFFFFF) {
                    logger::warn("Skipping color outside the RGB range: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                requestAction = MakeOptionAction(RestoreActionType::RequestColorDialogData, 0, a_setting.selection.optionIndex);
                applyAction = MakeIntegerAction(RestoreActionType::SetColorValue, 0, desiredColor);
                hasRequest = true;
                break;
            }
            case ControlType::Input:
                if (!a_setting.value.is_string()) {
                    logger::warn("Skipping input with a non-string value: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                requestAction = MakeOptionAction(RestoreActionType::RequestInputDialogData, 0, a_setting.selection.optionIndex);
                applyAction = MakeStringAction(RestoreActionType::SetInputText, 0, a_setting.value.get<std::string>());
                hasRequest = true;
                break;
            case ControlType::Keymap: {
                if (!a_setting.value.is_number()) {
                    logger::warn("Skipping keymap with a non-integer key code: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                int desiredKeyCode = a_setting.value.get<int>();
                if (desiredKeyCode < -1) {
                    logger::warn("Skipping keymap with an invalid key code: {} option {}", a_setting.selection.identity.modID, a_setting.selection.optionIndex);
                    return false;
                }
                if (!a_setting.settingID.empty()) {
                    applyAction = MakeSettingIntegerAction(0, a_setting.settingID, desiredKeyCode);
                    settingChangedAction = MakeStringAction(RestoreActionType::NotifySettingChanged, 0, a_setting.settingID);
                    hasSettingChangedAction = true;
                }
                else {
                    applyAction = MakeKeymapAction(0, a_setting.selection.optionIndex, desiredKeyCode, !a_setting.stateName.empty());
                }
                break;
            }
            default:
                return false;
        }

        // Add the page call first, then the control specific calls.
        size_t mcmIndex = GetOrAddMCM(a_setting);
        settingChangedAction.mcmIndex = mcmIndex;
        for (auto* action : { &requestAction, &applyAction }) {
            action->mcmIndex = mcmIndex;
            action->controlType = a_setting.type;
            action->optionIndex = a_setting.selection.optionIndex;
            action->optionLabel = a_setting.optionLabel;
            action->stateName = a_setting.stateName;
            action->pageName = a_setting.selection.pageName;
            action->pageIndex = a_setting.selection.pageIndex;
        }
        AddPageAction(mcmIndex, a_setting.selection);
        if (hasRequest) {
            restoreMCMs[mcmIndex].settingActions.push_back(std::move(requestAction));
        }
        restoreMCMs[mcmIndex].settingActions.push_back(std::move(applyAction));
        if (hasSettingChangedAction) {
            restoreMCMs[mcmIndex].settingActions.push_back(std::move(settingChangedAction));
        }
        return true;
    }

    bool Restore::LoadProfile()
    {
        Profile profile;
        if (!ProfileStorage::Load(profile)) {
            logger::info("No readable persistent profile is available at {}; automatic restoration is inactive", ToUTF8(ProfileStorage::Path()));
            return false;
        }

        size_t supportedSettingCount{};
        size_t excludedSettingCount{};
        for (const auto& setting : profile) {
            const auto& modID = setting.selection.identity.modID;
            const bool selected = AllowsMCM(mcmFilter, modID);
            const bool automaticEnabled = operationMode != OperationMode::Automatic || GetSettings().IsAutoRestoreEnabled(modID);
            if (!selected || !automaticEnabled) {
                ++excludedSettingCount;
                continue;
            }
            if (AddSettingActions(setting)) {
                ++supportedSettingCount;
            }
        }

        logger::info("Loaded persistent profile with {} supported settings across {} MCM configurations ({} excluded)", supportedSettingCount, restoreMCMs.size(), excludedSettingCount);
        if (supportedSettingCount == 0) {
            logger::warn("Persistent profile contains no supported settings to restore");
            return false;
        }
        
        return true;
    }
}
