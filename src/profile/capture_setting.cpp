#include "mcm/mcm_script.hpp"
#include "profile/capture.hpp"
#include "utils/json.hpp"

namespace MCMMemory
{
    void Capture::ProcessCapturedEvent(CaptureRecord& a_record)
    {
        // Turn a raw callback into one setting that can be restored later.
        CapturedSetting setting;
        setting.sourceEventID = a_record.eventID;
        setting.type = ControlTypeForEvent(a_record.type);
        setting.selection = a_record.selection;
        auto activeMCM = MCMRegistry().ReadActiveMCM();
        if (activeMCM && activeMCM->identity.modID == setting.selection.identity.modID) {
            auto stateName = MCMScript(activeMCM->mcmScript).ReadStateName(setting.selection.optionIndex);
            if (stateName) {
                setting.stateName = std::move(*stateName);
            }
        }

        // Dialog controls show their label in a different place from normal rows.
        const auto& state = a_record.stateAfter;
        auto dialogControl = setting.type == ControlType::Slider || setting.type == ControlType::Menu || setting.type == ControlType::Color || setting.type == ControlType::Input;
        if (dialogControl && state.contains("panelMembers")) {
            auto dialogTitle = JSON::ReadString(state["panelMembers"], "_dialogTitleText");
            if (dialogTitle) {
                setting.optionLabel = *dialogTitle;
            }
        }

        // Do not take a label or value from a different highlighted option.
        bool cursorMatches = false;
        if (state.contains("fields")) {
            auto cursorIndex = JSON::ReadNumber(state["fields"], "OptionCursorIndex");
            cursorMatches = cursorIndex && static_cast<int>(*cursorIndex) == setting.selection.optionIndex;
        }
        if (setting.optionLabel.empty() && cursorMatches && state.contains("optionCursorMembers")) {
            auto cursorText = JSON::ReadString(state["optionCursorMembers"], "text");
            if (cursorText) {
                setting.optionLabel = *cursorText;
            }
        }
        if (setting.optionLabel.empty() && cursorMatches && state.contains("fields")) {
            auto cursorText = JSON::ReadString(state["fields"], "OptionCursorText");
            if (cursorText) {
                setting.optionLabel = *cursorText;
            }
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
                // numArg is the option index here, not the key. SkyUI stores the key in SelectedKeyCode.
                if (a_record.state.contains("fields")) {
                    auto keyCode = JSON::ReadNumber(a_record.state["fields"], "SelectedKeyCode");
                    if (keyCode) {
                        setting.value = static_cast<int>(*keyCode);
                        setting.valueSource = "state.SelectedKeyCode";
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

        // Incomplete settings stay in Capture.json but not saved in Profile.json.
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
