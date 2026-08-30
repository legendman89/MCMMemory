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

    void Capture::ProcessCapturedEvent(CaptureRecord& a_record)
    {
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
        if (activeMCMScript) {
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

        // Do not take a label or value from a different highlighted option.
        const auto& state = a_record.stateAfter;
        bool cursorMatches = false;
        if (state.contains("fields")) {
            auto cursorIndex = JSON::ReadNumber(state["fields"], "OptionCursorIndex");
            cursorMatches = cursorIndex && static_cast<int>(*cursorIndex) == setting.selection.optionIndex;
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
            case EventType::OptionSelected:
            case EventType::OptionDefaulted:
                // numArg is the option index. The new toggle value is read after the menu updates.
                if (cursorMatches && state.contains("optionCursorMembers")) {
                    auto value = JSON::ReadNumber(state["optionCursorMembers"], "numValue");
                    if (value) {
                        setting.value = *value;
                        setting.valueSource = "stateAfter.optionCursor.numValue";
                    }
                }
                if (cursorMatches && setting.valueSource.empty() && state.contains("fields")) {
                    auto value = JSON::ReadNumber(state["fields"], "OptionCursorNumberValue");
                    if (value) {
                        setting.value = *value;
                        setting.valueSource = "stateAfter.OptionCursorNumberValue";
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
    }

}
