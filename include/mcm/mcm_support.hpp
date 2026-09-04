#pragma once

#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_script.hpp"
#include "mcm/mcm_support_defs.hpp"
#include "utils/scheduler.hpp"

#define DECLARE_IGNORED_MCM_TEXT(text) std::string_view{ text },
#define DECLARE_IGNORED_MCM_CONTROL(scriptName, pageIndex, type, optionLabel) MCMIgnoredControl{ scriptName, optionLabel, pageIndex, type },
#define DECLARE_MCM_ACTIVATION_STATUS(enabledText, disabledText) MCMActivationStatus{ enabledText, disabledText },
#define DECLARE_MCM_ACTIVATION_COMMAND(enableText, disableText) MCMActivationCommand{ enableText, disableText },
#define DECLARE_SKYUI_SCRIPT_CYCLE_SETTING(scriptName, pageIndex, optionVariable, valueVariable, optionLabel, valueTextArray, valueCount, readableWhenDisabled) MakeScriptCycleSetting(scriptName, pageIndex, optionVariable, valueVariable, optionLabel, valueTextArray, valueCount, readableWhenDisabled),
#define DECLARE_SKYUI_GLOBAL_CYCLE_SETTING(scriptName, pageIndex, optionVariable, valueVariable, optionLabel, profileLabel, valueCount, firstValue, valueStep) MakeGlobalCycleSetting(scriptName, pageIndex, optionVariable, valueVariable, optionLabel, profileLabel, valueCount, MakeSequentialCycleValues(valueCount, firstValue, valueStep)),
#define DECLARE_SKYUI_CUSTOM_GLOBAL_CYCLE_SETTING(scriptName, pageIndex, optionVariable, valueVariable, optionLabel, profileLabel, valueCount, value0, value1, value2, value3, value4, value5) MakeGlobalCycleSetting(scriptName, pageIndex, optionVariable, valueVariable, optionLabel, profileLabel, valueCount, std::array<int, 6>{ value0, value1, value2, value3, value4, value5 }),

namespace MCMMemory
{
    inline constexpr RE::FormID markerBaseLocalFormID{ 0x800 };
    inline constexpr RE::FormID markerCellLocalFormID{ 0x801 };
    inline constexpr std::string_view mcmUnlockedPluginName{ "MCM Unlocked.esp" };
    inline constexpr std::string_view markerScriptName{ "MCMUnlockedMarkerScript" };
    inline constexpr std::string_view mcmMenuRedoneScriptName{ "MCMR_Native" };
    inline constexpr std::string_view mcmMenuMaidScriptName{ "MenuMaid2" };
    inline constexpr std::string_view mcmHelperBaseScriptName{ "MCM_ConfigBase" };
    inline constexpr std::string_view nlMCMBaseScriptName{ "nl_mcm" };
    inline constexpr std::string_view skyUIConfigScriptName{ "SKI_ConfigMenu" };

    enum class MCMCycleValueSource
    {
        ScriptInteger,
        GlobalVariable
    };

    struct MCMIgnoredControl
    {
        std::string_view scriptName;

        std::string_view optionLabel;

        int pageIndex{};

        ControlType type{ ControlType::Unknown };
    };

    struct MCMActivationStatus
    {
        std::string_view enabledText;

        std::string_view disabledText;
    };

    inline constexpr std::array mcmActivationStatuses
    {
        FOREACH_MCM_ACTIVATION_STATUS(DECLARE_MCM_ACTIVATION_STATUS)
    };

    struct MCMActivationCommand
    {
        std::string_view enableText;

        std::string_view disableText;
    };

    inline constexpr std::array mcmActivationCommands
    {
        FOREACH_MCM_ACTIVATION_COMMAND(DECLARE_MCM_ACTIVATION_COMMAND)
    };

    struct MCMActivationState
    {
        MCMActivation activation;

        bool enabled{};
    };

    // Identifies a cycling text setting without relying on its translated label alone.
    struct MCMCycleSetting
    {
        std::string_view scriptName;

        std::string_view optionVariable;

        std::string_view valueVariable;

        std::string_view optionLabel;

        std::string_view profileLabel;

        std::string_view valueTextArray;

        std::array<int, 6> values{};

        int pageIndex{};

        int valueCount{};

        MCMCycleValueSource valueSource{ MCMCycleValueSource::ScriptInteger };

        bool readableWhenDisabled{};
    };

    struct VioLensSettingOrder
    {
        bool operator()(const CapturedSetting& a_left, const CapturedSetting& a_right) const;
    };

    // Sorts MCM Unlocked markers by FormID so registry reads have a stable order.
    struct MCMMarkerFormIDLess
    {
        bool operator()(const RE::NiPointer<RE::TESObjectREFR>& a_left, const RE::NiPointer<RE::TESObjectREFR>& a_right) const noexcept
        {
            return a_left->GetFormID() < a_right->GetFormID();
        }
    };

    // MCM Helper defines stable setting IDs in JSON and stores their current values in INI files
    struct MCMHelperSetting
    {
        std::string id;

        std::string label;

        std::string pageName;

        int pageIndex{-1};

        int optionIndex{-1};

        ControlType type{ ControlType::Unknown };
    };

    struct MCMHelperConfig
    {
        std::filesystem::path directory;

        std::string modName;

        std::vector<MCMHelperSetting> settings;
    };

    // Identifies one asynchronous registry read in one loaded game session.
    struct MCMRegistryRequest
    {
        uint64_t loadedGameSession{};

        uint64_t requestID{};
    };

    inline constexpr MCMCycleSetting MakeScriptCycleSetting(std::string_view a_scriptName, int a_pageIndex, std::string_view a_optionVariable, std::string_view a_valueVariable, std::string_view a_optionLabel, std::string_view a_valueTextArray, int a_valueCount, bool a_readableWhenDisabled)
    {
        MCMCycleSetting setting;
        setting.scriptName = a_scriptName;
        setting.optionVariable = a_optionVariable;
        setting.valueVariable = a_valueVariable;
        setting.optionLabel = a_optionLabel;
        setting.profileLabel = a_optionLabel;
        setting.valueTextArray = a_valueTextArray;
        setting.pageIndex = a_pageIndex;
        setting.valueCount = a_valueCount;
        setting.readableWhenDisabled = a_readableWhenDisabled;
        for (int index = 0; index < a_valueCount; ++index) {
            setting.values[static_cast<size_t>(index)] = index;
        }
        return setting;
    }

    inline constexpr std::array<int, 6> MakeSequentialCycleValues(int a_valueCount, int a_firstValue, int a_valueStep)
    {
        std::array<int, 6> values{};
        for (int index = 0; index < a_valueCount && index < static_cast<int>(values.size()); ++index) {
            values[static_cast<size_t>(index)] = a_firstValue + index * a_valueStep;
        }
        return values;
    }

    inline constexpr MCMCycleSetting MakeGlobalCycleSetting(std::string_view a_scriptName, int a_pageIndex, std::string_view a_optionVariable, std::string_view a_valueVariable, std::string_view a_optionLabel, std::string_view a_profileLabel, int a_valueCount, std::array<int, 6> a_values)
    {
        MCMCycleSetting setting;
        setting.scriptName = a_scriptName;
        setting.optionVariable = a_optionVariable;
        setting.valueVariable = a_valueVariable;
        setting.optionLabel = a_optionLabel;
        setting.profileLabel = a_profileLabel;
        setting.values = a_values;
        setting.pageIndex = a_pageIndex;
        setting.valueCount = a_valueCount;
        setting.valueSource = MCMCycleValueSource::GlobalVariable;
        return setting;
    }

    inline bool HasMCMScript(std::string_view a_modID, std::string_view a_scriptName)
    {
        const auto scriptName = a_modID.substr(0, a_modID.find("::"));
        return EqualsCaseInsensitive(scriptName, a_scriptName);
    }

    inline std::string_view GetMCMExclusionReason(std::string_view a_modID)
    {
        // Match the script, not a name the player can rename or translate.
        constexpr std::string_view checklistScript{ "dbm_dynamicmcmscript" };
        if (HasMCMScript(a_modID, checklistScript)) {
            return "LOTD Checklist is ignored because it displays dynamic checklist status rather than configurable settings";
        }
        return {};
    }

    // Ignore profile-management pages and text commands that do not store persistent settings.
    inline constexpr std::array ignoredMCMPageTerms
    {
        FOREACH_IGNORED_MCM_PAGE(DECLARE_IGNORED_MCM_TEXT)
    };

    inline constexpr std::array ignoredMCMCommandTerms
    {
        FOREACH_IGNORED_MCM_COMMAND(DECLARE_IGNORED_MCM_TEXT)
    };

    inline constexpr std::array ignoredMCMControls
    {
        FOREACH_IGNORED_MCM_CONTROL(DECLARE_IGNORED_MCM_CONTROL)
    };

    // This relates to activation labels.
    inline constexpr std::array mcmActivationLabelTerms
    {
        FOREACH_MCM_ACTIVATION_LABEL(DECLARE_IGNORED_MCM_TEXT)
    };

    inline constexpr std::array mcmCycleSettings
    {
        FOREACH_SKYUI_SCRIPT_CYCLE_SETTING(DECLARE_SKYUI_SCRIPT_CYCLE_SETTING)
        FOREACH_SKYUI_GLOBAL_CYCLE_SETTING(DECLARE_SKYUI_GLOBAL_CYCLE_SETTING, DECLARE_SKYUI_CUSTOM_GLOBAL_CYCLE_SETTING)
    };

    // Responsible for filtering out profilie-management pages and text commands.
    struct MCMCommandSupport
    {
        MCMCommandSupport() = delete;

        static bool IsIgnoredPage(std::string_view a_pageName);

        static bool IsIgnored(std::string_view a_modID, std::string_view a_pageName, int a_pageIndex, ControlType a_type, std::string_view a_stateName, std::string_view a_optionLabel);
    };

    // Finds a status, toggle or activation command that enables an MCM before its settings appear.
    struct MCMActivationSupport
    {
        MCMActivationSupport() = delete;

        // Looks through the current page for a control that represents whether the MCM is active.
        // We defined keywords for this.
        static std::optional<MCMActivationState> ReadState(const MCMScript& a_script, const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex);

        // Checks whether the selected control is the MCM activation control and reads its state.
        static std::optional<MCMActivationState> ReadSelectedState(const MCMScript& a_script, const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, int a_optionIndex, const MCMControl& a_control);

        // Reads the current state of an activation control found during backup.
        static std::optional<bool> IsEnabled(const MCMScript& a_script, const MCMActivation& a_activation);

        // Finds the current option index for a saved activation control.
        static std::optional<int> FindOption(const MCMScript& a_script, const MCMActivation& a_activation);

        // Checks whether a live control matches the saved activation control.
        static bool MatchesControl(const MCMControl& a_control, const MCMActivation& a_activation);

    private:

        // Keeps SkyUI HUD toggle from being mistaken for an MCM activation control.
        static inline bool CanBeStaged(const MCMScript& a_script)
        {
            return !a_script.IsBasedOn(skyUIConfigScriptName);
        }

        // Checks the control for a name that suggests it enables or disables the MCM.
        static bool HasActivationName(const MCMControl& a_control);

        // Checks the control for a known enabled or disabled status.
        static bool HasActivationStatusName(const MCMControl& a_control);

        // Reads the state represented by a Start/Stop, Activate/Deactivate or Enable/Disable command.
        static std::optional<bool> ReadCommandState(const MCMControl& a_control, const MCMIdentity& a_identity);

        // Builds an activation record from the control and its location in the MCM.
        static MCMActivation MakeActivation(const MCMIdentity& a_identity, std::string_view a_pageName, int a_pageIndex, int a_optionIndex, const MCMControl& a_control);

        // Replaces a disabled status word with its enabled counterpart.
        static std::string MakeEnabledText(std::string_view a_text, const MCMActivationStatus& a_status);
    };

    // Kicker resets SkyUI after a delay; a quiet registry before that reset is not reliable.
    class MCMKickerSupport : public RE::BSTEventSink<SKSE::ModCallbackEvent>
    {
    public:

        enum class Status { Inactive, Waiting, Ready, Failed };

        static MCMKickerSupport* GetSingleton()
        {
            static MCMKickerSupport singleton;
            return std::addressof(singleton);
        }

        void Install();

        void Reset();

        Status GetStatus()
        {
            std::lock_guard lock(kickerMutex);
            return status;
        }

        uint64_t CacheGeneration() const { return cacheGeneration.load(); }

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;

    private:

        struct CheckTask
        {
            uint64_t loadedGameSession{};

            void operator()() const { GetSingleton()->Check(loadedGameSession); }
        };

        bool IsKickDue() const;

        void Check(uint64_t a_loadedGameSession);

        void QueueCheck();

        std::mutex kickerMutex;

        RegistryWait registryWait;

        std::atomic<uint64_t> cacheGeneration{};

        uint64_t loadedGameSession{};

        RE::TESQuest* kickerQuest{};

        RE::TESQuest* managerQuest{};

        Status status{ Status::Inactive };

        bool installed{};

        bool resetDetected{};
    };


    // NL_MCM still uses SkyUI controls, but each page owns its setting states.
    struct NLMCMSupport
    {
        NLMCMSupport() = delete;

        static bool IsSupported(const MCMScript& a_script)
        {
            return a_script.IsBasedOn(nlMCMBaseScriptName);
        }
    };

    // Supports known SkyUI text controls that cycle through values when clicked.
    struct SkyUICycleSupport
    {
        SkyUICycleSupport() = delete;

        static const MCMCycleSetting* Find(std::string_view a_modID, std::string_view a_settingID);

        static const MCMCycleSetting* Find(const MCMScript& a_script, int a_optionIndex);

        static std::optional<int> FindOption(const MCMScript& a_script, const MCMCycleSetting& a_setting, bool a_requireEnabled = false);

        static std::optional<int> ReadValue(const MCMScript& a_script, const MCMCycleSetting& a_setting);

        static bool ReadSetting(const MCMScript& a_script, CapturedSetting& a_setting);
    };

    // Keeps VioLens command filtering and restore order separate from shared cycle handling.
    struct VioLensSupport
    {
        VioLensSupport() = delete;

        static constexpr std::string_view scriptName{ "VL_ConfigMenu" };

        static bool IsSupported(const MCMScript& a_script) { return a_script.IsBasedOn(scriptName); }

        static bool IsSupported(std::string_view a_modID)
        {
            return HasMCMScript(a_modID, scriptName);
        }

        static bool IsCommand(std::string_view a_modID, std::string_view a_stateName, int a_pageIndex, std::string_view a_optionLabel);

        static void OrderSettings(std::vector<CapturedSetting>& a_settings);

        static int RestoreOrder(const CapturedSetting& a_setting)
        {
            if (a_setting.settingID == "KillmoveOID") {
                return 0;
            }
            if (a_setting.type == ControlType::Menu && a_setting.stateName.empty() && a_setting.optionLabel == "$Camera View") {
                return 1;
            }
            if (a_setting.stateName == "CameraSettingMenu") {
                return 2;
            }
            if (a_setting.settingID == "RangedModeOID") {
                return 3;
            }
            // Ranged Killmoves set to Off disables the camera control.
            return a_setting.settingID == "RangedPerspectiveOID" ? 4 : 5;
        }
    };

    // Reads stable setting IDs and values from MCM Helper JSON and INI files.
    // This is used to restore keymap settings from MCM Helper mods that don't provide a live MCM script.
    class MCMHelperSupport
    {
    public:

        static MCMHelperSupport* GetSingleton()
        {
            static MCMHelperSupport singleton;
            return std::addressof(singleton);
        }

        bool ReadKeymapSetting(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, CapturedSetting& a_setting);

    private:

        bool IsMCMHelperScript(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript) const
        {
            return MCMScript(a_mcmScript).IsBasedOn(mcmHelperBaseScriptName);
        }

        std::optional<std::string> ReadConfigModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript) const;

        MCMHelperConfig* GetConfig(std::string_view a_modName);

        std::optional<MCMHelperConfig> LoadConfig(std::string_view a_modName) const;

        void ReadPageSettings(const nlohmann::json& a_page, std::string_view a_pageName, int a_pageIndex, std::string_view a_defaultFillMode, std::vector<MCMHelperSetting>& a_settings) const;

        const MCMHelperSetting* FindSetting(const MCMHelperConfig& a_config, const CapturedSetting& a_setting) const;

        std::optional<int> ReadInteger(const MCMHelperConfig& a_config, std::string_view a_settingID) const;

        std::unordered_map<std::string, MCMHelperConfig> configs;
    };

    // Reads the registry through MCM Menu Redone Papyrus functions.
    class MCMMenuRedoneRegistry
    {
    public:

        static MCMMenuRedoneRegistry* GetSingleton()
        {
            static MCMMenuRedoneRegistry singleton;
            return std::addressof(singleton);
        }

        bool IsRefreshing()
        {
            std::lock_guard lock(registryMutex);
            return refreshing;
        }

        uint64_t CacheGeneration()
        {
            std::lock_guard lock(registryMutex);
            return cacheGeneration;
        }

        void Reset();

        void Refresh();

        std::vector<MCMRegistryEntry> ReadRegisteredMCMs();

    private:

        struct CountResult : public RE::BSScript::IStackCallbackFunctor
        {
            MCMRegistryRequest request;

            explicit CountResult(MCMRegistryRequest a_request) : request(a_request) {}

            void operator()(RE::BSScript::Variable a_result) override;

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
        };

        struct MenuResult : public RE::BSScript::IStackCallbackFunctor
        {
            MCMRegistryRequest request;

            size_t registryIndex{};

            MenuResult(MCMRegistryRequest a_request, size_t a_registryIndex) : request(a_request), registryIndex(a_registryIndex) {}

            void operator()(RE::BSScript::Variable a_result) override;

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
        };

        struct RefreshTask
        {
            MCMRegistryRequest request;

            void operator()() const
            {
                MCMMenuRedoneRegistry::GetSingleton()->DispatchCount(request);
            }
        };

        struct CountTask
        {
            MCMRegistryRequest request;

            int menuCount{-1};

            void operator()() const
            {
                MCMMenuRedoneRegistry::GetSingleton()->ReceiveCount(request, menuCount);
            }
        };

        struct MenuTask
        {
            RE::BSTSmartPointer<RE::BSScript::Object> menuQuest;

            MCMRegistryRequest request;

            size_t registryIndex{};

            void operator()() const
            {
                MCMMenuRedoneRegistry::GetSingleton()->ReceiveMenu(request, registryIndex, menuQuest);
            }
        };

        inline bool IsCurrentRequest(MCMRegistryRequest a_request) const
        {
            return refreshing && a_request.loadedGameSession == currentRequest.loadedGameSession && a_request.requestID == currentRequest.requestID;
        }

        void DispatchCount(MCMRegistryRequest a_request);

        void ReceiveCount(MCMRegistryRequest a_request, int a_menuCount);

        void ReceiveMenu(MCMRegistryRequest a_request, size_t a_registryIndex, RE::BSTSmartPointer<RE::BSScript::Object> a_menuQuest);

        void FailRequest(MCMRegistryRequest a_request, std::string_view a_reason);

        bool Call(std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result);

        void CompleteRequest(size_t a_nativeMenuCount);

        std::mutex registryMutex;

        std::vector<MCMRegistryEntry> registeredMCMs;

        std::vector<std::optional<MCMRegistryEntry>> pendingMCMs;

        MCMRegistryRequest currentRequest;

        uint64_t cacheGeneration{};

        size_t pendingMenuCount{};

        bool refreshing{};

        bool cacheReady{};
    };

    // Reads the SkyUI 128-limit of Menu Maid 2 registry without opening an MCM.
    class MCMMenuMaidRegistry
    {
    public:

        static MCMMenuMaidRegistry* GetSingleton()
        {
            static MCMMenuMaidRegistry singleton;
            return std::addressof(singleton);
        }

        bool IsRefreshing()
        {
            std::lock_guard lock(registryMutex);
            return refreshing;
        }

        uint64_t CacheGeneration()
        {
            std::lock_guard lock(registryMutex);
            return cacheGeneration;
        }

        void Reset();

        void Refresh();

        std::vector<MCMRegistryEntry> ReadRegisteredMCMs();

    private:

        enum class ResultType
        {
            Hired,
            Count,
            Menus
        };

        struct Result : public RE::BSScript::IStackCallbackFunctor
        {
            MCMRegistryRequest request;

            ResultType type{};

            Result(MCMRegistryRequest a_request, ResultType a_type) : request(a_request), type(a_type) {}

            void operator()(RE::BSScript::Variable a_result) override;

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
        };

        struct RefreshTask
        {
            MCMRegistryRequest request;

            void operator()() const
            {
                MCMMenuMaidRegistry::GetSingleton()->Dispatch(request, ResultType::Hired, "Hired");
            }
        };

        struct ResultTask
        {
            RE::BSScript::Variable result;

            MCMRegistryRequest request;

            ResultType type{};

            void operator()() const
            {
                MCMMenuMaidRegistry::GetSingleton()->Receive(request, type, result);
            }
        };

        inline bool IsCurrentRequest(MCMRegistryRequest a_request) const
        {
            return refreshing && a_request.loadedGameSession == currentRequest.loadedGameSession && a_request.requestID == currentRequest.requestID;
        }

        void Dispatch(MCMRegistryRequest a_request, ResultType a_type, std::string_view a_functionName);

        void Receive(MCMRegistryRequest a_request, ResultType a_type, const RE::BSScript::Variable& a_result);

        void ReceiveMenus(MCMRegistryRequest a_request, const RE::BSScript::Variable& a_result);

        void CompleteRequest(MCMRegistryRequest a_request, std::vector<MCMRegistryEntry> a_registeredMCMs, size_t a_arraySlots, size_t a_occupiedSlots, size_t a_unresolved, size_t a_duplicates);

        void UseFallbackRegistry(MCMRegistryRequest a_request, std::string_view a_reason);

        void FailRequest(MCMRegistryRequest a_request, std::string_view a_reason);

        std::mutex registryMutex;

        std::vector<MCMRegistryEntry> registeredMCMs;

        MCMRegistryRequest currentRequest;

        uint64_t cacheGeneration{};

        int reportedMenuCount{-1};

        bool refreshing{};

        bool cacheReady{};
    };

}

#undef DECLARE_IGNORED_MCM_TEXT
#undef DECLARE_IGNORED_MCM_CONTROL
#undef DECLARE_MCM_ACTIVATION_STATUS
#undef DECLARE_MCM_ACTIVATION_COMMAND
#undef DECLARE_SKYUI_SCRIPT_CYCLE_SETTING
#undef DECLARE_SKYUI_GLOBAL_CYCLE_SETTING
#undef DECLARE_SKYUI_CUSTOM_GLOBAL_CYCLE_SETTING
