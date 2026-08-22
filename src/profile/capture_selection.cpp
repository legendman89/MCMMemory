#include "profile/capture.hpp"
#include "utils/json.hpp"

namespace MCMMemory
{
    void Capture::UpdateSelectionFromEvent(EventType a_type, const SKSE::ModCallbackEvent& a_event)
    {
        // A callback has eventName, numArg, strArg and sender.
        // eventName decides what the other fields mean.
        switch (a_type) {
        case EventType::ModSelected: {
            selection = {};
            // numArg is the selected row in the MCM mod list.
            selection.modIndex = static_cast<int>(a_event.numArg);
            // Reuse an identity we already found for this menu index.
            if (auto identity = mcmIdentities.find(selection.modIndex); identity != mcmIdentities.end()) {
                selection.identity = identity->second;
            }
            break;
        }
        case EventType::PageSelected:
            // numArg is the page index and strArg is the page name.
            selection.pageIndex = static_cast<int>(a_event.numArg);
            selection.pageName = a_event.strArg.c_str();
            selection.optionIndex = -1;
            break;
        case EventType::OptionHighlighted:
        case EventType::OptionSelected:
        case EventType::OptionDefaulted:
        case EventType::KeymapChanged:
        case EventType::SliderSelected:
        case EventType::MenuSelected:
        case EventType::ColorSelected:
        case EventType::InputSelected:
            // numArg is the option index for row and dialog events.
            selection.optionIndex = static_cast<int>(a_event.numArg);
            break;
        case EventType::SliderAccepted:
        case EventType::MenuAccepted:
        case EventType::ColorAccepted:
        case EventType::InputAccepted:
        case EventType::DialogCanceled:
            // Keep the option index from the earlier Selected event. These events carry the result instead.
            break;
        default:
            break;
        }
    }

    void Capture::UpdateSelectionFromMenu(EventType a_type, const nlohmann::json& a_state)
    {
        // Menu fields give us readable mod and page names.
        if (a_state.contains("fields") && a_state["fields"].is_object()) {
            const auto& fields = a_state["fields"];
            if (fields.contains("ModListSelectedText") && fields["ModListSelectedText"].is_string()) {
                selection.identity.modName = fields["ModListSelectedText"].get<std::string>();
            }
            else if (fields.contains("ModListSelectedLabel") && fields["ModListSelectedLabel"].is_string()) {
                selection.identity.modName = fields["ModListSelectedLabel"].get<std::string>();
            }
            if (selection.pageName.empty() && fields.contains("PageListSelectedText") && fields["PageListSelectedText"].is_string()) {
                selection.pageName = fields["PageListSelectedText"].get<std::string>();
            }
        }

        // Some MCMs only expose their name through the panel title.
        if (selection.identity.modName.empty() && selection.modIndex >= 0 && a_type != EventType::ModSelected && a_state.contains("panelMembers")) {
            auto title = JSON::ReadString(a_state["panelMembers"], "_titleText");
            if (title && !title->empty() && *title != "MOD CONFIGURATION") {
                selection.identity.modName = *title;
            }
        }

        SyncMCMIdentity();
    }

    void Capture::FindActiveMCMIdentity(EventType a_type, const nlohmann::json& a_state)
    {
        if (selection.modIndex < 0 || a_type == EventType::ModSelected) {
            return;
        }

        auto previousName = selection.identity.modName;
        auto previousID = selection.identity.modID;
        UpdateSelectionFromMenu(a_type, a_state);

        // MCM Registry provides the stable ID that display text cannot give us.
        if (selection.identity.modID.empty()) {
            auto activeMCM = MCMRegistry().ReadActiveMCM();
            if (activeMCM) {
                selection.identity.modID = activeMCM->identity.modID;
                if (selection.identity.modName.empty()) {
                    selection.identity.modName = activeMCM->identity.modName;
                }
            }
        }

        SyncMCMIdentity();
        
        if (previousName.empty() && !selection.identity.modName.empty()) {
            logger::info("Locked MCM index {} to display name '{}'", selection.modIndex, selection.identity.modName);
        }
        if (previousID.empty() && !selection.identity.modID.empty()) {
            logger::info("Locked MCM index {} to stable ID '{}'", selection.modIndex, selection.identity.modID);
        }
    }

    void Capture::SyncMCMIdentity()
    {
        if (selection.modIndex < 0) {
            return;
        }

        // Keep the current selection and the identity cache in step.
        auto& identity = mcmIdentities[selection.modIndex];
        if (!selection.identity.modName.empty()) {
            identity.modName = selection.identity.modName;
        }
        else {
            selection.identity.modName = identity.modName;
        }
        if (!selection.identity.modID.empty()) {
            identity.modID = selection.identity.modID;
        }
        else {
            selection.identity.modID = identity.modID;
        }

        // Fill the identity into recent records made before it became available.
        for (auto record = records.rbegin(); record != records.rend(); ++record) {
            if (record->selection.modIndex != selection.modIndex) {
                break;
            }
            if (record->selection.identity.modName.empty()) {
                record->selection.identity.modName = identity.modName;
            }
            if (record->selection.identity.modID.empty()) {
                record->selection.identity.modID = identity.modID;
            }
        }
    }
}
