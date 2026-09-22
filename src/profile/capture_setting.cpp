#include "mcm/mcm_script.hpp"
#include "mcm/mcm_support.hpp"
#include "profile/capture.hpp"
#include "utils/json.hpp"

namespace MCMMemory
{
    std::string Capture::ReadOptionLabel(const nlohmann::json& a_state, int a_optionIndex, bool a_dialogControl) const
    {
        if (a_dialogControl && a_state.contains("panelMembers")) {
            auto dialogTitle = JSON::ReadString(a_state["panelMembers"], "_dialogTitleText");
            if (dialogTitle) {
                return *dialogTitle;
            }
        }

        if (!a_state.contains("fields")) {
            return {};
        }

        const auto& fields = a_state["fields"];
        auto cursorIndex = JSON::ReadNumber(fields, "OptionCursorIndex");
        if (!cursorIndex || static_cast<int>(*cursorIndex) != a_optionIndex) {
            return {};
        }

        if (a_state.contains("optionCursorMembers")) {
            auto cursorText = JSON::ReadString(a_state["optionCursorMembers"], "text");
            if (cursorText) {
                return *cursorText;
            }
        }

        auto cursorText = JSON::ReadString(fields, "OptionCursorText");
        return cursorText ? *cursorText : std::string{};
    }

    std::string Capture::ReadOptionLabel(const CaptureRecord& a_record) const
    {
        auto type = ControlTypeForEvent(a_record.type);
        auto dialogControl = type == ControlType::Slider || type == ControlType::Menu || type == ControlType::Color || type == ControlType::Input;
        auto label = ReadOptionLabel(a_record.stateAfter, a_record.selection.optionIndex, dialogControl);
        if (!label.empty()) {
            return label;
        }

        // Key dialogs can hide the cursor before the delayed read. Reuse the same earlier highlight.
        for (auto previous = records.rbegin(); previous != records.rend(); ++previous) {
            if (previous->eventID >= a_record.eventID || previous->selection.modIndex != a_record.selection.modIndex || previous->selection.pageIndex != a_record.selection.pageIndex || previous->selection.optionIndex != a_record.selection.optionIndex) {
                continue;
            }

            label = ReadOptionLabel(previous->stateAfter, a_record.selection.optionIndex, dialogControl);
            if (label.empty()) {
                label = ReadOptionLabel(previous->state, a_record.selection.optionIndex, dialogControl);
            }
            if (!label.empty()) {
                return label;
            }
        }

        return {};
    }

    bool Capture::IsCapturePageCurrent(const CaptureRecord& a_record) const
    {
        if (selection.modIndex != a_record.selection.modIndex || selection.pageIndex != a_record.selection.pageIndex || selection.pageName != a_record.selection.pageName) {
            return false;
        }

        for (auto later = records.rbegin(); later != records.rend() && later->eventID > a_record.eventID; ++later) {
            if (later->type == EventType::ModSelected || IsValueChange(later->type)) {
                return false;
            }
            if (later->type == EventType::PageSelected && (later->selection.pageIndex != a_record.selection.pageIndex || later->selection.pageName != a_record.selection.pageName)) {
                return false;
            }
        }
        return true;
    }

    void Capture::RememberControl(CaptureRecord& a_record, bool a_allowMenuRead)
    {
        if (a_record.control) {
            return;
        }

        // A page reset may already be running. Prefer the identity read while hovering.
        for (auto previous = records.rbegin(); previous != records.rend(); ++previous) {
            if (previous->eventID >= a_record.eventID) {
                continue;
            }
            if (previous->eventID <= menuOpenedEventID || Role(previous->type) == EventRole::Navigation) {
                break;
            }
            if (IsValueChange(previous->type)) {
                // Repeated clicks need the last captured value.
                if (previous->selection.modIndex == a_record.selection.modIndex && previous->selection.pageIndex == a_record.selection.pageIndex && previous->selection.optionIndex == a_record.selection.optionIndex && previous->control && previous->stateAfter.contains("changedOptionMembers")) {
                    const auto& option = previous->stateAfter["changedOptionMembers"];
                    auto type = JSON::ReadNumber(option, "optionType");
                    auto text = JSON::ReadString(option, "strValue");
                    auto number = JSON::ReadNumber(option, "numValue");
                    if (type && *type == static_cast<double>(SkyUIOptionType::Toggle) && number) {
                        a_record.control = previous->control;
                        a_record.pageScopedState = previous->pageScopedState;
                        a_record.control->toggleValue = *number != 0.0;
                        return;
                    }
                    if (type && *type == static_cast<double>(SkyUIOptionType::Text) && text && !text->empty()) {
                        a_record.control = previous->control;
                        a_record.pageScopedState = previous->pageScopedState;
                        a_record.control->valueText = std::move(*text);
                        return;
                    }
                }
                break;
            }
            if (previous->selection.modIndex == a_record.selection.modIndex && previous->selection.pageIndex == a_record.selection.pageIndex && previous->selection.optionIndex == a_record.selection.optionIndex && previous->control) {
                a_record.control = previous->control;
                a_record.pageScopedState = previous->pageScopedState;
                return;
            }
        }

        if (!a_allowMenuRead || !IsCapturePageCurrent(a_record)) {
            return;
        }
        
        for (auto later = records.rbegin(); later != records.rend() && later->eventID > a_record.eventID; ++later) {
            if (later->type == EventType::PageSelected) {
                return;
            }
        }

        auto activeMCM = MCMRegistry().ReadActiveMCM();
        if (activeMCM && activeMCM->identity.modID == a_record.selection.identity.modID) {
            MCMScript script(activeMCM->mcmScript);
            if (script.IsPageReady(a_record.selection.pageIndex)) {
                a_record.control = script.ReadControl(a_record.selection.optionIndex);
                a_record.pageScopedState = NLMCMSupport::IsSupported(script);
                if (a_record.control && a_record.control->type == ControlType::Option && !IsValueChange(a_record.type)) {
                    const auto option = MCMMenu::ReadOption(a_record.selection.optionIndex);
                    auto value = JSON::ReadNumber(option, "numValue");
                    if (value) {
                        a_record.control->toggleValue = *value != 0.0;
                    }
                }
                if (a_record.control && script.IsTextControl(a_record.selection.optionIndex)) {
                    const auto option = MCMMenu::ReadOption(a_record.selection.optionIndex);
                    auto text = JSON::ReadString(option, "strValue");
                    a_record.control->valueText = text.value_or("");
                }
            }
        }
    }

    bool Capture::ReadSelectedSetting(CaptureRecord& a_record, const MCMScript& a_script, CapturedSetting& a_setting) const
    {
        if (!a_record.control || !a_record.stateAfter.contains("fields")) {
            return false;
        }

        // The menu becomes ready only after the handler and any requested page reset finish.
        // This fix allows us to correctly capture the new toggle state after a page reset, 
        // instead of the old state before the reset.
        auto panelState = JSON::ReadNumber(a_record.stateAfter["fields"], "PanelState");
        bool pageResetRequested{};
        JSON::ReadValue(a_record.stateAfter["fields"], "PageResetRequested", pageResetRequested);
        if (!panelState || *panelState != 0.0 || pageResetRequested) {
            return false;
        }

        if (!a_script.IsPageReady(a_setting.selection.pageIndex)) {
            return false;
        }

        auto index = a_script.FindControlIndex(*a_record.control, a_setting.selection.optionIndex);
        if (!index) {
            return false;
        }

        a_setting.selection.optionIndex = *index;
        a_setting.optionLabel = a_record.control->optionLabel;
        a_setting.stateName = a_record.control->stateName;
        const bool recordCommand = CanRecordCommand(a_record);
        if (a_record.control->type == ControlType::Cycle) {
            if (!SkyUICycleSupport::ReadSetting(a_script, a_setting)) {
                return false;
            }
            logger::info("Finished cycling setting capture {}: mod: '{}', setting: '{}', value: {}", a_record.eventID, a_setting.selection.identity.modName, a_setting.settingID, a_setting.value.get<int>());
            return true;
        }

        // SkyUI updates the visible row without changing its original script value buffer.
        auto& option = a_record.stateAfter["changedOptionMembers"];
        option = MCMMenu::ReadOption(*index);
        auto type = JSON::ReadNumber(option, "optionType");
        auto label = JSON::ReadString(option, "text");
        if (!type || !label) {
            return false;
        }

        if (IsRecordableTextSetting(a_record) && a_script.IsTextControl(*index) && *type == static_cast<double>(SkyUIOptionType::Text)) {
            auto text = JSON::ReadString(option, "strValue");
            if ((!text || text->empty() || *text == a_record.control->valueText) && !recordCommand) {
                // A cycling setting advances its value when clicked. Unchanged means the handler is
                // still running, or this row is a command.
                return false;
            }
            if (text && !text->empty() && *text != a_record.control->valueText) {
                a_setting.type = ControlType::Cycle;
                a_setting.textControl = true;
                a_setting.value = *text;
                a_setting.valueSource = "menu.option.strValue";
                logger::info("Finished text setting capture {}: mod: '{}', option: '{}', value: '{}'", a_record.eventID, a_setting.selection.identity.modName, a_setting.optionLabel, *text);
                return true;
            }
        }

        // SetToggleOptionValue updates the menu row, not the script's original page buffer.
        // Read the row even if a redraw or mouse movement changed the cursor.
        auto value = JSON::ReadNumber(option, "numValue");
        const bool unchangedToggle = *type == static_cast<double>(SkyUIOptionType::Toggle) && value && a_record.control->toggleValue && (*value != 0.0) == *a_record.control->toggleValue;
        if (recordCommand && (*type == static_cast<double>(SkyUIOptionType::Text) || unchangedToggle)) {
            a_setting.type = a_record.control->type;
            SetCapturedCommand(a_setting, a_record.confirmationAccepted);
            logger::info("Finished command capture {}: mod: '{}', option: '{}'", a_record.eventID, a_setting.selection.identity.modName, a_setting.optionLabel);
            return true;
        }
        // SkyUI translates this text. FindControlIndex already checked the script's control identity.
        if (*type != 3.0 || !value) {
            return false;
        }

        const bool enabled = *value != 0.0;
        a_setting.value = enabled;
        a_setting.valueSource = "menu.option.numValue";
        logger::info("Finished toggle capture {}: mod: '{}', option: '{}', state: '{}', value: {}", a_record.eventID, a_setting.selection.identity.modName, a_setting.optionLabel, a_setting.stateName, enabled);
        return true;
    }

    bool Capture::CaptureMCMActivation(CaptureRecord& a_record, const MCMScript& a_script)
    {
        if (a_record.profileName != GetSettings().activeProfile) {
            return false;
        }
        if (a_record.activationEvent || a_record.type != EventType::OptionSelected || !a_record.control) {
            return false;
        }

        auto page = a_script.ReadCurrentPage();
        if (!page) {
            return false;
        }
        auto activation = MCMActivationSupport::ReadSelectedState(a_script, a_record.selection.identity, page->name, page->index, a_record.selection.optionIndex, *a_record.control);
        if (!activation) {
            return false;
        }

        // Instead of waiting to read the new state of the control, clicking on disabled means enabled,
        // so we remember that now because some activation controls close the MCM immediately.
        activation->enabled = !activation->enabled;
        RememberActivation(*activation);
        if (GetSettings().recordActions) {
            // Leave this click for action recording. 
            return false;
        }
        a_record.activationEvent = true;
        logger::info("Remembered MCM '{}' as {} for the next manual backup", a_record.selection.identity.modID, activation->enabled ? "enabled" : "disabled");
        if (GetSettings().autoBackup) {
            if (!ProfileStorage::UpdateActivation(a_record.profileName, activation->activation, activation->enabled)) {
                logger::error("Failed to update the activation state for '{}' in the persistent profile", a_record.selection.identity.modID);
                return true;
            }
            DelayProfileSave();
            logger::info("Automatic backup captured MCM '{}' as {}; profile will be saved after inactivity or when the journal closes", a_record.selection.identity.modID, activation->enabled ? "enabled" : "disabled");
        }
        return true;
    }

    void Capture::RememberActivation(const MCMActivationState& a_activation)
    {
        for (auto& detected : detectedActivations) {
            if (detected.activation.selection.identity.modID == a_activation.activation.selection.identity.modID) {
                detected = a_activation;
                return;
            }
        }
        detectedActivations.push_back(a_activation);
    }

    bool Capture::CapturePendingCommand(CaptureRecord& a_record)
    {
        if (!IsPendingTextClick(a_record) || !CanRecordCommand(a_record) || ShouldSkipCapture(a_record)) {
            return false;
        }
        const auto& control = *a_record.control;
        const auto& selection = a_record.selection;
        if (!HasControlIdentity(selection, control.optionLabel)) {
            return false;
        }

        // Initialization can hide its own control or wait for CloseConfig. No current menu read is needed.
        CapturedSetting setting;
        setting.sourceEventID = a_record.eventID;
        setting.selection = selection;
        setting.optionLabel = control.optionLabel;
        setting.stateName = control.stateName;
        setting.type = control.type;
        setting.pageScopedState = a_record.pageScopedState;
        SetCapturedCommand(setting, a_record.confirmationAccepted);
        StoreCapturedSetting(a_record, std::move(setting));
        logger::info("Captured command {} before leaving its control: mod: '{}', option: '{}'", a_record.eventID, selection.identity.modName, control.optionLabel);
        return true;
    }

    void Capture::CapturePendingCommands()
    {
        if (!GetSettings().recordActions) {
            return;
        }
        for (auto& record : records) {
            if (record.eventID > menuOpenedEventID && IsPendingTextClick(record)) {
                CapturePendingCommand(record);
                record.captureComplete = true;
                record.capturePending = false;
            }
        }
    }

    bool Capture::ShouldSkipCapture(const CaptureRecord& a_record) const
    {
        if (a_record.profileName != GetSettings().activeProfile) {
            return true;
        }
        if (MCMCommandSupport::IsExcludedPage(a_record.selection.identity.modID, a_record.selection.pageName, a_record.selection.pageIndex)) {
            return true;
        }

        if (const auto reason = GetMCMExclusionReason(a_record.selection.identity.modID); !reason.empty()) {
            logger::debug("Skipped capture {}: {}", a_record.eventID, reason);
            return true;
        }

        return a_record.activationEvent || a_record.confirmationCancelled;
    }

    bool Capture::ProcessCapturedEvent(CaptureRecord& a_record)
    {
        if (ShouldSkipCapture(a_record)) {
            return true;
        }

        // Turn a raw callback into one setting that can be restored later.
        CapturedSetting setting;
        setting.sourceEventID = a_record.eventID;
        setting.type = ControlTypeForEvent(a_record.type);
        setting.selection = a_record.selection;
        auto activeMCM = MCMRegistry().ReadActiveMCM();
        RE::BSTSmartPointer<RE::BSScript::Object> activeMCMScript;
        if (activeMCM && activeMCM->identity.modID == setting.selection.identity.modID) {
            activeMCMScript = activeMCM->mcmScript;
        }

        MCMScript mcmScript(activeMCMScript);
        const bool menuSetting = setting.type == ControlType::Menu;
        if (menuSetting) {
            if (!activeMCMScript || !IsCapturePageCurrent(a_record)) {
                // Do not mistake a command for a setting after leaving its page.
                return true;
            }
            RememberControl(a_record);
            if (!a_record.control || a_record.control->type != ControlType::Menu) {
                // Page and file commands can clear the current buffers before this read.
                // A translated dialog title alone cannot tell us which command ran.
                logger::debug("Ignored menu capture {} in '{}' without a confirmed control identity", a_record.eventID, setting.selection.identity.modID);
                return true;
            }
            // Preset dialogs can outlive the initial read. Save only after the handler finishes.
            if (!a_record.stateAfter.contains("fields")) {
                return false;
            }
            const auto& fields = a_record.stateAfter["fields"];
            auto panelState = JSON::ReadNumber(fields, "PanelState");
            bool pageResetRequested{};
            JSON::ReadValue(fields, "PageResetRequested", pageResetRequested);
            if (!panelState || *panelState != 0.0 || pageResetRequested || !mcmScript.IsPageReady(setting.selection.pageIndex)) {
                return false;
            }
        }

        setting.pageScopedState = NLMCMSupport::IsSupported(mcmScript);
        if (setting.pageScopedState && !IsCapturePageCurrent(a_record)) {
            logger::debug("Stopped NL_MCM capture {} after navigation or a newer change", a_record.eventID);
            return true;
        }
        if (setting.type == ControlType::Option) {
            RememberControl(a_record);
            // Only known cycling text settings may be saved; ordinary text buttons are commands.
            // While recording, a text row that shows its own value can be replayed by clicking it.
            const bool recordableText = IsRecordableTextSetting(a_record);
            const bool commandCandidate = CanRecordCommand(a_record);
            if (a_record.control && a_record.control->type != ControlType::Option && a_record.control->type != ControlType::Cycle && !recordableText && !commandCandidate) {
                return true;
            }
            if (!activeMCMScript || !ReadSelectedSetting(a_record, mcmScript, setting)) {
                return false;
            }
        }
        else if (menuSetting) {
            setting.optionLabel = a_record.control->optionLabel;
            setting.stateName = a_record.control->stateName;
        }
        else if (activeMCMScript) {
            auto optionLabel = mcmScript.ReadOptionLabel(setting.selection.optionIndex);
            if (optionLabel) {
                setting.optionLabel = std::move(*optionLabel);
            }

            auto stateName = mcmScript.ReadStateName(setting.selection.optionIndex);
            if (stateName) {
                setting.stateName = std::move(*stateName);
            }
        }

        if (menuSetting && GetSettings().recordActions && !IsProfileWriteCommand(setting.optionLabel, setting.stateName)) {
            // Menus on configuration pages can apply presets or batch changes.
            setting.command = MCMCommandSupport::IsIgnored(setting.selection.identity.modID, setting.selection.pageName, setting.selection.pageIndex, setting.type, setting.stateName, setting.optionLabel) || ContainsCaseInsensitive(setting.optionLabel, "Load") || ContainsCaseInsensitive(setting.stateName, "Load") || ContainsCaseInsensitive(setting.optionLabel, "Apply") || ContainsCaseInsensitive(setting.stateName, "Import");
            setting.confirmedCommand = setting.command && a_record.confirmationAccepted;
            setting.recorded = setting.command;
        }
        if (((setting.command || menuSetting) && IsProfileWriteCommand(setting.optionLabel, setting.stateName)) || (!setting.command && MCMCommandSupport::IsIgnored(setting.selection.identity.modID, setting.selection.pageName, setting.selection.pageIndex, setting.type, setting.stateName, setting.optionLabel))) {
            return true;
        }

        if (setting.optionLabel.empty()) {
            setting.optionLabel = ReadOptionLabel(a_record);
        }

        if (activeMCMScript && setting.type == ControlType::Keymap) {
            MCMHelperSupport::GetSingleton()->ReadKeymapSetting(activeMCMScript, setting);
        }

        if (a_record.pageHash) {
            // A changed hash means the handler rebuilt the page.
            auto currentHash = mcmScript.ReadPageHash();
            setting.rebuildsPage = currentHash && *currentHash != *a_record.pageHash;
        }

        // Each control reports its accepted value in a different place.
        switch (a_record.type) {

            case EventType::SliderAccepted:
            case EventType::ColorAccepted:
                // numArg is the accepted slider value or RGB color.
                setting.value = a_record.numberArgument;
                setting.valueSource = "event.numberArgument";
                break;
            case EventType::MenuAccepted: {
                setting.value = a_record.numberArgument;
                setting.valueSource = "event.numberArgument";
                // Save the text this row now shows, so restore can decide the menu is already correct
                // without asking SkyUI for its dialog data. Only a row that actually changed is
                // trusted. A mod that never calls SetMenuOptionValue still shows the value chosen
                // before this one, and storing that would let restore skip a menu it never set.
                RememberControl(a_record);
                if (activeMCMScript && a_record.control && !a_record.control->valueText.empty()) {
                    // A dropdown can move or replace its own row during a rebuild.
                    auto index = mcmScript.FindControlIndex(*a_record.control, setting.selection.optionIndex);
                    auto text = index ? mcmScript.ReadOptionText(*index) : std::nullopt;
                    if (text && !text->empty() && *text != a_record.control->valueText) {
                        setting.valueText = std::move(*text);
                    }
                }
                break;
            }
            case EventType::InputAccepted:
                // strArg is the text accepted in the input dialog.
                setting.value = a_record.stringArgument;
                setting.valueSource = "event.stringArgument";
                break;
            case EventType::KeymapChanged: {
                // SkyUI keeps the newly selected key here while the script handles the change.
                std::optional<double> keyCode;
                if (a_record.state.contains("fields")) {
                    keyCode = JSON::ReadNumber(a_record.state["fields"], "SelectedKeyCode");
                }
                if (!keyCode && a_record.stateAfter.contains("fields")) {
                    keyCode = JSON::ReadNumber(a_record.stateAfter["fields"], "SelectedKeyCode");
                }
                if (keyCode) {
                    setting.value = static_cast<int>(*keyCode);
                    setting.valueSource = "menu.selectedKeyCode";
                }
                else if (setting.valueSource.empty()) {
                    // SkyUI does not put the new key in its value buffer; the MCM has to do that from
                    // its own handler and many mods never do. Reading the buffer here would save the key
                    // before this change, so the old binding is kept instead of a wrong one.
                    logger::warn("Could not read the new key for '{}' in '{}'; its saved binding is left unchanged", setting.optionLabel, setting.selection.identity.modID);
                }
                break;
            }
            default:
                break;

        }

        StoreCapturedSetting(a_record, std::move(setting));
        return true;
    }

    void Capture::StoreCapturedSetting(CaptureRecord& a_record, CapturedSetting a_setting)
    {
        a_record.captureComplete = true;
        a_record.capturePending = false;
        // Incomplete settings stay in Capture.json but not in the selected profile.
        a_setting.identityComplete = HasControlIdentity(a_setting.selection, a_setting.optionLabel) &&
                                     (a_setting.type != ControlType::Unknown || a_setting.command) && !a_setting.valueSource.empty();

        if (a_setting.identityComplete && GetSettings().autoBackup) {
            const auto& modID = a_setting.selection.identity.modID;
            a_setting.reopensConfig = IsConfigReopened(a_record.profileName, modID, a_record.configSession);
            if (ProfileStorage::UpdateSetting(a_record.profileName, a_setting)) {
                recordedConfigSessions[a_record.profileName][modID] = a_record.configSession;
                Deduplicate(pendingAutoBackupSettings, a_setting);
                DelayProfileSave();
            }
            else {
                logger::error("Failed to update captured setting '{}' in the persistent profile", a_setting.optionLabel);
            }
        }

        Deduplicate(settings, std::move(a_setting));
    }

}
