#include "menu/hud.hpp"
#include "profile/restore.hpp"

#include "mcm/mcm_support.hpp"

namespace MCMMemory
{
    bool Restore::Install()
    {
        std::lock_guard lock(restoreMutex);
        if (installed) {
            return true;
        }

        auto* modEvents = SKSE::GetModCallbackEventSource();
        auto* ui = RE::UI::GetSingleton();
        if (!modEvents || !ui) {
            logger::error("Persistent profile restore could not find its event sources");
            return false;
        }

        modEvents->AddEventSink(static_cast<RE::BSTEventSink<SKSE::ModCallbackEvent>*>(this));
        ui->AddEventSink<RE::MenuOpenCloseEvent>(static_cast<RE::BSTEventSink<RE::MenuOpenCloseEvent>*>(this));
        installed = true;
        logger::info("Persistent profile restore events installed");
        return true;
    }

    void Restore::Clear()
    {
        // Leave the event sink installed, but discard everything from the previous restore.
        configLoaded = false;
        configValid = false;
        started = false;
        restoring = false;
        registryCheckQueued = false;
        currentActionIndex = 0;
        activeMCMIndex = 0;
        registryWait.Reset();
        stats.Reset();
        mcmStats.Reset();
        scheduledTaskID = 0;
        operationMode = OperationMode::Automatic;
        status = OperationStatus::Idle;
        requestFailed = false;
        journalMenuOpen = false;
        characterCreationOpen = false;
        mcmStarted = false;
        mcmOpen = false;
        mcmStatsRecorded = false;
        restoreMCMs.clear();
        actions.clear();
        activityMods.clear();
        mcmFilter.clear();
    }

    bool Restore::Begin(MCMFilter a_filter)
    {
        std::lock_guard lock(restoreMutex);
        if (status != OperationStatus::Idle) {
            logger::warn("Persistent profile restoration is already running");
            return false;
        }

        // A new session number makes callbacks from an older restore harmless.
        ++loadedGameSession;
        Clear();
        operationMode = OperationMode::Manual;
        mcmFilter = std::move(a_filter);
        configLoaded = true;
        configValid = LoadProfile();
        if (!configValid) {
            HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreFailed", "HUD.Failure.ProfileLoadFailed");
            return false;
        }

        status = OperationStatus::Running;
        HUD::GetSingleton()->ShowRestoreStarted();

        const auto registeredMCMs = MCMRegistry().ReadRegisteredMCMs();
        if (MCMRegistry::IsMCMMenuRedoneAvailable() && (registeredMCMs.empty() || MCMRegistry::IsRefreshing())) {
            MCMRegistry::Refresh();
            QueueRegistryCheck(GetSettings().actionTrialDelaySeconds);
            logger::info("Manual persistent profile restoration is waiting for the MCM Menu Redone registry");
            return status == OperationStatus::Running;
        }

        if (registeredMCMs.empty()) {
            HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreFailed", "HUD.Failure.NoRegisteredMCMs");
            logger::warn("Manual persistent profile restoration found no registered MCMs");
            status = OperationStatus::Idle;
            return false;
        }

        MatchRegisteredMCMs(registeredMCMs);
        StartRestore();
        logger::info("Manual persistent profile restoration started from {} registered MCMs", registeredMCMs.size());
        return status == OperationStatus::Running;
    }

    bool Restore::Cancel()
    {
        std::lock_guard lock(restoreMutex);
        if (status != OperationStatus::Running) {
            return false;
        }

        status = OperationStatus::Stopping;
        started = true;
        logger::info("Persistent profile restoration cancellation requested");
        return true;
    }

    void Restore::Reset(bool a_autoRestoreAllowed)
    {
        std::lock_guard lock(restoreMutex);
        // Cancel old tasks and wait for MCM registration again.
        ++loadedGameSession;
        Clear();
        autoRestoreAllowed = a_autoRestoreAllowed;
        logger::info("Persistent profile restore event reset; automatic restore allowed={}", autoRestoreAllowed);
    }

    RE::BSEventNotifyControl Restore::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
    {
        // SkyUI sends this after its config manager has started registering MCMs.
        if (!a_event || a_event->eventName != "SKICP_configManagerReady") {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::lock_guard lock(restoreMutex);
        if (started || !autoRestoreAllowed || !GetSettings().autoRestore) {
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
            status = OperationStatus::Running;
            QueueRegistryCheck();
            if (!registryCheckQueued) {
                status = OperationStatus::Idle;
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl Restore::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
    {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        const std::string_view menuName{ a_event->menuName.c_str() };
        const bool journalMenu = menuName == RE::JournalMenu::MENU_NAME;
        const bool characterMenu = menuName == RE::RaceSexMenu::MENU_NAME;
        if (!journalMenu && !characterMenu) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::lock_guard lock(restoreMutex);
        if (characterMenu) {
            characterCreationOpen = a_event->opening;
            if (restoring) {
                if (characterCreationOpen) {
                    logger::info("Character creation opened; MCM restore actions are paused");
                }
                else {
                    logger::info("Character creation closed; MCM restore actions are resuming");
                }
            }
            else if (!started) {
                registryWait.Reset();
                if (characterCreationOpen) {
                    logger::info("Character creation opened; MCM registry checks are paused");
                }
                else {
                    logger::info("Character creation closed; MCM registry stability will be checked again");
                }
                if (!characterCreationOpen && configValid && !registryCheckQueued) {
                    QueueRegistryCheck();
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        journalMenuOpen = a_event->opening;
        if (journalMenuOpen && restoring) {
            CloseJournalMenu();
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
        if (status == OperationStatus::Stopping) {
            FinishCancellation();
            return;
        }
        if (started || !configValid || (operationMode == OperationMode::Automatic && (!autoRestoreAllowed || !GetSettings().autoRestore))) {
            return;
        }

        auto* ui = RE::UI::GetSingleton();
        const bool characterMenuOpen = ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
        if (characterMenuOpen) {
            if (!characterCreationOpen) {
                logger::info("Character creation is open; MCM registry checks are paused");
            }
            characterCreationOpen = true;
            registryWait.Reset();
            QueueRegistryCheck(registryCheckDelaySeconds);
            return;
        }
        if (characterCreationOpen) {
            characterCreationOpen = false;
            registryWait.Reset();
            logger::info("Character creation has closed; restarting the MCM registry stability check");
        }

        // The ready event can arrive before every MCM is registered, so wait for an unchanged list.
        auto registeredMCMs = MCMRegistry().ReadRegisteredMCMs();
        if (MCMRegistry::IsRefreshing()) {
            logger::debug("Persistent profile restore is waiting for the MCM Menu Redone registry query");
            QueueRegistryCheck(operationMode == OperationMode::Manual ? GetSettings().actionTrialDelaySeconds : registryCheckDelaySeconds);
            return;
        }

        if (operationMode == OperationMode::Manual) {
            if (!registeredMCMs.empty()) {
                MatchRegisteredMCMs(registeredMCMs);
                StartRestore();
                return;
            }

            const auto result = registryWait.Update(registeredMCMs);
            if (result == RegistryWaitResult::Expired) {
                logger::error("Manual persistent profile restoration found no registered MCMs");
                HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreFailed", "HUD.Failure.NoRegisteredMCMs");
                status = OperationStatus::Idle;
                return;
            }

            MCMRegistry::Refresh();
            QueueRegistryCheck(GetSettings().actionTrialDelaySeconds);
            return;
        }

        const auto result = registryWait.Update(registeredMCMs);
        if (result == RegistryWaitResult::Ready) {
            MatchRegisteredMCMs(registeredMCMs);
            StartRestore();
            return;
        }
        if (result == RegistryWaitResult::Expired) {
            configValid = false;
            logger::error("Persistent profile restore stopped because the MCM registry did not become stable");
            HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreFailed", "HUD.Failure.RegistrationIncomplete");
            status = OperationStatus::Idle;
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

        MCMRegistry::Refresh();
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
            if (operationMode == OperationMode::Manual) {
                HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreStopped", "HUD.Failure.NoMatchingMCMs");
            }
            status = OperationStatus::Idle;
            return;
        }

        restoring = true;

        if (operationMode == OperationMode::Automatic) {
            HUD::GetSingleton()->ShowRestoreStarted();
        }
        if (auto* ui = RE::UI::GetSingleton()) {
            journalMenuOpen = ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);
        }
        if (journalMenuOpen) {
            CloseJournalMenu();
        }
        logger::info("Starting persistent profile restoration with {} actions", actions.size());
        QueueNextAction(0.0F);
    }

    void Restore::FinishRestore()
    {
        restoring = false;
        status = OperationStatus::Idle;
        HUD::GetSingleton()->HideRestoreMenuWarning();
        logger::info("Persistent profile restoration completed: {} applied, {} unchanged, {} skipped", stats.appliedSettingCount, stats.unchangedSettingCount, stats.skippedSettingCount);
        Activity::GetSingleton()->RecordRestore(operationMode, stats, activityMods);
        HUD::GetSingleton()->ShowRestoreSummary(stats);
    }

    void Restore::FinishMCMStats(size_t a_mcmIndex, OperationResult a_result)
    {
        if (a_mcmIndex >= restoreMCMs.size()) {
            return;
        }

        // Keep one short result for the MCM and add it to the final result.
        mcmStats.MCMCount = 1;
        const auto& identity = restoreMCMs[a_mcmIndex].identity;
        activityMods.emplace_back(identity, mcmStats, a_result);

        if (a_result == OperationResult::Completed) {
            HUD::GetSingleton()->ShowRestoreMCM(identity.modName, mcmStats, operationMode);
        }
        stats += mcmStats;
        mcmStatsRecorded = true;
        logger::info("Recorded '{}' restore result '{}': {} applied, {} unchanged, {} skipped", identity.modName, OperationResultName(a_result), mcmStats.appliedSettingCount, mcmStats.unchangedSettingCount, mcmStats.skippedSettingCount);
        mcmStats.Reset();
    }

    void Restore::CloseJournalMenu()
    {
        auto* messages = RE::UIMessageQueue::GetSingleton();
        if (!messages) {
            logger::error("Persistent profile restore could not close the Journal Menu");
            return;
        }

        messages->AddMessage(RE::JournalMenu::MENU_NAME.data(), RE::UI_MESSAGE_TYPE::kHide, nullptr);
        HUD::GetSingleton()->ShowRestoreMenuWarning();
        logger::warn("Journal Menu opened during profile restore and was closed");
    }

    void Restore::RunNextAction(uint64_t a_loadedGameSession, uint64_t a_taskID)
    {
        std::lock_guard lock(restoreMutex);
        // Ignore callbacks from an older loaded game or an action that has already been replaced.
        if (a_loadedGameSession != loadedGameSession || a_taskID != scheduledTaskID || !restoring) {
            return;
        }
        if (status == OperationStatus::Stopping) {
            ContinueCancellation();
            return;
        }
        if (currentActionIndex >= actions.size()) {
            FinishRestore();
            return;
        }
        if (auto* ui = RE::UI::GetSingleton()) {
            journalMenuOpen = ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);
            characterCreationOpen = ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
        }
        if (characterCreationOpen) {
            QueueNextAction(GetSettings().actionTrialDelaySeconds);
            return;
        }
        if (journalMenuOpen) {
            CloseJournalMenu();
            QueueNextAction(GetSettings().actionTrialDelaySeconds);
            return;
        }

        const auto& action = actions[currentActionIndex];
        if (action.type == RestoreActionType::OpenConfig) {
            // Each MCM gets its own applied, unchanged and skipped counts.
            mcmStats.Reset();
            activeMCMIndex = action.mcmIndex;
            mcmStarted = true;
            mcmStatsRecorded = false;
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
            if (action.type == RestoreActionType::OpenConfig && dispatched) {
                mcmOpen = true;
            }
        }

        if (applyAction) {
            // A failed request only belongs to the apply action immediately after it.
            requestFailed = false;
        }
        if (action.type == RestoreActionType::CloseConfig) {
            mcmOpen = false;
            FinishMCMStats(action.mcmIndex);
        }

        ++currentActionIndex;
        if (currentActionIndex >= actions.size()) {
            if (!dispatched) {
                FinishRestore();
            }
            return;
        }

        if (!dispatched) {
            // Skipped and failed calls have no Papyrus callback to continue the queue.
            QueueNextAction(0.0F);
        }
    }

    void Restore::ContinueCancellation()
    {
        // Close an open MCM through the same callback queue before stopping.
        if (mcmOpen && activeMCMIndex < restoreMCMs.size()) {
            mcmOpen = false;
            const uint64_t continuationTaskID = ++scheduledTaskID;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new MCMCallResult(RestoreTask{ loadedGameSession, continuationTaskID }));
            if (CallMCMFunction(activeMCMIndex, "CloseConfig", RE::MakeFunctionArguments(), std::move(result))) {
                logger::info("Persistent profile restore is closing '{}' before cancellation", restoreMCMs[activeMCMIndex].identity.modID);
                return;
            }
            logger::warn("Persistent profile restore could not close '{}' during cancellation", restoreMCMs[activeMCMIndex].identity.modID);
        }

        FinishCancellation();
    }

    void Restore::FinishCancellation()
    {
        // Values already applied remain changed, so record the interrupted result.
        if (mcmStarted && !mcmStatsRecorded) {
            FinishMCMStats(activeMCMIndex, OperationResult::Cancelled);
        }

        ++scheduledTaskID;
        restoring = false;
        registryCheckQueued = false;
        status = OperationStatus::Idle;
        HUD::GetSingleton()->HideRestoreMenuWarning();
        Activity::GetSingleton()->RecordRestore(operationMode, stats, activityMods, OperationResult::Cancelled);
        HUD::GetSingleton()->ShowRestoreCancelled(stats);
        logger::info("Persistent profile restoration cancelled after changing {} settings in {} MCMs", stats.appliedSettingCount, stats.MCMCount);
    }
}
