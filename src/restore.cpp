#include "restore.hpp"

namespace MCMMemory
{
    bool Restore::Install()
    {
        std::lock_guard lock(restoreMutex);
        if (installed) {
            return true;
        }

        auto* source = SKSE::GetModCallbackEventSource();
        if (!source) {
            logger::error("Persistent profile restore could not find the SKSE mod callback event source");
            return false;
        }

        source->AddEventSink(this);
        installed = true;
        logger::info("Persistent profile restore event installed");
        return true;
    }

    void Restore::Clear()
    {
        // Leave the event sink installed, but discard everything from the previous restore.
        configLoaded = false;
        configValid = false;
        started = false;
        registryCheckQueued = false;
        currentActionIndex = 0;
        registryWait.Reset();
        stats.Reset();
        mcmStats.Reset();
        scheduledTaskID = 0;
        manualRequest = false;
        requestFailed = false;
        restoreMCMs.clear();
        actions.clear();
    }

    bool Restore::Start()
    {
        std::lock_guard lock(restoreMutex);
        // A new session number makes callbacks from an older restore harmless.
        ++loadedGameSession;
        Clear();
        configLoaded = true;
        configValid = LoadProfile();
        if (!configValid) {
            return false;
        }

        manualRequest = true;
        QueueRegistryCheck();
        logger::info("Manual persistent profile restoration requested");
        return registryCheckQueued;
    }

    void Restore::Reset()
    {
        std::lock_guard lock(restoreMutex);
        // Cancel old tasks and wait for MCM registration again.
        ++loadedGameSession;
        Clear();
        logger::info("Persistent profile restore event reset");
    }

    bool Restore::IsRunning()
    {
        std::lock_guard lock(restoreMutex);
        return registryCheckQueued || (started && currentActionIndex < actions.size());
    }

    RE::BSEventNotifyControl Restore::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
    {
        // SkyUI sends this after its config manager has started registering MCMs.
        if (!a_event || a_event->eventName != "SKICP_configManagerReady") {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::lock_guard lock(restoreMutex);
        if (started || !GetSettings().autoRestore) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (!configLoaded) {
            configLoaded = true;
            configValid = LoadProfile();
        }
        if (!configValid) {
            return RE::BSEventNotifyControl::kContinue;
        }
        if (!registryCheckQueued) {
            QueueRegistryCheck();
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void Restore::CheckRegistry(uint64_t a_loadedGameSession)
    {
        std::lock_guard lock(restoreMutex);
        if (a_loadedGameSession != loadedGameSession) {
            return;
        }

        registryCheckQueued = false;
        if (started || !configValid || (!manualRequest && !GetSettings().autoRestore)) {
            return;
        }

        // The ready event can arrive before every MCM is registered, so wait for an unchanged list.
        auto registeredMCMs = MCMRegistry().ReadRegisteredMCMs();
        const auto result = registryWait.Update(registeredMCMs);
        if (result == RegistryWaitResult::Ready) {
            MatchRegisteredMCMs(registeredMCMs);
            StartRestore();
            return;
        }
        if (result == RegistryWaitResult::Expired) {
            configValid = false;
            logger::error("Persistent profile restore stopped because the MCM registry did not become stable");
            return;
        }
        if (result == RegistryWaitResult::Empty) {
            logger::info("Persistent profile restore is waiting for MCM registry entries (check {})", registryWait.checkCount);
        }
        else if (result == RegistryWaitResult::Changed) {
            logger::info("Persistent profile restore caught {} registered MCMs and is waiting for registration to settle", registeredMCMs.size());
        }
        else {
            logger::info("Persistent profile restore registry is unchanged (quiet check {} of {})", registryWait.quietCheckCount, requiredStableRegistryChecks);
        }

        QueueRegistryCheck(registryCheckDelaySeconds);
    }

    void Restore::StartRestore()
    {
        // Build the per-MCM actions queue.
        BuildActionQueue();
        started = true;
        currentActionIndex = 0;
        if (actions.empty()) {
            logger::warn("Persistent profile restoration has no actions after finding available MCMs");
            return;
        }

        logger::info("Starting persistent profile restoration with {} actions", actions.size());
        QueueNextAction(0.0F);
    }

    void Restore::FinishMCMStats(size_t a_mcmIndex)
    {
        if (a_mcmIndex >= restoreMCMs.size()) {
            return;
        }

        // Keep one short result for the MCM and add it to the final result.
        stats += mcmStats;
        logger::info("Restored '{}': {} applied, {} unchanged, {} skipped", restoreMCMs[a_mcmIndex].identity.modName, mcmStats.appliedSettingCount, mcmStats.unchangedSettingCount, mcmStats.skippedSettingCount);
        mcmStats.Reset();
    }

    void Restore::RunNextAction(uint64_t a_loadedGameSession, uint64_t a_taskID)
    {
        std::lock_guard lock(restoreMutex);
        // Ignore callbacks from an older loaded game or an action that has already been replaced.
        if (a_loadedGameSession != loadedGameSession || a_taskID != scheduledTaskID || currentActionIndex >= actions.size()) {
            return;
        }

        const auto& action = actions[currentActionIndex];
        if (action.type == RestoreActionType::OpenConfig) {
            // Each MCM gets its own applied, unchanged and skipped counts.
            mcmStats.Reset();
        }

        // Some settings need one call to prepare their data and another call to apply the value.
        const bool applyAction = IsRestoreApplyAction(action.type);
        const bool requestAction = action.controlType != ControlType::Unknown && !applyAction;
        const bool directRequestUnneeded = requestAction && action.controlType != ControlType::Menu && currentActionIndex + 1 < actions.size() && !IsActionNeeded(actions[currentActionIndex + 1]);
        bool runAction = true;
        if (directRequestUnneeded) {
            // The live value already matches, so its preparation call is unnecessary.
            runAction = false;
        }
        else if (applyAction && requestFailed) {
            // Never apply a value when the call that prepared its control data failed.
            runAction = false;
            requestFailed = false;
            ++mcmStats.skippedSettingCount;
            logger::warn("Skipping '{}' because its data request failed", action.optionLabel);
        }
        else if (!IsActionValid(action)) {
            // The option index may now belong to another control after a mod update.
            runAction = false;
            if (requestAction) {
                requestFailed = true;
            }
            if (applyAction) {
                ++mcmStats.skippedSettingCount;
                logger::warn("Skipping changed or missing control '{}' in '{}'", action.optionLabel, restoreMCMs[action.mcmIndex].identity.modID);
            }
        }
        else if (applyAction && !IsActionNeeded(action)) {
            // Avoid triggering the MCM callback when the saved value is up to date.
            runAction = false;
            ++mcmStats.unchangedSettingCount;
            logger::debug("Profile setting '{}' already matches", action.optionLabel);
        }

        bool dispatched{};
        uint64_t continuationTaskID{};
        if (runAction) {
            // A successful Papyrus call queues the next action through this callback.
            continuationTaskID = ++scheduledTaskID;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new MCMCallResult(RestoreTask{ loadedGameSession, continuationTaskID }));
            dispatched = RunAction(action, std::move(result));
            if (!dispatched) {
                if (requestAction) {
                    requestFailed = true;
                }
                if (applyAction) {
                    ++mcmStats.skippedSettingCount;
                }
                logger::error("Profile restore action {} ({}) failed; continuing with the remaining profile", currentActionIndex, RestoreActionFunctionName(action.type));
            }
            else if (applyAction) {
                ++mcmStats.appliedSettingCount;
            }
        }

        if (applyAction) {
            // A failed request only belongs to the apply action immediately after it.
            requestFailed = false;
        }
        if (action.type == RestoreActionType::CloseConfig) {
            FinishMCMStats(action.mcmIndex);
        }

        ++currentActionIndex;
        if (currentActionIndex >= actions.size()) {
            logger::info("Persistent profile restoration completed: {} applied, {} unchanged, {} skipped", stats.appliedSettingCount, stats.unchangedSettingCount, stats.skippedSettingCount);
            return;
        }

        if (!dispatched) {
            // Skipped and failed calls have no Papyrus callback to continue the queue.
            QueueNextAction(0.0F);
        }
    }
}
