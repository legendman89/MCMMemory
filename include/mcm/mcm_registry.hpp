#pragma once

#include "profile/types.hpp"

// Each supported MCM manager gives us the live scripts needed by backup and restore.

namespace MCMMemory
{

    inline constexpr uint32_t maximumRegistryChecks{ 30 };
    inline constexpr uint32_t requiredStableRegistryChecks{ 2 };
    inline constexpr float registryCheckDelaySeconds{ 5.0F };

    enum class RegistryWaitResult
    {
        Empty,
        Changed,
        Waiting,
        Ready,
        Expired
    };

    struct MCMRegistryEntry
    {
        // Visible name and stable ID made from the config script name and visible mod name.
        // I found out the mod name is not enough because two MCMs could use similar names.
        // The script type helps distinguish them. Therefore, we combine mod name + script name.
        MCMIdentity identity;

        // Live MCM script used by restore.
        RE::BSTSmartPointer<RE::BSScript::Object> mcmScript;

        explicit MCMRegistryEntry(MCMIdentity a_identity, RE::BSTSmartPointer<RE::BSScript::Object> a_mcmScript) :
            identity(std::move(a_identity)), mcmScript(std::move(a_mcmScript)) {}
    };

    struct RegistryWait
    {
        // Keeps the last sorted registry so changes can be detected.
        std::vector<std::string> modIDs;

        // Counts every registry read in the current operation.
        uint32_t checkCount{};

        // Counts consecutive unchanged registry reads.
        uint32_t quietCheckCount{};

        RegistryWaitResult Update(const std::vector<MCMRegistryEntry>& a_registeredMCMs);

        void Reset()
        {
            modIDs.clear();
            checkCount = 0;
            quietCheckCount = 0;
        }
    };

    // Forward it to keep it private to the implementation.
    class MCMMenuRedoneRegistry;
    class MCMMenuMaidRegistry;

    class MCMRegistry
    {
    public:

        static bool IsSkyUIAvailable()
        {
            return RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance") != nullptr;
        }
        
        static void Install();

        static void Reset();

        static void Refresh();

        static bool IsRefreshing();

        static uint64_t CacheGeneration();

        static bool IsMCMMenuRedoneAvailable();

        static bool IsMCMMenuMaidAvailable();

        static bool UsesCachedRegistry();

        // Uses the registry provided by MCM Menu Redone, Menu Maid 2, MCM Unlocked, or SkyUI.
        std::vector<MCMRegistryEntry> ReadRegisteredMCMs() const;

        std::optional<MCMRegistryEntry> ReadActiveMCM() const;

    private:

        // Allows the MCM Menu Redone registry to use the private CreateRegistryEntry helper.
        friend class MCMMenuRedoneRegistry;

        // Allows the Menu Maid 2 registry to turn its returned forms into normal registry we can use.
        friend class MCMMenuMaidRegistry;


        static const char* ReadScriptName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript)
        {
            auto* typeInfo = a_mcmScript ? a_mcmScript->GetTypeInfo() : nullptr;
            auto* scriptName = typeInfo ? typeInfo->GetName() : nullptr;
            return scriptName && scriptName[0] ? scriptName : nullptr;
        }

        static std::string CreateModID(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, std::string_view a_modName)
        {
            const auto* scriptName = ReadScriptName(a_mcmScript);
            if (!scriptName || a_modName.empty()) {
                return {};
            }
            return std::format("{}::{}", scriptName, a_modName);
        }

        static bool IsMCMUnlockedAvailable();

        static std::optional<std::string> ReadModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, std::string* a_failureReason = nullptr);

        static RE::BSTSmartPointer<RE::BSScript::Object> ReadManagerScript();

        static std::vector<MCMRegistryEntry> ReadMCMUnlockedRegistry();

        static std::vector<MCMRegistryEntry> ReadSkyUIRegistry();

        // Follows an MCM Unlocked marker to the live MCM script it represents.
        static std::optional<MCMRegistryEntry> ReadMCMFromMarker(RE::TESObjectREFR* a_marker, RE::BSScript::Internal::VirtualMachine* a_vm, RE::BSScript::IObjectHandlePolicy* a_policy);

        static void TryAddMarker(std::vector<RE::NiPointer<RE::TESObjectREFR>>& a_markers, RE::TESObjectREFR* a_reference, const RE::TESBoundObject* a_markerBase);

        static std::vector<RE::NiPointer<RE::TESObjectREFR>> CollectMCMMarkers(RE::TESObjectCELL* a_markerCell, const RE::TESBoundObject* a_markerBase);

        // Create an MCMRegistryEntry instance for each MCM.
        static std::optional<MCMRegistryEntry> CreateRegistryEntry(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, std::string* a_failureReason = nullptr);
    };
}
