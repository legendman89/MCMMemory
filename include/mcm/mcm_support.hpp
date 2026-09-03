#pragma once

#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_script.hpp"
#include "utils/scheduler.hpp"

namespace MCMMemory
{
    inline constexpr RE::FormID markerBaseLocalFormID{ 0x800 };
    inline constexpr RE::FormID markerCellLocalFormID{ 0x801 };
    inline constexpr std::string_view mcmUnlockedPluginName{ "MCM Unlocked.esp" };
    inline constexpr std::string_view markerScriptName{ "MCMUnlockedMarkerScript" };
    inline constexpr std::string_view mcmMenuRedoneScriptName{ "MCMR_Native" };
    inline constexpr std::string_view mcmHelperBaseScriptName{ "MCM_ConfigBase" };
    inline constexpr std::string_view nlMCMBaseScriptName{ "nl_mcm" };

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

        bool resetObserved{};
    };

    inline std::string_view GetMCMExclusionReason(std::string_view a_modID)
    {
        // Match the script, not a name the player can rename or translate.
        constexpr std::string_view checklistScript{ "dbm_dynamicmcmscript" };
        const auto scriptName = a_modID.substr(0, a_modID.find("::"));
        if (scriptName.size() == checklistScript.size() && ContainsCaseInsensitive(scriptName, checklistScript)) {
            return "LOTD Checklist is ignored because it displays dynamic checklist status rather than configurable settings";
        }
        return {};
    }

    // NL_MCM still uses SkyUI controls, but each page owns its setting states.
    struct NLMCMSupport
    {
        NLMCMSupport() = delete;

        static bool IsSupported(const MCMScript& a_script)
        {
            return a_script.IsBasedOn(nlMCMBaseScriptName);
        }
    };

    // Identifies a cycling text setting without relying on its translated label alone.
    struct MCMCycleSetting
    {
        std::string_view optionVariable;

        std::string_view valueVariable;

        std::string_view optionLabel;

        int valueCount{};
    };

    // VioLens uses text buttons for several settings. Only these known buttons may be replayed.
    struct VioLensSupport
    {
        VioLensSupport() = delete;

        static constexpr std::string_view scriptName{ "VL_ConfigMenu" };

        static constexpr std::array<MCMCycleSetting, 6> cyclingSettings{{
            { "KillmoveOID", "KillmoveListIndex", "$Killmoves", 4 },
            { "RangedModeOID", "RangedModeListIndex", "$Selection Mode", 2 },
            { "MoveAnimationsOID", "MoveAnimationsListIndex", "$Advancing Killmoves", 3 },
            { "DecapitationOID", "DecapitationListIndex", "$Decapitations", 3 },
            { "RangedKillmoveOID", "RangedKillmoveListIndex", "$Killmoves", 3 },
            { "RangedPerspectiveOID", "RangedPerspectiveListIndex", "$Camera View", 2 }
        }};

        static bool IsSupported(const MCMScript& a_script) { return a_script.IsBasedOn(scriptName); }

        static bool IsSupported(std::string_view a_modID)
        {
            const auto name = a_modID.substr(0, a_modID.find("::"));
            return name.size() == scriptName.size() && ContainsCaseInsensitive(name, scriptName);
        }

        static bool IsCommand(std::string_view a_modID, std::string_view a_stateName, int a_pageIndex, std::string_view a_optionLabel);

        static const MCMCycleSetting* FindCycle(std::string_view a_settingID);

        static const MCMCycleSetting* FindCycle(const MCMScript& a_script, int a_optionIndex);

        // A disabled camera value can be read, but a click still requires an enabled control.
        static std::optional<int> FindCycleIndex(const MCMScript& a_script, const MCMCycleSetting& a_setting, bool a_requireEnabled = false);

        static std::optional<int> ReadCycleValue(const MCMScript& a_script, const MCMCycleSetting& a_setting);

        static bool ReadCycleSetting(const MCMScript& a_script, CapturedSetting& a_setting);

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

        // Identifies one registry refresh in one loaded game session.
        // Keeping these structs here as they only needed for Menu Redone.
        struct RegistryRequest
        {
            uint64_t loadedGameSession{};

            uint64_t requestID{};
        };

        struct CountResult : public RE::BSScript::IStackCallbackFunctor
        {
            RegistryRequest request;

            explicit CountResult(RegistryRequest a_request) : request(a_request) {}

            void operator()(RE::BSScript::Variable a_result) override;

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
        };

        struct MenuResult : public RE::BSScript::IStackCallbackFunctor
        {
            RegistryRequest request;

            size_t registryIndex{};

            MenuResult(RegistryRequest a_request, size_t a_registryIndex) : request(a_request), registryIndex(a_registryIndex) {}

            void operator()(RE::BSScript::Variable a_result) override;

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
        };

        struct RefreshTask
        {
            RegistryRequest request;

            void operator()() const
            {
                MCMMenuRedoneRegistry::GetSingleton()->DispatchCount(request);
            }
        };

        struct CountTask
        {
            RegistryRequest request;

            int menuCount{-1};

            void operator()() const
            {
                MCMMenuRedoneRegistry::GetSingleton()->ReceiveCount(request, menuCount);
            }
        };

        struct MenuTask
        {
            RE::BSTSmartPointer<RE::BSScript::Object> menuQuest;

            RegistryRequest request;

            size_t registryIndex{};

            void operator()() const
            {
                MCMMenuRedoneRegistry::GetSingleton()->ReceiveMenu(request, registryIndex, menuQuest);
            }
        };

        inline bool IsCurrentRequest(RegistryRequest a_request) const
        {
            return refreshing && a_request.loadedGameSession == currentRequest.loadedGameSession && a_request.requestID == currentRequest.requestID;
        }

        void DispatchCount(RegistryRequest a_request);

        void ReceiveCount(RegistryRequest a_request, int a_menuCount);

        void ReceiveMenu(RegistryRequest a_request, size_t a_registryIndex, RE::BSTSmartPointer<RE::BSScript::Object> a_menuQuest);

        void FailRequest(RegistryRequest a_request, std::string_view a_reason);

        bool Call(std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result);

        void CompleteRequest(size_t a_nativeMenuCount);

        std::mutex registryMutex;

        std::vector<MCMRegistryEntry> registeredMCMs;

        std::vector<std::optional<MCMRegistryEntry>> pendingMCMs;

        RegistryRequest currentRequest;

        uint64_t cacheGeneration{};

        size_t pendingMenuCount{};

        bool refreshing{};

        bool cacheReady{};
    };

}
