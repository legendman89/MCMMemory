// Authored by Wuerfelhusten to support MCM bridge

#include "profile/capture.hpp"
#include "mcm/mcm_script.hpp"

namespace MCMMemory
{

    std::string Text(const char* a_text) { return a_text ? a_text : ""; }

    ControlType HostType(uint32_t a_type)
    {
        switch (a_type) {
        case MCM_HOST_TOGGLE: return ControlType::Option;
        case MCM_HOST_SLIDER: return ControlType::Slider;
        case MCM_HOST_MENU: return ControlType::Menu;
        case MCM_HOST_COLOR: return ControlType::Color;
        case MCM_HOST_KEYMAP: return ControlType::Keymap;
        case MCM_HOST_INPUT: return ControlType::Input;
        default: return ControlType::Unknown;
        }
    }

    EventType HostEventType(uint32_t a_type)
    {
        switch (a_type) {
        case MCM_HOST_SLIDER: return EventType::SliderAccepted;
        case MCM_HOST_MENU: return EventType::MenuAccepted;
        case MCM_HOST_COLOR: return EventType::ColorAccepted;
        case MCM_HOST_KEYMAP: return EventType::KeymapChanged;
        case MCM_HOST_INPUT: return EventType::InputAccepted;
        default: return EventType::OptionSelected;
        }
    }

    bool Capture::InstallHostCapture()
    {
        hostPresent = true;
        // Bridge provides no Classic UI events, even if subscribing fails.
        const auto module = GetModuleHandleW(L"MCMBridge.dll");
        const auto getHost = module ? reinterpret_cast<MCMBridgeGetHost>(GetProcAddress(module, "MCMBridge_GetHost")) : nullptr;
        const auto* api = getHost ? getHost() : nullptr;
        if (!api || !api->subscribe || !api->unsubscribe) {
            logger::error("MCMBridge recording interface is unavailable; Classic capture is disabled");
            return false;
        }
        const auto result = api->subscribe([](void* a_user, const MCMHostEvent* a_event) {
            if (a_event) {
                static_cast<Capture*>(a_user)->HostEvent(*a_event);
            }
        }, this, &hostSubscription);
        installed = result == MCM_HOST_OK;
        if (!installed) {
            logger::error("Could not subscribe to MCMBridge recording (result {})", result);
        }
        return installed;
    }

    void Capture::HostEvent(const MCMHostEvent& a_event)
    {
        std::scoped_lock lock(captureMutex);
        if (a_event.type == MCM_HOST_SESSION_BEGIN) {
            hostSession = a_event.session;
            ++configSession;
            return;
        }
        if (a_event.type == MCM_HOST_SESSION_END) {
            SaveProfileChanges();
            return;
        }
        if (a_event.type == MCM_HOST_USER_CHANGING) {
            if (a_event.session == hostSession && a_event.change_id) {
                BeginHostCapture(a_event);
            }
            return;
        }
        // Edits can finish after CloseConfig; use their saved profile and session.
        if (a_event.type == MCM_HOST_USER_REJECTED) {
            hostCaptures.erase(a_event.change_id);
        }
        else if (a_event.type == MCM_HOST_USER_CHANGE) {
            FinishHostCapture(a_event);
        }
    }

    void Capture::BeginHostCapture(const MCMHostEvent& a_event)
    {
        HostCapture pending;
        // Copy event strings and save the profile before the page changes or closes.
        auto& record = pending.record;
        record.profileName = GetSettings().activeProfile;
        record.configSession = configSession;
        record.eventID = ++eventCount;
        record.type = HostEventType(a_event.control_type);
        record.selection.identity = { Text(a_event.mod_name), Text(a_event.mcm_id) };
        record.selection.pageName = Text(a_event.page_name);
        record.selection.pageIndex = a_event.page_index;
        record.selection.optionIndex = a_event.option_index;
        record.pageScopedState = a_event.page_scoped_state != 0;
        record.capturePending = true;
        record.control = MCMControl{ Text(a_event.label), Text(a_event.state_name), Text(a_event.value_text), HostType(a_event.control_type) };
        if (a_event.control_type == MCM_HOST_TOGGLE && a_event.value.type == MCM_HOST_INTEGER) {
            record.control->toggleValue = a_event.value.integer != 0;
        }
        if (ShouldSkipCapture(record)) {
            return;
        }
        auto entry = MCMRegistry().ReadActiveMCM();
        if (entry && entry->identity.modID == record.selection.identity.modID) {
            pending.script = entry->mcmScript;
            MCMScript script(pending.script);
            record.pageHash = script.ReadPageHash();
            pending.setting.rowLabel = script.ReadRowLabel(a_event.option_index);
            if (auto control = script.ReadControl(a_event.option_index); control && control->stateName == record.control->stateName) {
                // Use Memory's existing cycle detection.
                if (control->type == ControlType::Cycle) {
                    record.control->type = ControlType::Cycle;
                }
            }
            if (record.type == EventType::OptionSelected && a_event.intent != 2) {
                pending.activation = MCMActivationSupport::ReadSelectedState(script, record.selection.identity,
                    record.selection.pageName, record.selection.pageIndex, record.selection.optionIndex, *record.control);
            }
        }
        auto& setting = pending.setting;
        setting.selection = record.selection;
        setting.optionLabel = record.control->optionLabel;
        if (!setting.optionLabel.empty()) {
            setting.rowLabel = {};
        }
        setting.stateName = record.control->stateName;
        setting.settingID = Text(a_event.setting_id);
        setting.sourceEventID = record.eventID;
        setting.type = record.control->type;
        setting.pageScopedState = record.pageScopedState;
        hostCaptures.insert_or_assign(a_event.change_id, std::move(pending));
    }

    void Capture::FinishHostCapture(const MCMHostEvent& a_event)
    {
        const auto found = hostCaptures.find(a_event.change_id);
        if (found == hostCaptures.end()) {
            return;
        }
        auto pending = std::move(found->second);
        hostCaptures.erase(found);
        auto& record = pending.record;
        record.confirmationAccepted = a_event.confirmation_accepted != 0;
        record.confirmationCancelled = a_event.confirmation_declined != 0;
        if (ShouldSkipCapture(record)) {
            return;
        }
        if (pending.activation) {
            auto& activation = *pending.activation;
            activation.enabled = !activation.enabled;
            RememberActivation(activation);
            if (!GetSettings().recordActions) {
                if (GetSettings().autoBackup && ProfileStorage::UpdateActivation(record.profileName, activation.activation, activation.enabled)) {
                    DelayProfileSave();
                }
                return;
            }
        }
        auto& setting = pending.setting;
        MCMScript script(pending.script);
        const auto currentHash = pending.script ? script.ReadPageHash() : std::nullopt;
        setting.rebuildsPage = record.pageHash && currentHash && *record.pageHash != *currentHash;
        setting.valueText = Text(a_event.value_text);
        setting.valueSource = "host.userChange";
        if (a_event.value.type == MCM_HOST_INTEGER) {
            if (a_event.control_type == MCM_HOST_TOGGLE) {
                setting.value = a_event.value.integer != 0;
            }
            else {
                setting.value = a_event.value.integer;
            }
        }
        else if (a_event.value.type == MCM_HOST_FLOAT) {
            setting.value = a_event.value.number;
        }
        else if (a_event.value.type == MCM_HOST_STRING) {
            setting.value = Text(a_event.value.text);
        }
        if (a_event.control_type == MCM_HOST_TEXT) {
            if (pending.script && SkyUICycleSupport::ReadSetting(script, setting)) {
                // Keep numeric values for known cycle controls.
            }
            else if (IsRecordableTextSetting(record) && setting.value.is_string() && setting.value.get<std::string>() != record.control->valueText) {
                setting.type = ControlType::Cycle;
                setting.textControl = true;
            }
            else if (CanRecordCommand(record)) {
                SetCapturedCommand(setting, record.confirmationAccepted);
            }
            else {
                return;
            }
        }
        else if (a_event.control_type == MCM_HOST_TOGGLE && CanRecordCommand(record) && record.control->toggleValue &&
                 setting.value == nlohmann::json(*record.control->toggleValue)) {
            SetCapturedCommand(setting, record.confirmationAccepted);
        }
        const bool menuSetting = setting.type == ControlType::Menu;
        if (menuSetting && GetSettings().recordActions && !IsProfileWriteCommand(setting.optionLabel, setting.stateName)) {
            // Use the same command rules as Classic capture.
            setting.command = MCMCommandSupport::IsIgnored(setting.selection.identity.modID, setting.selection.pageName, setting.selection.pageIndex, setting.type, setting.stateName, setting.optionLabel) || ContainsCaseInsensitive(setting.optionLabel, "Load") || ContainsCaseInsensitive(setting.stateName, "Load") || ContainsCaseInsensitive(setting.optionLabel, "Apply") || ContainsCaseInsensitive(setting.stateName, "Import");
            setting.confirmedCommand = setting.command && record.confirmationAccepted;
            setting.recorded = setting.command;
        }
        if (((setting.command || menuSetting) && IsProfileWriteCommand(setting.optionLabel, setting.stateName)) ||
            (!setting.command && MCMCommandSupport::IsIgnored(setting.selection.identity.modID, setting.selection.pageName, setting.selection.pageIndex, setting.type, setting.stateName, setting.optionLabel))) {
            return;
        }
        if (pending.script && setting.type == ControlType::Keymap) {
            MCMHelperSupport::GetSingleton()->ReadKeymapSetting(pending.script, setting);
        }
        if (setting.value.is_null()) {
            return;
        }
        StoreCapturedSetting(record, std::move(setting));
        if (records.size() == 4096) {
            records.erase(records.begin());
        }
        records.push_back(std::move(record));
    }
}
