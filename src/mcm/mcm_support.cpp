#include "mcm/mcm_support.hpp"

#include <SimpleIni.h>
#include <charconv>
#include <iterator>

namespace MCMMemory
{
    bool MCMCommandSupport::IsIgnoredPage(std::string_view a_pageName)
    {
        for (const auto term : ignoredMCMPageTerms) {
            if (ContainsCaseInsensitive(a_pageName, term)) {
                return true;
            }
        }
        return false;
    }

    bool MCMCommandSupport::IsIgnored(std::string_view a_modID, std::string_view a_pageName, int a_pageIndex, ControlType a_type, std::string_view a_stateName, std::string_view a_optionLabel)
    {
        if (IsIgnoredPage(a_pageName) || VioLensSupport::IsCommand(a_modID, a_stateName, a_pageIndex, a_optionLabel)) {
            return true;
        }
        for (const auto& control : ignoredMCMControls) {
            if (HasMCMScript(a_modID, control.scriptName) && a_pageIndex == control.pageIndex && a_type == control.type && EqualsCaseInsensitive(a_optionLabel, control.optionLabel)) {
                return true;
            }
        }
        if (a_type != ControlType::Unknown) {
            return false;
        }
        for (const auto term : ignoredMCMCommandTerms) {
            if (ContainsCaseInsensitive(a_optionLabel, term)) {
                return true;
            }
        }
        return false;
    }

    std::string MCMActivationSupport::MakeEnabledText(std::string_view a_text, const MCMActivationStatus& a_status)
    {
        for (size_t start = 0; start + a_status.disabledText.size() <= a_text.size(); ++start) {
            bool matches = true;
            for (size_t index = 0; index < a_status.disabledText.size(); ++index) {
                if (ToLowerASCII(static_cast<unsigned char>(a_text[start + index])) != ToLowerASCII(static_cast<unsigned char>(a_status.disabledText[index]))) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                std::string enabledText(a_text);
                enabledText.replace(start, a_status.disabledText.size(), a_status.enabledText);
                return enabledText;
            }
        }
        return {};
    }

    bool MCMActivationSupport::HasActivationName(const MCMControl& a_control)
    {
        for (const auto term : mcmActivationLabelTerms) {
            if (ContainsCaseInsensitive(a_control.optionLabel, term) || ContainsCaseInsensitive(a_control.stateName, term)) {
                return true;
            }
        }
        return false;
    }

    bool MCMActivationSupport::HasActivationStatusName(const MCMControl& a_control)
    {
        for (const auto& status : mcmActivationStatuses) {
            if (ContainsCaseInsensitive(a_control.optionLabel, status.enabledText) || ContainsCaseInsensitive(a_control.optionLabel, status.disabledText) || ContainsCaseInsensitive(a_control.stateName, status.enabledText) || ContainsCaseInsensitive(a_control.stateName, status.disabledText)) {
                return true;
            }
        }
        return false;
    }

    std::optional<bool> MCMActivationSupport::ReadCommandState(const MCMControl& a_control, const MCMIdentity& a_identity)
    {
        const auto displayText = GetDisplayText(a_control.optionLabel);
        const auto displayModName = GetDisplayModName(a_identity.modName);
        if (a_control.type != ControlType::Unknown || displayModName.empty() || !ContainsCaseInsensitive(displayText, displayModName)) {
            return std::nullopt;
        }

        for (const auto& command : mcmActivationCommands) {
            if (ContainsCaseInsensitiveWordStart(a_control.optionLabel, command.enableText) || ContainsCaseInsensitiveWordStart(displayText, command.enableText)) {
                return false;
            }
            if (ContainsCaseInsensitiveWordStart(a_control.optionLabel, command.disableText) || ContainsCaseInsensitiveWordStart(displayText, command.disableText)) {
                return true;
            }
        }
        return std::nullopt;
    }

    MCMActivation MCMActivationSupport::MakeActivation(const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, int a_optionIndex, const MCMControl& a_control)
    {
        MCMActivation activation;
        activation.selection.identity = a_identity;
        activation.selection.pageName = a_pageName;
        activation.selection.pageIndex = a_pageIndex;
        activation.selection.optionIndex = a_optionIndex;
        activation.optionLabel = a_control.optionLabel;
        activation.stateName = a_control.stateName;
        activation.type = a_control.type;
        return activation;
    }

    std::optional<MCMActivationState> MCMActivationSupport::ReadSelectedState(const MCMScript& a_script, const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, int a_optionIndex, const MCMControl& a_control)
    {
        if (!CanBeStaged(a_script)) {
            return std::nullopt;
        }

        auto commandState = ReadCommandState(a_control, a_identity);
        if (commandState) {
            MCMActivationState result;
            result.activation = MakeActivation(a_identity, a_pageName, a_pageIndex, a_optionIndex, a_control);
            result.activation.startCommand = true;
            result.enabled = *commandState;
            return result;
        }

        auto result = ReadState(a_script, a_identity, a_pageName, a_pageIndex);
        if (!result || !MatchesControl(a_control, result->activation)) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<MCMActivationState> MCMActivationSupport::ReadState(const MCMScript& a_script, const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex)
    {
        if (!CanBeStaged(a_script) || a_pageIndex < -1 || a_pageIndex > 0) {
            return std::nullopt;
        }

        for (int index = 0; index < 128; ++index) {
            auto control = a_script.ReadControl(index);
            if (!control || (control->type != ControlType::Unknown && control->type != ControlType::Option)) {
                continue;
            }

            auto commandState = ReadCommandState(*control, a_identity);
            if (!commandState && !HasActivationName(*control)) {
                continue;
            }

            if (control->type == ControlType::Option && !HasActivationStatusName(*control)) {
                continue;
            }

            MCMActivationState result;
            result.activation = MakeActivation(a_identity, a_pageName, a_pageIndex, index, *control);

            if (commandState) {
                result.activation.startCommand = true;
                result.enabled = *commandState;
                return result;
            }

            if (control->type == ControlType::Option) {
                auto value = a_script.ReadCurrentValue(ControlType::Option, index);
                if (value && value->is_boolean()) {
                    result.enabled = value->get<bool>();
                    return result;
                }
                continue;
            }

            auto text = a_script.ReadOptionText(index);
            if (!text) {
                continue;
            }

            for (const auto& status : mcmActivationStatuses) {
                const bool disabled = ContainsCaseInsensitive(*text, status.disabledText);
                const bool enabled = !disabled && ContainsCaseInsensitive(*text, status.enabledText);
                if (!enabled && !disabled) {
                    continue;
                }

                result.activation.enabledText = enabled ? std::move(*text) : MakeEnabledText(*text, status);
                result.enabled = enabled;
                if (!result.activation.enabledText.empty()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    std::optional<int> MCMActivationSupport::FindOption(const MCMScript& a_script, const MCMActivation& a_activation)
    {
        if (a_activation.startCommand) {
            for (int index = 0; index < 128; ++index) {
                auto control = a_script.ReadControl(index);
                if (control && ReadCommandState(*control, a_activation.selection.identity)) {
                    return index;
                }
            }
            return std::nullopt;
        }

        MCMControl control;
        control.optionLabel = a_activation.optionLabel;
        control.stateName = a_activation.stateName;
        control.type = a_activation.type;
        return a_script.FindControlIndex(control, a_activation.selection.optionIndex);
    }

    bool MCMActivationSupport::MatchesControl(const MCMControl& a_control, const MCMActivation& a_activation)
    {
        if (a_control.type != a_activation.type) {
            return false;
        }
        if (a_activation.startCommand) {
            return ReadCommandState(a_control, a_activation.selection.identity).has_value();
        }
        if (!a_control.stateName.empty() && !a_activation.stateName.empty()) {
            return a_control.stateName == a_activation.stateName;
        }
        return a_control.optionLabel == a_activation.optionLabel || HasActivationName(a_control);
    }

    std::optional<bool> MCMActivationSupport::IsEnabled(const MCMScript& a_script, const MCMActivation& a_activation)
    {
        auto index = FindOption(a_script, a_activation);
        if (a_activation.startCommand) {
            auto control = index ? a_script.ReadControl(*index) : std::nullopt;
            return control ? ReadCommandState(*control, a_activation.selection.identity) : std::nullopt;
        }
        if (a_activation.type == ControlType::Option) {
            auto value = index ? a_script.ReadCurrentValue(ControlType::Option, *index) : std::nullopt;
            return value && value->is_boolean() ? std::optional<bool>(value->get<bool>()) : std::nullopt;
        }
        auto text = index ? a_script.ReadOptionText(*index) : std::nullopt;
        return text ? std::optional<bool>(EqualsCaseInsensitive(*text, a_activation.enabledText)) : std::nullopt;
    }

    bool VioLensSupport::IsCommand(std::string_view a_modID, std::string_view a_stateName, int a_pageIndex, std::string_view a_optionLabel)
    {
        if (!IsSupported(a_modID)) {
            return false;
        }

        // File operations, batch edits and subpage navigation are not saved settings.
        constexpr std::array<std::string_view, 8> commands{
            "SaveMainProfileMenu", "LoadMainProfileMenu", "DeleteMainProfileMenu",
            "SaveCustomizeKillmoveProfileMenu", "LoadCustomizeKillmoveProfileMenu", "DeleteCustomizeKillmoveProfileMenu",
            "VL_SettingsMenu", "CustomizeWeaponMenu"
        };
        if (std::ranges::find(commands, a_stateName) != commands.end()) {
            return true;
        }

        // Disabled versions have no state; older captures may also contain the English label without '$'.
        if (a_optionLabel.starts_with('$')) {
            a_optionLabel.remove_prefix(1);
        }
        if (a_pageIndex == 2) {
            return a_optionLabel == "Page" || a_optionLabel == "Add/Remove";
        }
        if (a_pageIndex == 3) {
            return a_optionLabel == "Save" || a_optionLabel == "Load" || a_optionLabel == "Delete";
        }
        return false;
    }

    const MCMCycleSetting* SkyUICycleSupport::Find(std::string_view a_modID, std::string_view a_settingID)
    {
        for (const auto& setting : mcmCycleSettings) {
            if (HasMCMScript(a_modID, setting.scriptName) && setting.optionVariable == a_settingID) {
                return &setting;
            }
        }
        return nullptr;
    }

    std::optional<int> SkyUICycleSupport::FindOption(const MCMScript& a_script, const MCMCycleSetting& a_setting, bool a_requireEnabled)
    {
        auto page = a_script.ReadCurrentPage();
        if (!a_script.IsBasedOn(a_setting.scriptName) || !page || page->index != a_setting.pageIndex) {
            return std::nullopt;
        }
        auto option = a_script.ReadInteger(a_setting.optionVariable);
        // SkyUI IDs include the page number in their high byte; the buffer index does not.
        const int pageOffset = (a_setting.pageIndex + 1) * 256;
        if (!option || *option < pageOffset || *option >= pageOffset + 128) {
            return std::nullopt;
        }
        const int index = *option - pageOffset;
        auto flags = a_script.ReadNumber("_optionFlagsBuf", static_cast<size_t>(index));
        auto label = a_script.ReadOptionLabel(index);
        if (!flags || !label || *label != a_setting.optionLabel) {
            return std::nullopt;
        }
        if (static_cast<int>(*flags) == 2) {
            return index;
        }

        // Only the real disabled camera row is readable. Reject the "$Disabled" placeholder,
        // which can occupy the same slot while the script still holds an old option ID.
        if (!a_requireEnabled && a_setting.readableWhenDisabled && static_cast<int>(*flags) == 258) {
            auto value = ReadValue(a_script, a_setting);
            auto text = a_script.ReadString("_strValueBuf", static_cast<size_t>(index));
            auto expected = value && !a_setting.valueTextArray.empty() ? a_script.ReadString(a_setting.valueTextArray, static_cast<size_t>(*value)) : std::nullopt;
            if (text && expected && *text == *expected) {
                return index;
            }
        }
        return std::nullopt;
    }

    const MCMCycleSetting* SkyUICycleSupport::Find(const MCMScript& a_script, int a_optionIndex)
    {
        if (a_optionIndex < 0) {
            return nullptr;
        }
        for (const auto& setting : mcmCycleSettings) {
            if (a_script.IsBasedOn(setting.scriptName) && FindOption(a_script, setting) == a_optionIndex) {
                return &setting;
            }
        }
        return nullptr;
    }

    std::optional<int> SkyUICycleSupport::ReadValue(const MCMScript& a_script, const MCMCycleSetting& a_setting)
    {
        if (!a_script.IsBasedOn(a_setting.scriptName)) {
            return std::nullopt;
        }
        std::optional<int> rawValue;
        if (a_setting.valueSource == MCMCycleValueSource::GlobalVariable) {
            auto value = a_script.ReadGlobalValue(a_setting.valueVariable);
            if (value) {
                rawValue = static_cast<int>(*value);
            }
        }
        else {
            rawValue = a_script.ReadInteger(a_setting.valueVariable);
        }
        if (!rawValue) {
            return std::nullopt;
        }
        for (int index = 0; index < a_setting.valueCount; ++index) {
            if (a_setting.values[static_cast<size_t>(index)] == *rawValue) {
                return index;
            }
        }
        return std::nullopt;
    }

    bool SkyUICycleSupport::ReadSetting(const MCMScript& a_script, CapturedSetting& a_setting)
    {
        const auto* cycle = Find(a_script, a_setting.selection.optionIndex);
        if (!cycle) {
            return false;
        }
        auto value = ReadValue(a_script, *cycle);
        if (!value) {
            return false;
        }
        a_setting.type = ControlType::Cycle;
        a_setting.settingID = cycle->optionVariable;
        a_setting.optionLabel = cycle->profileLabel;
        a_setting.value = *value;
        a_setting.valueSource = std::format("{}.{}", cycle->valueSource == MCMCycleValueSource::GlobalVariable ? "global" : "script", cycle->valueVariable);
        return true;
    }

    bool VioLensSettingOrder::operator()(const CapturedSetting& a_left, const CapturedSetting& a_right) const
    {
        if (a_left.selection.identity.modID != a_right.selection.identity.modID) {
            return a_left.selection.identity.modID < a_right.selection.identity.modID;
        }
        if (a_left.selection.pageIndex != a_right.selection.pageIndex) {
            return a_left.selection.pageIndex < a_right.selection.pageIndex;
        }
        return VioLensSupport::RestoreOrder(a_left) < VioLensSupport::RestoreOrder(a_right);
    }

    void VioLensSupport::OrderSettings(std::vector<CapturedSetting>& a_settings)
    {
        std::vector<size_t> positions;
        std::vector<CapturedSetting> settings;
        for (size_t index = 0; index < a_settings.size(); ++index) {
            if (IsSupported(a_settings[index].selection.identity.modID)) {
                positions.push_back(index);
                settings.push_back(std::move(a_settings[index]));
            }
        }
        // Selection Mode resets ranged Killmoves. Apply the mode first, even in older profiles.
        std::stable_sort(settings.begin(), settings.end(), VioLensSettingOrder{});
        for (size_t index = 0; index < positions.size(); ++index) {
            a_settings[positions[index]] = std::move(settings[index]);
        }
    }

    void MCMKickerSupport::Install()
    {
        kickerQuest = RE::TESForm::LookupByEditorID<RE::TESQuest>("JaxonzMCMKicker");
        managerQuest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance");
        if (!kickerQuest || installed) {
            return;
        }
        auto* events = SKSE::GetModCallbackEventSource();
        if (!events) {
            logger::error("MCM Kicker support could not find the registration event source");
            return;
        }
        events->AddEventSink(this);
        installed = true;
        logger::info("Jaxonz MCM Kicker detected, scripted operations will wait for its reset and registration");
    }

    void MCMKickerSupport::Reset()
    {
        std::lock_guard lock(kickerMutex);
        ++loadedGameSession;
        ++cacheGeneration;
        registryWait.Reset();
        resetDetected = false;
        status = !kickerQuest ? Status::Inactive : installed ? Status::Waiting : Status::Failed;
        if (status == Status::Waiting) {
            logger::info("Waiting for MCM Kicker to reset the registry in this game session");
            QueueCheck();
        }
    }

    bool MCMKickerSupport::IsKickDue() const
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!policy || !kickerQuest) {
            return false;
        }
        for (auto* alias : kickerQuest->aliases) {
            if (!alias) {
                continue;
            }
            const auto handle = policy->GetHandleForObject(alias->GetVMTypeID(), alias);
            RE::BSTSmartPointer<RE::BSScript::Object> script;
            if (handle == policy->EmptyHandle() || !vm->FindBoundObject(handle, "JaxonzMCMKicker", script) || !script) {
                continue;
            }
            const auto* elapsed = script->GetVariable("iWaitSeconds");
            const auto* delay = script->GetVariable("iMCMregdelay");
            const auto* limit = script->GetVariable("iMaxWait");
            const auto* ready = script->GetVariable("bMCMready");
            if (elapsed && elapsed->IsInt() && delay && delay->IsInt() && limit && limit->IsInt() && ready && ready->IsBool()) {
                // This is Kicker own condition.
                return (ready->GetBool() && elapsed->GetSInt() == delay->GetSInt()) || elapsed->GetSInt() == limit->GetSInt();
            }
        }
        return false;
    }

    RE::BSEventNotifyControl MCMKickerSupport::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
    {
        if (!a_event || a_event->eventName != "SKICP_configManagerReset" || a_event->sender != managerQuest) {
            return RE::BSEventNotifyControl::kContinue;
        }
        std::lock_guard lock(kickerMutex);
        if ((status != Status::Waiting && status != Status::Failed) || !IsKickDue()) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (status == Status::Failed) {
            // A late reset can recover support without restarting Skyrim.
            registryWait.Reset();
            status = Status::Waiting;
            QueueCheck();
        }
        resetDetected = true;
        registryWait.modIDs.clear();
        registryWait.quietCheckCount = 0;
        ++cacheGeneration;
        // A cached registry may still describe the menus from before the reset.
        if (MCMRegistry::IsMCMMenuRedoneAvailable()) {
            MCMMenuRedoneRegistry::GetSingleton()->Reset();
        }
        else if (MCMRegistry::IsMCMMenuMaidAvailable()) {
            MCMMenuMaidRegistry::GetSingleton()->Reset();
        }
        logger::info("MCM Kicker reset detected; waiting for the rebuilt registry to settle");
        return RE::BSEventNotifyControl::kContinue;
    }

    void MCMKickerSupport::QueueCheck()
    {
        if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(CheckTask{ loadedGameSession }, registryCheckDelaySeconds)) {
            status = Status::Failed;
            ++cacheGeneration;
            logger::error("MCM Kicker support could not schedule its registry check");
        }
    }

    void MCMKickerSupport::Check(uint64_t a_loadedGameSession)
    {
        std::lock_guard lock(kickerMutex);
        if (a_loadedGameSession != loadedGameSession || status != Status::Waiting || !IsGameLoaded()) {
            return;
        }
        auto* ui = RE::UI::GetSingleton();
        if (ui && (ui->GameIsPaused() || ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME))) {
            // Papyrus update timers can pause with menus. Do not charge that time to registration.
            QueueCheck();
            return;
        }
        std::vector<MCMRegistryEntry> mcms;
        if (resetDetected) {
            mcms = MCMRegistry().ReadRegisteredMCMs();
            if (MCMRegistry::IsRefreshing()) {
                mcms.clear();
            }
        }
        const auto result = registryWait.Update(mcms);
        if (result == RegistryWaitResult::Ready) {
            status = Status::Ready;
            ++cacheGeneration;
            logger::info("MCM Kicker registration settled with {} MCMs; scripted operations may start", mcms.size());
            return;
        }
        if (result == RegistryWaitResult::Expired) {
            status = Status::Failed;
            ++cacheGeneration;
            logger::error("MCM Kicker registration did not finish after {} checks (reset detected: {}); scripted operations will not use an unconfirmed registry", registryWait.checkCount, resetDetected);
            return;
        }
        if (resetDetected && result == RegistryWaitResult::Changed) {
            logger::info("MCM Kicker rebuilt registry contains {} MCMs; checking stability", mcms.size());
        }
        if (resetDetected) {
            MCMRegistry::Refresh();
        }
        QueueCheck();
    }

    std::optional<std::string> MCMHelperSupport::ReadConfigModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript) const
    {
        if (!IsMCMHelperScript(a_mcmScript)) {
            return std::nullopt;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        auto* form = policy && a_mcmScript ? policy->GetObjectForHandle(RE::FormType::Quest, a_mcmScript->GetHandle()) : nullptr;
        const auto* files = form ? form->sourceFiles.array : nullptr;
        const auto* file = files && !files->empty() ? files->front() : nullptr;
        if (!file || file->GetFilename().empty()) {
            return std::nullopt;
        }
        return ToUTF8(std::filesystem::path(file->GetFilename()).stem());
    }

    MCMHelperConfig* MCMHelperSupport::GetConfig(std::string_view a_modName)
    {
        auto existing = configs.find(std::string(a_modName));
        if (existing != configs.end()) {
            return std::addressof(existing->second);
        }

        auto config = LoadConfig(a_modName);
        if (!config) {
            return nullptr;
        }
        auto configName = config->modName;
        auto [inserted, added] = configs.emplace(std::move(configName), std::move(*config));
        return added ? std::addressof(inserted->second) : nullptr;
    }

    std::optional<MCMHelperConfig> MCMHelperSupport::LoadConfig(std::string_view a_modName) const
    {
        MCMHelperConfig config;
        config.modName = a_modName;
        config.directory = GetGameDataPath() / "MCM" / "Config" / FromUTF8(a_modName);
        const auto path = config.directory / "config.json";
        std::ifstream stream(path);
        if (!stream) {
            return std::nullopt;
        }

        try {
            auto document = nlohmann::json::parse(stream);
            if (!document.is_object()) {
                return std::nullopt;
            }

            const auto defaultFillMode = document.value("cursorFillMode", std::string{ "leftToRight" });
            ReadPageSettings(document, {}, -1, defaultFillMode, config.settings);
            auto pages = document.find("pages");
            if (pages != document.end() && pages->is_array()) {
                for (size_t pageIndex = 0; pageIndex < pages->size(); ++pageIndex) {
                    const auto& page = (*pages)[pageIndex];
                    if (!page.is_object()) {
                        continue;
                    }
                    const auto pageName = page.value("pageDisplayName", std::string{});
                    ReadPageSettings(page, pageName, static_cast<int>(pageIndex), defaultFillMode, config.settings);
                }
            }
        }
        catch (const std::exception& error) {
            logger::debug("MCM Helper config '{}' could not be read: {}", ToUTF8(path), error.what());
            return std::nullopt;
        }

        return config.settings.empty() ? std::nullopt : std::optional<MCMHelperConfig>(std::move(config));
    }

    void MCMHelperSupport::ReadPageSettings(const nlohmann::json& a_page, std::string_view a_pageName, int a_pageIndex, std::string_view a_defaultFillMode, std::vector<MCMHelperSetting>& a_settings) const
    {
        auto content = a_page.find("content");
        if (content == a_page.end() || !content->is_array()) {
            return;
        }

        const auto fillMode = a_page.value("cursorFillMode", std::string(a_defaultFillMode));
        const int cursorStep = fillMode == "topToBottom" ? 2 : 1;
        int cursorPosition{};
        for (const auto& control : *content) {
            if (!control.is_object()) {
                continue;
            }
            auto position = control.find("position");
            if (position != control.end() && position->is_number_integer()) {
                cursorPosition = position->get<int>();
            }

            const auto typeName = control.value("type", std::string{});
            if (typeName == "hiddenToggle") {
                continue;
            }

            if (typeName == "keymap") {
                auto valueOptions = control.find("valueOptions");
                const auto sourceType = valueOptions != control.end() && valueOptions->is_object() ? valueOptions->value("sourceType", std::string{}) : std::string{};
                const auto id = control.value("id", std::string{});
                const auto label = control.value("text", std::string{});
                if (sourceType == "ModSettingInt" && !id.empty() && !label.empty()) {
                    MCMHelperSetting setting;
                    setting.id = id;
                    setting.label = label;
                    setting.pageName = a_pageName;
                    setting.pageIndex = a_pageIndex;
                    setting.optionIndex = cursorPosition;
                    setting.type = ControlType::Keymap;
                    a_settings.push_back(std::move(setting));
                }
            }
            cursorPosition += cursorStep;
        }
    }

    const MCMHelperSetting* MCMHelperSupport::FindSetting(const MCMHelperConfig& a_config, const CapturedSetting& a_setting) const
    {
        for (const auto& setting : a_config.settings) {
            if (setting.type == a_setting.type && setting.pageIndex == a_setting.selection.pageIndex && setting.pageName == a_setting.selection.pageName && setting.optionIndex == a_setting.selection.optionIndex && setting.label == a_setting.optionLabel) {
                return std::addressof(setting);
            }
        }
        return nullptr;
    }

    std::optional<int> MCMHelperSupport::ReadInteger(const MCMHelperConfig& a_config, std::string_view a_settingID) const
    {
        const size_t separator = a_settingID.find(':');
        if (separator == std::string_view::npos || separator == 0 || separator + 1 >= a_settingID.size()) {
            return std::nullopt;
        }

        const std::string key(a_settingID.substr(0, separator));
        const std::string section(a_settingID.substr(separator + 1));
        const std::array paths
        {
            GetGameDataPath() / "MCM" / "Settings" / FromUTF8(std::format("{}.ini", a_config.modName)),
            a_config.directory / "settings.ini"
        };
        for (const auto& path : paths) {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) {
                continue;
            }

            std::string data;
            data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

            CSimpleIniA ini(true);
            if (ini.LoadData(data) < 0) {
                continue;
            }
            const char* text = ini.GetValue(section.c_str(), key.c_str());
            if (!text) {
                continue;
            }

            int value{};
            const std::string_view valueText(text);
            const auto result = std::from_chars(valueText.data(), valueText.data() + valueText.size(), value);
            if (result.ec == std::errc{}) {
                return value;
            }
        }
        return std::nullopt;
    }

    bool MCMHelperSupport::ReadKeymapSetting(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, CapturedSetting& a_setting)
    {
        if (a_setting.type != ControlType::Keymap) {
            return false;
        }

        auto modName = ReadConfigModName(a_mcmScript);
        auto* config = modName ? GetConfig(*modName) : nullptr;
        const auto* setting = config ? FindSetting(*config, a_setting) : nullptr;
        if (!setting) {
            return false;
        }

        a_setting.settingID = setting->id;
        auto value = ReadInteger(*config, setting->id);
        if (value) {
            a_setting.value = *value;
            a_setting.valueSource = "MCM Helper setting";
            logger::debug("Read MCM Helper keymap '{}' as {} from '{}'", setting->id, *value, config->modName);
        }
        return true;
    }

    void MCMRegistry::Install()
    {
        MCMKickerSupport::GetSingleton()->Install();
        if (IsMCMMenuRedoneAvailable()) {
            logger::info("MCM Menu Redone detected, its registry will be used");
        }
        else if (IsMCMMenuMaidAvailable()) {
            logger::info("Menu Maid 2 detected, its SkyUI-compatible registry will be used");
        }
        else if (IsMCMUnlockedAvailable()) {
            logger::info("MCM Unlocked detected, its marker registry will be used");
        }
        else {
            logger::info("SkyUI registry will be used");
        }
    }

    bool MCMRegistry::IsMCMMenuRedoneAvailable()
    {
        return GetModuleHandleW(L"MCMMenuRedone.dll") != nullptr;
    }

    bool MCMRegistry::IsMCMMenuMaidAvailable()
    {
        return GetModuleHandleW(L"MenuMaid2.dll") != nullptr;
    }

    bool MCMRegistry::UsesCachedRegistry()
    {
        return IsMCMMenuRedoneAvailable() || IsMCMMenuMaidAvailable();
    }

    bool MCMRegistry::IsMCMUnlockedAvailable()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler && dataHandler->LookupForm<RE::TESObjectACTI>(markerBaseLocalFormID, mcmUnlockedPluginName) && dataHandler->LookupForm<RE::TESObjectCELL>(markerCellLocalFormID, mcmUnlockedPluginName);
    }

    void MCMRegistry::TryAddMarker(std::vector<RE::NiPointer<RE::TESObjectREFR>>& a_markers, RE::TESObjectREFR* a_reference, const RE::TESBoundObject* a_markerBase)
    {
        if (!a_reference || a_reference->GetBaseObject() != a_markerBase) {
            return;
        }
        for (const auto& marker : a_markers) {
            if (marker.get() == a_reference) {
                return;
            }
        }
        a_markers.emplace_back(a_reference);
    }

    std::vector<RE::NiPointer<RE::TESObjectREFR>> MCMRegistry::CollectMCMMarkers(RE::TESObjectCELL* a_markerCell, const RE::TESBoundObject* a_markerBase)
    {
        std::vector<RE::NiPointer<RE::TESObjectREFR>> markers;
        auto& runtimeData = a_markerCell->GetRuntimeData();
        { // Trap the lock inside this scope.
            RE::BSSpinLockGuard lock(runtimeData.spinLock);
            markers.reserve(runtimeData.references.size() + runtimeData.objectList.size());
            for (const auto& reference : runtimeData.references) {
                TryAddMarker(markers, reference.get(), a_markerBase);
            }
            for (auto* reference : runtimeData.objectList) {
                TryAddMarker(markers, reference, a_markerBase);
            }
        }
        std::sort(markers.begin(), markers.end(), MCMMarkerFormIDLess());
        return markers;
    }

    std::optional<MCMRegistryEntry> MCMRegistry::ReadMCMFromMarker(RE::TESObjectREFR* a_marker, RE::BSScript::Internal::VirtualMachine* a_vm, RE::BSScript::IObjectHandlePolicy* a_policy)
    {
        auto handle = a_policy->GetHandleForObject(a_marker->GetFormType(), a_marker);
        if (handle == a_policy->EmptyHandle()) {
            return std::nullopt;
        }

        RE::BSTSmartPointer<RE::BSScript::Object> markerScript;
        if (!a_vm->FindBoundObject(handle, markerScriptName.data(), markerScript) || !markerScript) {
            return std::nullopt;
        }

        // InstanceScript points to the live MCM script represented by this marker.
        const RE::BSScript::Variable* mcmScriptValue = markerScript->GetProperty("InstanceScript");
        if (!mcmScriptValue || !mcmScriptValue->IsObject()) {
            mcmScriptValue = markerScript->GetVariable("::InstanceScript_var");
        }
        auto mcmScript = mcmScriptValue && mcmScriptValue->IsObject() ? mcmScriptValue->GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>();
        return CreateRegistryEntry(mcmScript);
    }

    std::vector<MCMRegistryEntry> MCMRegistry::ReadMCMUnlockedRegistry()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!dataHandler || !vm || !policy) {
            return {};
        }

        auto* markerBase = dataHandler->LookupForm<RE::TESObjectACTI>(markerBaseLocalFormID, mcmUnlockedPluginName);
        auto* markerCell = dataHandler->LookupForm<RE::TESObjectCELL>(markerCellLocalFormID, mcmUnlockedPluginName);
        if (!markerBase || !markerCell) {
            return {};
        }

        auto markers = CollectMCMMarkers(markerCell, markerBase);
        std::vector<MCMRegistryEntry> registeredMCMs;
        registeredMCMs.reserve(markers.size());
        for (const auto& marker : markers) {
            auto registeredMCM = ReadMCMFromMarker(marker.get(), vm, policy);
            if (registeredMCM) {
                registeredMCMs.push_back(std::move(*registeredMCM));
            }
        }
        logger::info("MCM registry read {} MCM scripts from {} marker references", registeredMCMs.size(), markers.size());
        return registeredMCMs;
    }

    void MCMRegistry::Reset()
    {
        MCMKickerSupport::GetSingleton()->Reset();
        if (IsMCMMenuRedoneAvailable()) {
            MCMMenuRedoneRegistry::GetSingleton()->Reset();
        }
        else if (IsMCMMenuMaidAvailable()) {
            MCMMenuMaidRegistry::GetSingleton()->Reset();
        }
    }

    void MCMRegistry::Refresh()
    {
        if (IsMCMMenuRedoneAvailable()) {
            MCMMenuRedoneRegistry::GetSingleton()->Refresh();
        }
        else if (IsMCMMenuMaidAvailable()) {
            MCMMenuMaidRegistry::GetSingleton()->Refresh();
        }
    }

    bool MCMRegistry::IsRefreshing()
    {
        if (IsMCMMenuRedoneAvailable()) {
            return MCMMenuRedoneRegistry::GetSingleton()->IsRefreshing();
        }
        return IsMCMMenuMaidAvailable() && MCMMenuMaidRegistry::GetSingleton()->IsRefreshing();
    }

    uint64_t MCMRegistry::CacheGeneration()
    {
        uint64_t generation = MCMKickerSupport::GetSingleton()->CacheGeneration();
        if (IsMCMMenuRedoneAvailable()) {
            generation += MCMMenuRedoneRegistry::GetSingleton()->CacheGeneration();
        }
        else if (IsMCMMenuMaidAvailable()) {
            generation += MCMMenuMaidRegistry::GetSingleton()->CacheGeneration();
        }
        return generation;
    }

    std::vector<MCMRegistryEntry> MCMRegistry::ReadRegisteredMCMs() const
    {
        if (IsMCMMenuRedoneAvailable()) {
            return MCMMenuRedoneRegistry::GetSingleton()->ReadRegisteredMCMs();
        }
        if (IsMCMMenuMaidAvailable()) {
            return MCMMenuMaidRegistry::GetSingleton()->ReadRegisteredMCMs();
        }
        if (IsMCMUnlockedAvailable()) {
            return ReadMCMUnlockedRegistry();
        }
        return ReadSkyUIRegistry();
    }

    // Menu Redone patch.
    void MCMMenuRedoneRegistry::CountResult::operator()(RE::BSScript::Variable a_result)
    {
        const int menuCount = a_result.IsInt() ? a_result.GetSInt() : -1;
        if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(CountTask{ request, menuCount }, 0.0F)) {
            logger::error("MCM Menu Redone registry count could not reach the game task queue");
            MCMMenuRedoneRegistry::GetSingleton()->FailRequest(request, "the count result could not reach the game task queue");
        }
    }

    void MCMMenuRedoneRegistry::MenuResult::operator()(RE::BSScript::Variable a_result)
    {
        auto menuQuest = a_result.IsObject() ? a_result.GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>();
        if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(MenuTask{ std::move(menuQuest), request, registryIndex }, 0.0F)) {
            logger::error("MCM Menu Redone registry entry {} could not reach the game task queue", registryIndex);
            MCMMenuRedoneRegistry::GetSingleton()->FailRequest(request, "a menu result could not reach the game task queue");
        }
    }

    void MCMMenuRedoneRegistry::Reset()
    {
        std::lock_guard lock(registryMutex);
        ++currentRequest.loadedGameSession;
        ++currentRequest.requestID;
        ++cacheGeneration;
        registeredMCMs.clear();
        pendingMCMs.clear();
        pendingMenuCount = 0;
        refreshing = false;
        cacheReady = false;
        logger::info("MCM Menu Redone registry cache reset");
    }

    void MCMMenuRedoneRegistry::Refresh()
    {
        MCMRegistryRequest request;
        {
            std::lock_guard lock(registryMutex);
            if (refreshing) {
                return;
            }

            refreshing = true;
            request = MCMRegistryRequest{ currentRequest.loadedGameSession, ++currentRequest.requestID };
        }

        if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(RefreshTask{ request }, 0.0F)) {
            FailRequest(request, "the game task queue is unavailable");
        }
    }

    std::vector<MCMRegistryEntry> MCMMenuRedoneRegistry::ReadRegisteredMCMs()
    {
        bool needsRefresh{};
        std::vector<MCMRegistryEntry> result;
        {
            std::lock_guard lock(registryMutex);
            result = registeredMCMs;
            needsRefresh = !cacheReady && !refreshing;
        }
        if (needsRefresh) {
            Refresh();
        }
        return result;
    }

    void MCMMenuRedoneRegistry::DispatchCount(MCMRegistryRequest a_request)
    {
        {
            std::lock_guard lock(registryMutex);
            if (!IsCurrentRequest(a_request)) {
                return;
            }
        }

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new CountResult(a_request));
        if (!Call("GetMenuCount", RE::MakeFunctionArguments(), std::move(result))) {
            FailRequest(a_request, "GetMenuCount could not be called");
        }
    }

    void MCMMenuRedoneRegistry::ReceiveCount(MCMRegistryRequest a_request, int a_menuCount)
    {
        if (a_menuCount < 0) {
            FailRequest(a_request, "GetMenuCount returned no number");
            return;
        }

        {
            std::lock_guard lock(registryMutex);
            if (!IsCurrentRequest(a_request)) {
                return;
            }

            pendingMCMs.clear();
            pendingMCMs.resize(static_cast<size_t>(a_menuCount));
            pendingMenuCount = static_cast<size_t>(a_menuCount);
            if (pendingMenuCount == 0) {
                registeredMCMs.clear();
                CompleteRequest(0);
                return;
            }
        }

        for (int index = 0; index < a_menuCount; ++index) {
            const size_t registryIndex = static_cast<size_t>(index);
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new MenuResult(a_request, registryIndex));
            if (!Call("GetMenu", RE::MakeFunctionArguments(int{ index }), std::move(result))) {
                ReceiveMenu(a_request, registryIndex, {});
            }
        }
    }

    void MCMMenuRedoneRegistry::ReceiveMenu(MCMRegistryRequest a_request, size_t a_registryIndex, RE::BSTSmartPointer<RE::BSScript::Object> a_menuQuest)
    {
        RE::BSTSmartPointer<RE::BSScript::Object> mcmScript;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> configType;
        if (vm && a_menuQuest && vm->GetScriptObjectType(RE::BSFixedString("SKI_ConfigBase"), configType)) {
            vm->CastObject(a_menuQuest, configType, mcmScript);
        }

        std::string failureReason;
        auto entry = MCMRegistry::CreateRegistryEntry(mcmScript, std::addressof(failureReason));

        std::lock_guard lock(registryMutex);
        if (!IsCurrentRequest(a_request) || a_registryIndex >= pendingMCMs.size()) {
            return;
        }

        if (entry) {
            logger::debug("MCM Menu Redone registry entry {} found '{}'", a_registryIndex, entry->identity.modID);
            pendingMCMs[a_registryIndex] = std::move(entry);
        }
        else {
            logger::debug("MCM Menu Redone registry skipped entry {}: {}", a_registryIndex, failureReason.empty() ? "no config script was returned" : failureReason);
        }

        if (pendingMenuCount > 0) {
            --pendingMenuCount;
        }
        if (pendingMenuCount != 0) {
            return;
        }

        registeredMCMs.clear();
        registeredMCMs.reserve(pendingMCMs.size());
        for (auto& pendingMCM : pendingMCMs) {
            if (pendingMCM) {
                registeredMCMs.push_back(std::move(*pendingMCM));
            }
        }
        const size_t nativeMenuCount = pendingMCMs.size();
        CompleteRequest(nativeMenuCount);
    }

    void MCMMenuRedoneRegistry::FailRequest(MCMRegistryRequest a_request, std::string_view a_reason)
    {
        std::lock_guard lock(registryMutex);
        if (!IsCurrentRequest(a_request)) {
            return;
        }

        pendingMCMs.clear();
        pendingMenuCount = 0;
        refreshing = false;
        logger::error("MCM Menu Redone registry refresh failed because {}", a_reason);
    }

    bool MCMMenuRedoneRegistry::Call(std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        return vm && vm->DispatchStaticCall(RE::BSFixedString(mcmMenuRedoneScriptName), RE::BSFixedString(a_functionName), a_arguments, a_result);
    }

    void MCMMenuRedoneRegistry::CompleteRequest(size_t a_nativeMenuCount)
    {
        pendingMCMs.clear();
        pendingMenuCount = 0;
        cacheReady = true;
        refreshing = false;
        ++cacheGeneration;
        logger::info("MCM Menu Redone registry read {} MCM scripts from {} entries", registeredMCMs.size(), a_nativeMenuCount);
    }

    // Menu Maid patch.
    void MCMMenuMaidRegistry::Result::operator()(RE::BSScript::Variable a_result)
    {
        if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(ResultTask{ std::move(a_result), request, type }, 0.0F)) {
            logger::error("Menu Maid 2 registry result could not reach the game task queue");
            MCMMenuMaidRegistry::GetSingleton()->FailRequest(request, "a result could not reach the game task queue");
        }
    }

    void MCMMenuMaidRegistry::Reset()
    {
        std::lock_guard lock(registryMutex);
        ++currentRequest.loadedGameSession;
        ++currentRequest.requestID;
        ++cacheGeneration;
        registeredMCMs.clear();
        reportedMenuCount = -1;
        refreshing = false;
        cacheReady = false;
        logger::info("Menu Maid 2 registry cache reset");
    }

    void MCMMenuMaidRegistry::Refresh()
    {
        MCMRegistryRequest request;
        {
            std::lock_guard lock(registryMutex);
            if (refreshing) {
                return;
            }

            refreshing = true;
            reportedMenuCount = -1;
            request = MCMRegistryRequest{ currentRequest.loadedGameSession, ++currentRequest.requestID };
        }

        if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(RefreshTask{ request }, 0.0F)) {
            UseFallbackRegistry(request, "the game task queue is unavailable");
        }
    }

    std::vector<MCMRegistryEntry> MCMMenuMaidRegistry::ReadRegisteredMCMs()
    {
        bool needsRefresh{};
        std::vector<MCMRegistryEntry> result;
        {
            std::lock_guard lock(registryMutex);
            result = registeredMCMs;
            needsRefresh = !cacheReady && !refreshing;
        }
        if (needsRefresh) {
            Refresh();
        }
        return result;
    }

    void MCMMenuMaidRegistry::Dispatch(MCMRegistryRequest a_request, ResultType a_type, std::string_view a_functionName)
    {
        {
            std::lock_guard lock(registryMutex);
            if (!IsCurrentRequest(a_request)) {
                return;
            }
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new Result(a_request, a_type));
        if (!vm || !vm->DispatchStaticCall(RE::BSFixedString(mcmMenuMaidScriptName), RE::BSFixedString(a_functionName), RE::MakeFunctionArguments(), result)) {
            UseFallbackRegistry(a_request, std::format("{} could not be called", a_functionName));
        }
    }

    void MCMMenuMaidRegistry::Receive(MCMRegistryRequest a_request, ResultType a_type, const RE::BSScript::Variable& a_result)
    {
        {
            std::lock_guard lock(registryMutex);
            if (!IsCurrentRequest(a_request)) {
                return;
            }
        }

        if (a_type == ResultType::Hired) {
            if (!a_result.IsBool() || !a_result.GetBool()) {
                UseFallbackRegistry(a_request, "Menu Maid 2 is dismissed");
                return;
            }
            Dispatch(a_request, ResultType::Count, "PluginsAmount");
            return;
        }

        if (a_type == ResultType::Count) {
            {
                std::lock_guard lock(registryMutex);
                if (!IsCurrentRequest(a_request)) {
                    return;
                }
                reportedMenuCount = a_result.IsInt() ? a_result.GetSInt() : -1;
            }
            Dispatch(a_request, ResultType::Menus, "GetConfigsForSkyUI");
            return;
        }

        ReceiveMenus(a_request, a_result);
    }

    void MCMMenuMaidRegistry::ReceiveMenus(MCMRegistryRequest a_request, const RE::BSScript::Variable& a_result)
    {
        auto forms = a_result.IsObjectArray() ? a_result.GetArray() : RE::BSTSmartPointer<RE::BSScript::Array>();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> configType;
        if (!forms || !vm || !vm->GetScriptObjectType(RE::BSFixedString("SKI_ConfigBase"), configType)) {
            UseFallbackRegistry(a_request, "GetConfigsForSkyUI returned no usable Form array");
            return;
        }

        std::vector<MCMRegistryEntry> entries;
        std::vector<uint64_t> handles;
        entries.reserve(forms->size());
        handles.reserve(forms->size());
        size_t occupiedSlots{};
        size_t unresolved{};
        size_t duplicates{};
        size_t registryIndex{};
        for (const auto& value : *forms) {
            auto menuQuest = value.IsObject() ? value.GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>();
            if (!menuQuest) {
                ++registryIndex;
                continue;
            }
            ++occupiedSlots;

            RE::BSTSmartPointer<RE::BSScript::Object> mcmScript;
            vm->CastObject(menuQuest, configType, mcmScript);
            std::string failureReason;
            auto entry = MCMRegistry::CreateRegistryEntry(mcmScript, std::addressof(failureReason));
            if (!entry) {
                ++unresolved;
                logger::debug("Menu Maid 2 registry skipped entry {}: {}", registryIndex, failureReason.empty() ? "no config script was returned" : failureReason);
                ++registryIndex;
                continue;
            }

            const uint64_t handle = mcmScript->GetHandle();
            if (std::find(handles.begin(), handles.end(), handle) != handles.end()) {
                ++duplicates;
                logger::debug("Menu Maid 2 registry skipped duplicate entry {} for '{}'", registryIndex, entry->identity.modID);
                ++registryIndex;
                continue;
            }

            handles.push_back(handle);
            entries.push_back(std::move(*entry));
            ++registryIndex;
        }

        CompleteRequest(a_request, std::move(entries), forms->size(), occupiedSlots, unresolved, duplicates);
    }

    void MCMMenuMaidRegistry::CompleteRequest(MCMRegistryRequest a_request, std::vector<MCMRegistryEntry> a_registeredMCMs, size_t a_arraySlots, size_t a_occupiedSlots, size_t a_unresolved, size_t a_duplicates)
    {
        int menuCount{};
        size_t usableCount{};
        {
            std::lock_guard lock(registryMutex);
            if (!IsCurrentRequest(a_request)) {
                return;
            }

            registeredMCMs = std::move(a_registeredMCMs);
            usableCount = registeredMCMs.size();
            menuCount = reportedMenuCount;
            refreshing = false;
            cacheReady = true;
            ++cacheGeneration;
        }

        logger::info("Menu Maid 2 registry read {} MCM scripts from {} occupied slots in its {}-slot SkyUI-compatible array ({} unresolved, {} duplicates, {} total reported)",
            usableCount, a_occupiedSlots, a_arraySlots, a_unresolved, a_duplicates, menuCount);
        if (menuCount > 0 && usableCount < static_cast<size_t>(menuCount)) {
            logger::warn("Menu Maid 2 reports {} MCMs, but its current public form API exposes only {}; menus outside that list cannot be backed up or restored yet", menuCount, usableCount);
        }
    }

    void MCMMenuMaidRegistry::UseFallbackRegistry(MCMRegistryRequest a_request, std::string_view a_reason)
    {
        {
            std::lock_guard lock(registryMutex);
            if (!IsCurrentRequest(a_request)) {
                return;
            }
        }

        std::vector<MCMRegistryEntry> fallbackMCMs;
        std::string_view fallbackName{ "SkyUI" };
        if (MCMRegistry::IsMCMUnlockedAvailable()) {
            fallbackMCMs = MCMRegistry::ReadMCMUnlockedRegistry();
            fallbackName = "MCM Unlocked";
        }
        else {
            fallbackMCMs = MCMRegistry::ReadSkyUIRegistry();
        }

        size_t fallbackCount{};
        {
            std::lock_guard lock(registryMutex);
            if (!IsCurrentRequest(a_request)) {
                return;
            }

            registeredMCMs = std::move(fallbackMCMs);
            fallbackCount = registeredMCMs.size();
            refreshing = false;
            cacheReady = true;
            ++cacheGeneration;
        }
        logger::info("Menu Maid 2 registry was not used because {}; {} supplied {} MCMs instead", a_reason, fallbackName, fallbackCount);
    }

    void MCMMenuMaidRegistry::FailRequest(MCMRegistryRequest a_request, std::string_view a_reason)
    {
        std::lock_guard lock(registryMutex);
        if (!IsCurrentRequest(a_request)) {
            return;
        }

        refreshing = false;
        logger::error("Menu Maid 2 registry refresh failed because {}", a_reason);
    }
}
