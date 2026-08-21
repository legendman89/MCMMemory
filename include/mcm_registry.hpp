#pragma once

#include "types.hpp"

// Here we check and register MCM unlocked.
// It gives us the live script for every registered MCM, which we need to restore its settings.

namespace MCMMemory
{

    inline constexpr RE::FormID markerBaseLocalFormID{ 0x800 };
    inline constexpr RE::FormID markerCellLocalFormID{ 0x801 };
    inline constexpr std::string_view pluginName{ "MCM Unlocked.esp" };
    inline constexpr std::string_view markerScriptName{ "MCMUnlockedMarkerScript" };

    struct MCMRegistryEntry
    {
        // Visible name and stable ID made from the config script name and visible mod name.
        // I found out the mod name is not enough because two MCMs could use similar names. 
        // The script type helps distinguish them. Therefore, we combine mod name + script name.
        MCMIdentity identity;

        // Live MCM script used by restore.
        RE::BSTSmartPointer<RE::BSScript::Object> mcmScript;

        // A marker is a hidden Skyrim object created by MCM unlocked for every registered MCM.
        // We need those to trace them back to their MCM script (mcmScript).
        RE::FormID markerFormID{};

        explicit MCMRegistryEntry(RE::FormID a_markerFormID, MCMIdentity a_identity, RE::BSTSmartPointer<RE::BSScript::Object> a_mcmScript) :
            identity(std::move(a_identity)), mcmScript(std::move(a_mcmScript)), markerFormID(a_markerFormID) {}
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

        inline bool IsMCMUnlockedAvailable() const
        {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            return dataHandler && dataHandler->LookupForm<RE::TESObjectACTI>(markerBaseLocalFormID, pluginName) && dataHandler->LookupForm<RE::TESObjectCELL>(markerCellLocalFormID, pluginName);
        }
        
        // Trace back all registered MCMs through their markers.
        std::vector<MCMRegistryEntry> ReadRegisteredMCMs() const;

        std::optional<MCMRegistryEntry> ReadActiveMCM() const;

        std::optional<MCMRegistryEntry> FindRegisteredMCM(std::string_view a_modID) const;


    private:

        inline static std::string CreateModID(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, std::string_view a_modName)
        {
            auto* typeInfo = a_mcmScript ? a_mcmScript->GetTypeInfo() : nullptr;
            auto* scriptName = typeInfo ? typeInfo->GetName() : nullptr;
            if (!scriptName || !scriptName[0] || a_modName.empty()) {
                return {};
            }
            return std::format("{}::{}", scriptName, a_modName);
        }

        static std::optional<std::string> ReadModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript);

        // Follows an MCM Unlocked marker to the live MCM script it represents.
        static std::optional<MCMRegistryEntry> ReadMCMFromMarker(RE::TESObjectREFR* a_marker, RE::BSScript::Internal::VirtualMachine* a_vm, RE::BSScript::IObjectHandlePolicy* a_policy);

        static void TryAddMarker(std::vector<RE::NiPointer<RE::TESObjectREFR>>& a_markers, RE::TESObjectREFR* a_reference, const RE::TESBoundObject* a_markerBase);

        static std::vector<RE::NiPointer<RE::TESObjectREFR>> CollectMCMMarkers(RE::TESObjectCELL* a_markerCell, const RE::TESBoundObject* a_markerBase);

        // Create an MCMRegistryEntry instance for each MCM.
        static std::optional<MCMRegistryEntry> CreateRegistryEntry(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, RE::FormID a_markerFormID);
        
    };
}
