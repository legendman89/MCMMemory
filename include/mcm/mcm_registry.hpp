#pragma once

#include "profile/types.hpp"

// SkyUI and MCM Unlocked both keep the live scripts we need to restore registered MCMs.

namespace MCMMemory
{
    inline constexpr RE::FormID markerBaseLocalFormID{ 0x800 };
    inline constexpr RE::FormID markerCellLocalFormID{ 0x801 };
    inline constexpr std::string_view mcmUnlockedPluginName{ "MCM Unlocked.esp" };
    inline constexpr std::string_view markerScriptName{ "MCMUnlockedMarkerScript" };
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

    // Sorts marker references by FormID for a stable registry order.
    struct MCMMarkerFormIDLess
    {
        bool operator()(const RE::NiPointer<RE::TESObjectREFR>& a_left, const RE::NiPointer<RE::TESObjectREFR>& a_right) const noexcept
        {
            return a_left->GetFormID() < a_right->GetFormID();
        }
    };

    class MCMRegistry
    {
    public:

        static bool IsSkyUIAvailable()
        {
            return RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance") != nullptr;
        }

        // Reads MCM Unlocked markers when available, otherwise reads SkyUI registry array.
        std::vector<MCMRegistryEntry> ReadRegisteredMCMs() const;

        std::optional<MCMRegistryEntry> ReadActiveMCM() const;

    private:

        static bool IsMCMUnlockedAvailable()
        {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            return dataHandler && dataHandler->LookupForm<RE::TESObjectACTI>(markerBaseLocalFormID, mcmUnlockedPluginName) && dataHandler->LookupForm<RE::TESObjectCELL>(markerCellLocalFormID, mcmUnlockedPluginName);
        }

        static std::string CreateModID(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, std::string_view a_modName)
        {
            auto* typeInfo = a_mcmScript ? a_mcmScript->GetTypeInfo() : nullptr;
            auto* scriptName = typeInfo ? typeInfo->GetName() : nullptr;
            if (!scriptName || !scriptName[0] || a_modName.empty()) {
                return {};
            }
            return std::format("{}::{}", scriptName, a_modName);
        }

        static std::optional<std::string> ReadModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript);

        static RE::BSTSmartPointer<RE::BSScript::Object> ReadManagerScript();

        static std::vector<MCMRegistryEntry> ReadMCMUnlockedRegistry();

        static std::vector<MCMRegistryEntry> ReadSkyUIRegistry();

        // Follows an MCM Unlocked marker to the live MCM script it represents.
        static std::optional<MCMRegistryEntry> ReadMCMFromMarker(RE::TESObjectREFR* a_marker, RE::BSScript::Internal::VirtualMachine* a_vm, RE::BSScript::IObjectHandlePolicy* a_policy);

        static void TryAddMarker(std::vector<RE::NiPointer<RE::TESObjectREFR>>& a_markers, RE::TESObjectREFR* a_reference, const RE::TESBoundObject* a_markerBase);

        static std::vector<RE::NiPointer<RE::TESObjectREFR>> CollectMCMMarkers(RE::TESObjectCELL* a_markerCell, const RE::TESBoundObject* a_markerBase);

        // Create an MCMRegistryEntry instance for each MCM.
        static std::optional<MCMRegistryEntry> CreateRegistryEntry(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript);
    };
}
