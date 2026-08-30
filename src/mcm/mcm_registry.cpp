#include "mcm/mcm_registry.hpp"

namespace MCMMemory
{
    RegistryWaitResult RegistryWait::Update(const std::vector<MCMRegistryEntry>& a_registeredMCMs)
    {
        ++checkCount;
        std::vector<std::string> currentModIDs;
        currentModIDs.reserve(a_registeredMCMs.size());
        for (const auto& mcm : a_registeredMCMs) {
            currentModIDs.push_back(mcm.identity.modID);
        }
        std::sort(currentModIDs.begin(), currentModIDs.end());

        if (currentModIDs.empty()) {
            quietCheckCount = 0;
            return checkCount >= maximumRegistryChecks ? RegistryWaitResult::Expired : RegistryWaitResult::Empty;
        }
        if (currentModIDs != modIDs) {
            modIDs = std::move(currentModIDs);
            quietCheckCount = 0;
            return checkCount >= maximumRegistryChecks ? RegistryWaitResult::Expired : RegistryWaitResult::Changed;
        }

        ++quietCheckCount;
        if (quietCheckCount >= requiredStableRegistryChecks) {
            return RegistryWaitResult::Ready;
        }
        return checkCount >= maximumRegistryChecks ? RegistryWaitResult::Expired : RegistryWaitResult::Waiting;
    }

    std::optional<std::string> MCMRegistry::ReadModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, std::string* a_failureReason)
    {
        if (!a_mcmScript) {
            if (a_failureReason) {
                *a_failureReason = "the script object is unavailable";
            }
            return std::nullopt;
        }

        const RE::BSScript::Variable* value = a_mcmScript->GetProperty("ModName");
        if (!value || !value->IsString()) {
            value = a_mcmScript->GetVariable("::ModName_var");
        }

        if (!value) {
            if (a_failureReason) {
                *a_failureReason = "ModName is missing";
            }
            return std::nullopt;
        }
        if (!value->IsString()) {
            if (a_failureReason) {
                *a_failureReason = "ModName is not text";
            }
            return std::nullopt;
        }
        if (value->GetString().empty()) {
            if (a_failureReason) {
                *a_failureReason = "ModName is empty";
            }
            return std::nullopt;
        }
        return std::string(value->GetString());
    }

    RE::BSTSmartPointer<RE::BSScript::Object> MCMRegistry::ReadManagerScript()
    {
        // SkyUI stores its MCM manager on this quest.
        auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance");
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!quest || !vm || !policy) {
            return {};
        }

        auto handle = policy->GetHandleForObject(quest->GetFormType(), quest);
        RE::BSTSmartPointer<RE::BSScript::Object> managerScript;
        if (handle == policy->EmptyHandle() || !vm->FindBoundObject(handle, "SKI_ConfigManager", managerScript)) {
            return {};
        }
        return managerScript;
    }

    std::optional<MCMRegistryEntry> MCMRegistry::CreateRegistryEntry(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript, std::string* a_failureReason)
    {
        auto mcmScript = a_mcmScript;
        auto modName = ReadModName(mcmScript, a_failureReason);
        if (!modName) {
            return std::nullopt;
        }

        auto modID = CreateModID(mcmScript, *modName);
        if (modID.empty()) {
            if (a_failureReason) {
                *a_failureReason = "the script name is unavailable";
            }
            return std::nullopt;
        }
        MCMIdentity identity{ std::move(*modName), std::move(modID) };
        return MCMRegistryEntry{ std::move(identity), std::move(mcmScript) };
    }

    std::vector<MCMRegistryEntry> MCMRegistry::ReadSkyUIRegistry()
    {
        auto managerScript = ReadManagerScript();
        if (!managerScript) {
            return {};
        }

        // SkyUI stores every registered SKI_ConfigBase script in this array.
        const RE::BSScript::Variable* registeredMCMValue = managerScript->GetVariable("_modConfigs");
        if (!registeredMCMValue || !registeredMCMValue->IsObjectArray()) {
            logger::warn("SkyUI registry could not read _modConfigs; another SKI_ConfigManager script may be overriding SkyUI");
            return {};
        }

        auto registeredMCMArray = registeredMCMValue->GetArray();
        if (!registeredMCMArray) {
            return {};
        }

        // Cache the live scripts before doing any extra work with their names.
        std::vector<RE::BSTSmartPointer<RE::BSScript::Object>> mcmScripts;
        std::vector<size_t> registryIndices;
        mcmScripts.reserve(registeredMCMArray->size());
        registryIndices.reserve(registeredMCMArray->size());
        size_t registryIndex = 0;
        for (const auto& value : *registeredMCMArray) {
            if (value.IsObject()) {
                auto mcmScript = value.GetObject();
                if (mcmScript) {
                    mcmScripts.push_back(std::move(mcmScript));
                    registryIndices.push_back(registryIndex);
                }
            }
            ++registryIndex;
        }

        std::vector<MCMRegistryEntry> registeredMCMs;
        registeredMCMs.reserve(mcmScripts.size());
        for (size_t index = 0; index < mcmScripts.size(); ++index) {
            const auto& mcmScript = mcmScripts[index];
            std::string failureReason;
            auto registeredMCM = CreateRegistryEntry(mcmScript, std::addressof(failureReason));
            if (registeredMCM) {
                logger::debug("SkyUI registry entry {} found '{}' with raw ModName '{}'", registryIndices[index], registeredMCM->identity.modID, registeredMCM->identity.modName);
                registeredMCMs.push_back(std::move(*registeredMCM));
            }
            else {
                const auto* scriptName = ReadScriptName(mcmScript);
                logger::debug("SkyUI registry skipped entry {}: script='{}', {}", registryIndices[index], scriptName ? scriptName : "<unknown>", failureReason);
            }
        }

        logger::info("SkyUI registry read {} MCM scripts from {} occupied entries", registeredMCMs.size(), mcmScripts.size());
        return registeredMCMs;
    }

    std::optional<MCMRegistryEntry> MCMRegistry::ReadActiveMCM() const
    {
        auto managerScript = ReadManagerScript();
        if (!managerScript) {
            return std::nullopt;
        }

        const RE::BSScript::Variable* activeMCMValue = managerScript->GetVariable("_activeConfig");
        auto mcmScript = activeMCMValue && activeMCMValue->IsObject() ? activeMCMValue->GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>();
        return CreateRegistryEntry(mcmScript);
    }

}
