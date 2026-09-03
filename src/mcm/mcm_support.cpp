#include "mcm/mcm_support.hpp"

#include <SimpleIni.h>
#include <charconv>
#include <iterator>

namespace MCMMemory
{
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

    const MCMCycleSetting* VioLensSupport::FindCycle(std::string_view a_settingID)
    {
        for (const auto& setting : cyclingSettings) {
            if (setting.optionVariable == a_settingID) {
                return &setting;
            }
        }
        return nullptr;
    }

    std::optional<int> VioLensSupport::FindCycleIndex(const MCMScript& a_script, const MCMCycleSetting& a_setting, bool a_requireEnabled)
    {
        auto page = a_script.ReadCurrentPage();
        if (!IsSupported(a_script) || !page || page->index != 0) {
            return std::nullopt;
        }
        auto option = a_script.ReadInteger(a_setting.optionVariable);
        // SkyUI IDs include the page number in their high byte; the buffer index does not.
        if (!option || *option < 256 || *option >= 384) {
            return std::nullopt;
        }
        const int index = *option - 256;
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
        if (!a_requireEnabled && static_cast<int>(*flags) == 258 && a_setting.optionVariable == "RangedPerspectiveOID") {
            auto value = ReadCycleValue(a_script, a_setting);
            auto text = a_script.ReadString("_strValueBuf", static_cast<size_t>(index));
            auto expected = value ? a_script.ReadString("RangedPerspectiveList", static_cast<size_t>(*value)) : std::nullopt;
            if (text && expected && *text == *expected) {
                return index;
            }
        }
        return std::nullopt;
    }

    const MCMCycleSetting* VioLensSupport::FindCycle(const MCMScript& a_script, int a_optionIndex)
    {
        if (a_optionIndex < 0 || !IsSupported(a_script)) {
            return nullptr;
        }
        for (const auto& setting : cyclingSettings) {
            if (FindCycleIndex(a_script, setting) == a_optionIndex) {
                return &setting;
            }
        }
        return nullptr;
    }

    std::optional<int> VioLensSupport::ReadCycleValue(const MCMScript& a_script, const MCMCycleSetting& a_setting)
    {
        if (!IsSupported(a_script)) {
            return std::nullopt;
        }
        auto value = a_script.ReadInteger(a_setting.valueVariable);
        return value && *value >= 0 && *value < a_setting.valueCount ? value : std::nullopt;
    }

    bool VioLensSupport::ReadCycleSetting(const MCMScript& a_script, CapturedSetting& a_setting)
    {
        const auto* cycle = FindCycle(a_script, a_setting.selection.optionIndex);
        if (!cycle) {
            return false;
        }
        auto value = ReadCycleValue(a_script, *cycle);
        if (!value) {
            return false;
        }
        a_setting.type = ControlType::Cycle;
        a_setting.settingID = cycle->optionVariable;
        a_setting.optionLabel = cycle->optionLabel;
        a_setting.value = *value;
        a_setting.valueSource = std::format("script.{}", cycle->valueVariable);
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
        logger::info("Jaxonz MCM Kicker detected; scripted operations will wait for its reset and registration");
    }

    void MCMKickerSupport::Reset()
    {
        std::lock_guard lock(kickerMutex);
        ++loadedGameSession;
        ++cacheGeneration;
        registryWait.Reset();
        resetObserved = false;
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
        resetObserved = true;
        registryWait.modIDs.clear();
        registryWait.quietCheckCount = 0;
        ++cacheGeneration;
        // Redone cached scripts may still describe the registry from before the reset.
        if (MCMRegistry::IsMCMMenuRedoneAvailable()) {
            MCMMenuRedoneRegistry::GetSingleton()->Reset();
        }
        logger::info("MCM Kicker reset observed; waiting for the rebuilt registry to settle");
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
        if (resetObserved) {
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
            logger::error("MCM Kicker registration did not finish after {} checks (reset observed: {}); scripted operations will not use an unconfirmed registry", registryWait.checkCount, resetObserved);
            return;
        }
        if (resetObserved && result == RegistryWaitResult::Changed) {
            logger::info("MCM Kicker rebuilt registry contains {} MCMs; checking stability", mcms.size());
        }
        if (resetObserved) {
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
            logger::info("MCM Menu Redone detected; its registry will be used");
        }
        else if (IsMCMUnlockedAvailable()) {
            logger::info("MCM Unlocked detected; its marker registry will be used");
        }
        else {
            logger::info("SkyUI registry will be used");
        }
    }

    bool MCMRegistry::IsMCMMenuRedoneAvailable()
    {
        return GetModuleHandleW(L"MCMMenuRedone.dll") != nullptr;
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
    }

    void MCMRegistry::Refresh()
    {
        if (IsMCMMenuRedoneAvailable()) {
            MCMMenuRedoneRegistry::GetSingleton()->Refresh();
        }
    }

    bool MCMRegistry::IsRefreshing()
    {
        return IsMCMMenuRedoneAvailable() && MCMMenuRedoneRegistry::GetSingleton()->IsRefreshing();
    }

    uint64_t MCMRegistry::CacheGeneration()
    {
        return MCMKickerSupport::GetSingleton()->CacheGeneration() + (IsMCMMenuRedoneAvailable() ? MCMMenuRedoneRegistry::GetSingleton()->CacheGeneration() : 0);
    }

    std::vector<MCMRegistryEntry> MCMRegistry::ReadRegisteredMCMs() const
    {
        if (IsMCMMenuRedoneAvailable()) {
            return MCMMenuRedoneRegistry::GetSingleton()->ReadRegisteredMCMs();
        }
        if (IsMCMUnlockedAvailable()) {
            return ReadMCMUnlockedRegistry();
        }
        return ReadSkyUIRegistry();
    }

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
        RegistryRequest request;
        {
            std::lock_guard lock(registryMutex);
            if (refreshing) {
                return;
            }

            refreshing = true;
            request = RegistryRequest{ currentRequest.loadedGameSession, ++currentRequest.requestID };
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

    void MCMMenuRedoneRegistry::DispatchCount(RegistryRequest a_request)
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

    void MCMMenuRedoneRegistry::ReceiveCount(RegistryRequest a_request, int a_menuCount)
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

    void MCMMenuRedoneRegistry::ReceiveMenu(RegistryRequest a_request, size_t a_registryIndex, RE::BSTSmartPointer<RE::BSScript::Object> a_menuQuest)
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

    void MCMMenuRedoneRegistry::FailRequest(RegistryRequest a_request, std::string_view a_reason)
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
}
