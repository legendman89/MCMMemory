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

    void Capture::RememberControl(CaptureRecord& a_record)
    {
        if (a_record.control) {
            return;
        }

        // A page reset may already be running. Prefer the identity read while hovering.
        for (auto previous = records.rbegin(); previous != records.rend(); ++previous) {
            if (previous->eventID >= a_record.eventID) {
                continue;
            }
            if (previous->eventID <= menuOpenedEventID || Role(previous->type) == EventRole::Navigation || IsValueChange(previous->type)) {
                break;
            }
            if (previous->selection.modIndex == a_record.selection.modIndex && previous->selection.pageIndex == a_record.selection.pageIndex && previous->selection.optionIndex == a_record.selection.optionIndex && previous->control) {
                a_record.control = previous->control;
                return;
            }
        }

        if (!IsCapturePageCurrent(a_record)) {
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
            }
        }
    }

    bool Capture::ReadToggleSetting(CaptureRecord& a_record, const MCMScript& a_script, CapturedSetting& a_setting) const
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

        // SetToggleOptionValue updates the menu row, not the script's original page buffer.
        // Read the resolved row even if a redraw or mouse movement changed the cursor.
        auto& option = a_record.stateAfter["changedOptionMembers"];
        option = MCMMenu::ReadOption(*index);
        auto type = JSON::ReadNumber(option, "optionType");
        auto value = JSON::ReadNumber(option, "numValue");
        auto label = JSON::ReadString(option, "text");
        // SkyUI translates this text. FindControlIndex already checked the script's control identity.
        if (!type || *type != 3.0 || !value || !label) {
            return false;
        }

        a_setting.selection.optionIndex = *index;
        a_setting.optionLabel = a_record.control->optionLabel;
        a_setting.stateName = a_record.control->stateName;
        a_setting.value = *value != 0.0;
        a_setting.valueSource = "menu.option.numValue";
        logger::info("Finished toggle capture {}: mod='{}', option='{}', state='{}', value={}", a_record.eventID, a_setting.selection.identity.modName, a_setting.optionLabel, a_setting.stateName, a_setting.value.dump());
        return true;
    }

    bool Capture::ProcessCapturedEvent(CaptureRecord& a_record)
    {
        if (const auto reason = GetMCMExclusionReason(a_record.selection.identity.modID); !reason.empty()) {
            logger::debug("Skipped capture {}: {}", a_record.eventID, reason);
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
        setting.pageScopedState = NLMCMSupport::IsSupported(mcmScript);
        if (setting.pageScopedState && !IsCapturePageCurrent(a_record)) {
            logger::debug("Stopped NL_MCM capture {} after navigation or a newer change", a_record.eventID);
            return true;
        }
        if (setting.type == ControlType::Option) {
            RememberControl(a_record);
            // Text buttons send optionSelected too, but are not saved toggles.
            if (a_record.control && a_record.control->type != ControlType::Option) {
                return true;
            }
            if (!activeMCMScript || !ReadToggleSetting(a_record, mcmScript, setting)) {
                return false;
            }
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

        if (setting.optionLabel.empty()) {
            setting.optionLabel = ReadOptionLabel(a_record);
        }

        // Each control reports its accepted value in a different place.
        switch (a_record.type) {

            case EventType::SliderAccepted:
            case EventType::MenuAccepted:
            case EventType::ColorAccepted:
                // numArg is the accepted slider value, menu index or RGB color.
                setting.value = a_record.numberArgument;
                setting.valueSource = "event.numberArgument";
                break;
            case EventType::InputAccepted:
                // strArg is the text accepted in the input dialog.
                setting.value = a_record.stringArgument;
                setting.valueSource = "event.stringArgument";
                break;
            case EventType::KeymapChanged:
                // numArg is the option index here. Read the key after SkyUI updates the script buffer.
                if (activeMCMScript) {
                    auto keyCode = mcmScript.ReadCurrentValue(ControlType::Keymap, setting.selection.optionIndex);
                    if (keyCode && keyCode->is_number_integer()) {
                        setting.value = std::move(*keyCode);
                        setting.valueSource = "script._numValueBuf";
                    }
                }
                break;
            default:
                break;

        }

        if (activeMCMScript && setting.type == ControlType::Keymap) {
            MCMHelperSupport::GetSingleton()->ReadKeymapSetting(activeMCMScript, setting);
        }

        // Incomplete settings stay in Capture.json but not in the selected profile.
        setting.identityComplete = !setting.selection.identity.modName.empty() && !setting.selection.identity.modID.empty() && setting.selection.optionIndex >= 0 && !setting.optionLabel.empty() && setting.type != ControlType::Unknown && !setting.valueSource.empty();
        if (setting.identityComplete && GetSettings().autoBackup) {
            if (ProfileStorage::UpdateSetting(setting)) {
                Deduplicate(pendingAutoBackupSettings, setting);
            }
            else {
                logger::error("Failed to update captured setting '{}' in the persistent profile", setting.optionLabel);
            }
        }

        Deduplicate(settings, std::move(setting));
        
        return true;
    }

}
