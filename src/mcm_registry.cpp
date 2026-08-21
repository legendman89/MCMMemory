#include "mcm_registry.hpp"

namespace MCMMemory
{

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

    std::optional<std::string> MCMRegistry::ReadModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript)
    {
        if (!a_mcmScript) {
            return std::nullopt;
        }
        const RE::BSScript::Variable* value = a_mcmScript->GetProperty("ModName");
        if (!value || !value->IsString()) {
            value = a_mcmScript->GetVariable("::ModName_var");
        }
        if (!value || !value->IsString() || value->GetString().empty()) {
            return std::nullopt;
        }
        return std::string(value->GetString());
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

        // InstanceScript is what points to the live MCM script we want.
        const RE::BSScript::Variable* mcmScriptValue = markerScript->GetProperty("InstanceScript");
        if (!mcmScriptValue || !mcmScriptValue->IsObject()) {
            mcmScriptValue = markerScript->GetVariable("::InstanceScript_var");
        }
        auto mcmScript = mcmScriptValue && mcmScriptValue->IsObject() ? mcmScriptValue->GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>();
        return CreateRegistryEntry(mcmScript, a_marker->GetFormID());
    }

    std::optional<MCMRegistryEntry> MCMRegistry::CreateRegistryEntry(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, RE::FormID a_markerFormID)
    {
        auto mcmScript = a_mcmScript;
        auto modName = ReadModName(mcmScript);
        if (!mcmScript || !modName) {
            return std::nullopt;
        }

        auto modID = CreateModID(mcmScript, *modName);
        if (modID.empty()) {
            return std::nullopt;
        }
        MCMIdentity identity{ std::move(*modName), std::move(modID) };
        return MCMRegistryEntry{ a_markerFormID, std::move(identity), std::move(mcmScript) };
    }

    std::vector<MCMRegistryEntry> MCMRegistry::ReadRegisteredMCMs() const
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!dataHandler || !vm || !policy) {
            return {};
        }

        auto* markerBase = dataHandler->LookupForm<RE::TESObjectACTI>(markerBaseLocalFormID, pluginName);
        auto* markerCell = dataHandler->LookupForm<RE::TESObjectCELL>(markerCellLocalFormID, pluginName);
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
        logger::info("MCM registry traced {} MCM scripts from {} marker references", registeredMCMs.size(), markers.size());
        return registeredMCMs;
    }

    std::optional<MCMRegistryEntry> MCMRegistry::ReadActiveMCM() const
    {
        // SkyUI stores its MCM manager on a quest, so find that quest to access the active MCM script.
        auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance");
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!quest || !vm || !policy) {
            return std::nullopt;
        }

        auto handle = policy->GetHandleForObject(quest->GetFormType(), quest);
        RE::BSTSmartPointer<RE::BSScript::Object> managerScript;
        if (handle == policy->EmptyHandle() || !vm->FindBoundObject(handle, "SKI_ConfigManager", managerScript) || !managerScript) {
            return std::nullopt;
        }

        const RE::BSScript::Variable* activeMCMValue = managerScript->GetVariable("_activeConfig");
        auto mcmScript = activeMCMValue && activeMCMValue->IsObject() ? activeMCMValue->GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>();
        return CreateRegistryEntry(mcmScript, 0);
    }

    std::optional<MCMRegistryEntry> MCMRegistry::FindRegisteredMCM(std::string_view a_modID) const
    {
        const auto registeredMCMs = ReadRegisteredMCMs();
        for (const auto& mcm : registeredMCMs) {
            if (mcm.identity.modID == a_modID) {
                logger::info("MCM registry found '{}' as '{}' using marker {:08X}", mcm.identity.modID, mcm.identity.modName, mcm.markerFormID);
                return mcm;
            }
        }
        return std::nullopt;
    }
}
