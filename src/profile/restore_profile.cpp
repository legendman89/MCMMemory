#include "profile/restore.hpp"
#include "mcm/mcm_support.hpp"
#include "profile/action.hpp"
#include "utils/helper.hpp"

namespace MCMMemory
{
    size_t Restore::GetOrAddMCM(const MCMIdentity& a_identity)
    {
        // All settings with the same stable ID share one restore session.
        for (size_t index = 0; index < restoreMCMs.size(); ++index) {
            if (restoreMCMs[index].identity.modID == a_identity.modID) {
                return index;
            }
        }

        RestoreMCM mcm;
        mcm.identity = a_identity;
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

    void Restore::AddReopenActions(size_t a_mcmIndex)
    {
        auto& mcm = restoreMCMs[a_mcmIndex];
        for (const auto type : { RestoreActionType::CloseConfig, RestoreActionType::OpenConfig }) {
            auto action = MakeRestoreAction(type, a_mcmIndex);
            action.reopenStep = true;
            mcm.settingActions.push_back(std::move(action));
        }
        // A closed config forgets its page, so the next setting selects one again.
        mcm.hasQueuedPage = false;
        logger::debug("Profile restore will reopen '{}' through its recorded settings", mcm.identity.modID);
    }

    bool Restore::AddSettingActions(const CapturedSetting& a_setting)
    {
        if (!a_setting.identityComplete || MCMCommandSupport::IsExcludedPage(a_setting.selection.identity.modID, a_setting.selection.pageName, a_setting.selection.pageIndex)) {
            return false;
        }
        if (a_setting.command && a_setting.type != ControlType::Unknown && a_setting.type != ControlType::Option && a_setting.type != ControlType::Menu) {
            return false;
        }
        if (a_setting.command ? (!a_setting.recorded || IsProfileWriteCommand(a_setting.optionLabel, a_setting.stateName)) : MCMCommandSupport::IsIgnored(a_setting.selection.identity.modID, a_setting.selection.pageName, a_setting.selection.pageIndex, a_setting.type, a_setting.stateName, a_setting.optionLabel)) {
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
            case ControlType::Unknown:
                if (!a_setting.command) {
                    return false;
                }
                applyAction = MakeOptionAction(RestoreActionType::ApplyCommand, 0, a_setting.selection.optionIndex);
                break;
            case ControlType::Cycle: {
                if (a_setting.textControl) {
                    // Manual backups can include captured clicks without recording their order.
                    const bool capturedClick = a_setting.recorded || a_setting.valueSource == "menu.option.strValue";
                    if (!capturedClick || MCMCommandSupport::IsIgnored(a_setting.selection.identity.modID, a_setting.selection.pageName, a_setting.selection.pageIndex, ControlType::Unknown, a_setting.stateName, a_setting.optionLabel)) {
                        logger::warn("Skipping uncaptured or command text control '{}'", a_setting.optionLabel);
                        return false;
                    }
                    // No table and no typed value implies the recorded text is reached by clicking the row.
                    if (!a_setting.value.is_string() || a_setting.value.get<std::string>().empty()) {
                        logger::warn("Skipping text setting '{}' without a recorded value", a_setting.optionLabel);
                        return false;
                    }
                    applyAction = MakeStringAction(RestoreActionType::ApplyClicks, 0, a_setting.value.get<std::string>());
                    break;
                }
                const auto* cycle = SkyUICycleSupport::Find(a_setting.selection.identity.modID, a_setting.settingID);
                if (!cycle || a_setting.selection.pageIndex != cycle->pageIndex || !a_setting.value.is_number_integer() || a_setting.value < 0 || a_setting.value >= cycle->valueCount) {
                    logger::warn("Skipping unknown or invalid cycling setting '{}'", a_setting.settingID);
                    return false;
                }
                applyAction = MakeStringAction(RestoreActionType::ApplyCycle, 0, a_setting.settingID);
                applyAction.integerValue = a_setting.value.get<int>();
                break;
            }
            case ControlType::Option: {
                if (a_setting.command) {
                    applyAction = MakeOptionAction(RestoreActionType::ApplyCommand, 0, a_setting.selection.optionIndex);
                    break;
                }
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
                applyAction.valueText = a_setting.valueText;
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
                    applyAction = MakeKeymapAction(0, a_setting.selection.optionIndex, desiredKeyCode);
                }
                break;
            }
            default:
                return false;
        }

        // Add the page call first, then the control specific calls.
        size_t mcmIndex = GetOrAddMCM(a_setting.selection.identity);
        settingChangedAction.mcmIndex = mcmIndex;
        for (auto* action : { &requestAction, &applyAction }) {
            action->mcmIndex = mcmIndex;
            action->controlType = a_setting.type;
            action->optionIndex = a_setting.selection.optionIndex;
            action->optionLabel = a_setting.optionLabel;
            action->stateName = a_setting.stateName;
            action->pageName = a_setting.selection.pageName;
            action->pageIndex = a_setting.selection.pageIndex;
            action->command = a_setting.command;
            action->confirmedCommand = a_setting.confirmedCommand;
        }
        if (a_setting.reopensConfig && !restoreMCMs[mcmIndex].settingActions.empty()) {
            // The user left this MCM and came back before making this change, so its handler ran
            // on close. Replay that break first before applying the new setting.
            AddReopenActions(mcmIndex);
        }
        AddPageAction(mcmIndex, a_setting.selection);
        if (hasRequest) {
            restoreMCMs[mcmIndex].settingActions.push_back(std::move(requestAction));
        }
        restoreMCMs[mcmIndex].settingActions.push_back(std::move(applyAction));
        if (hasSettingChangedAction) {
            restoreMCMs[mcmIndex].settingActions.push_back(std::move(settingChangedAction));
        }
        const bool vioLensRefresh = VioLensSupport::IsSupported(a_setting.selection.identity.modID) && a_setting.type != ControlType::Cycle;
        const bool recordedMenu = a_setting.recorded && a_setting.type == ControlType::Menu;
        if (a_setting.command || a_setting.rebuildsPage || recordedMenu || vioLensRefresh) {
            // A recorded dropdown may rebuild after capture's page hash was read.
            // Selecting the page again gives the following action fresh buffers to verify against.
            // Cycling actions already refresh their own page after each click.
            restoreMCMs[mcmIndex].hasQueuedPage = false;
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

        MCMFilter recordedMCMs;
        MCMFilter commandMCMs;
        for (const auto& setting : profile.settings) {
            if (setting.recorded && setting.command && !IsProfileWriteCommand(setting.optionLabel, setting.stateName) && !ContainsMCMID(commandMCMs, setting.selection.identity.modID)) {
                commandMCMs.push_back(setting.selection.identity.modID);
            }
        }
        for (const auto& [modID, mode] : profile.mods) {
            if (mode == ProfileMode::Action) {
                recordedMCMs.push_back(modID);
            }
        }

        // A recorded order already describes these dependencies and must not be sorted again.
        VioLensSupport::OrderSettings(profile.settings, recordedMCMs);
        size_t supportedSettingCount{};
        size_t excludedSettingCount{};
        MCMFilter loggedExclusions;

        // Recorded settings first, since only they entail a real order. Scanned ones follow,
        // against a page the recorded steps have already unlocked.
        for (const bool recordedRound : { true, false }) {
            for (const auto& setting : profile.settings) {
                const auto& modID = setting.selection.identity.modID;
                const bool scanned = profile.IsActionMode(modID) && !setting.recorded;
                // Commands may replace the whole configuration. Replay the captured sequence last.
                const bool scannedFirst = ContainsMCMID(commandMCMs, modID);
                if (scanned == (scannedFirst ? !recordedRound : recordedRound)) {
                    continue;
                }
                const bool selected = AllowsMCM(mcmFilter, modID);
                const bool automaticEnabled = operationMode != OperationMode::Automatic || GetSettings().IsAutoRestoreEnabled(modID);
                if (!selected || !automaticEnabled) {
                    ++excludedSettingCount;
                    continue;
                }
                if (const auto reason = GetMCMExclusionReason(modID); !reason.empty()) {
                    if (!ContainsMCMID(loggedExclusions, modID)) {
                        logger::info("MCM restore skipped '{}': {}; saved settings are kept", modID, reason);
                        loggedExclusions.push_back(modID);
                    }
                    ++excludedSettingCount;
                    continue;
                }
                if (MCMCallWatch::IsUnavailable(modID)) {
                    if (!ContainsMCMID(loggedExclusions, modID)) {
                        logger::warn("MCM restore skipped '{}' because its script was unresponsive earlier in this game session", modID);
                        loggedExclusions.push_back(modID);
                    }
                    ++excludedSettingCount;
                    continue;
                }
                if (AddSettingActions(setting)) {
                    ++supportedSettingCount;
                }
            }
        }

        size_t activationCount{};
        for (const auto& activation : profile.activations) {
            const auto& modID = activation.selection.identity.modID;
            if (!AllowsMCM(mcmFilter, modID) || (operationMode == OperationMode::Automatic && !GetSettings().IsAutoRestoreEnabled(modID))) {
                continue;
            }
            if (const auto reason = GetMCMExclusionReason(modID); !reason.empty()) {
                if (!ContainsMCMID(loggedExclusions, modID)) {
                    logger::info("MCM restore skipped '{}': {}; saved activation is kept", modID, reason);
                    loggedExclusions.push_back(modID);
                }
                continue;
            }
            if (MCMCallWatch::IsUnavailable(modID)) {
                if (!ContainsMCMID(loggedExclusions, modID)) {
                    logger::warn("MCM restore skipped '{}' because its script was unresponsive earlier in this game session", modID);
                    loggedExclusions.push_back(modID);
                }
                continue;
            }
            if (IsActivationRecorded(profile.settings, activation)) {
                // The recorded order reaches the activation control on its own, so a separate step would repeat it.
                logger::info("MCM '{}' is enabled by its recorded order, so its detected activation step is skipped", modID);
                continue;
            }
            // Enabling an MCM is restorable even when no ordinary settings were captured.
            const size_t mcmIndex = GetOrAddMCM(activation.selection.identity);
            restoreMCMs[mcmIndex].activation = activation;
            ++activationCount;
        }

        logger::info("Loaded persistent profile with {} supported settings and {} activations across {} MCM configurations ({} settings excluded)", supportedSettingCount, activationCount, restoreMCMs.size(), excludedSettingCount);
        if (restoreMCMs.empty()) {
            logger::warn("Persistent profile contains no supported settings or activations to restore");
            return false;
        }
        
        return true;
    }
}
