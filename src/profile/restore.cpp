#include "menu/hud.hpp"
#include "profile/restore.hpp"

#include "mcm/mcm_support.hpp"
#include "utils/helper.hpp"

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
        callWatch.Release();
        firstActionIndex = 0;
        pendingActionIndex = static_cast<size_t>(-1);
        mcmFailed = false;
        configLoaded = false;
        configValid = false;
        started = false;
        restoring = false;
        waitingForCharacterCreation = false;
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
        callWatch.Cancel();
        if (!restoring) {
            // Registration cancellation does not depend on the next registry event.
            QueueNextAction(0.0F);
        }
        logger::info("Persistent profile restoration cancellation requested");
        return true;
    }

    bool Restore::QueueWatch()
    {
        if (Scheduler::GetSingleton()->ScheduleAfterSeconds(MCMWatchTask<Restore>{ this, loadedGameSession }, mcmWatchIntervalSeconds)) {
            return true;
        }
        logger::error("Persistent profile restore could not schedule its watchdog");
        FinishCancellation(OperationResult::Failed, callWatch.HasCall() || mcmOpen);
        return false;
    }

    void Restore::CheckCalls(uint64_t a_loadedGameSession)
    {
        std::lock_guard lock(restoreMutex);
        if (a_loadedGameSession != loadedGameSession || status == OperationStatus::Idle) {
            return;
        }
        const auto callStatus = callWatch.Check();
        if (callStatus == MCMCallStatus::Expired) {
            HandleExpiredCall();
            if (status != OperationStatus::Idle) {
                QueueWatch();
            }
            return;
        }
        if (callStatus == MCMCallStatus::Completed || (status == OperationStatus::Stopping && callStatus == MCMCallStatus::None)) {
            auto* tasks = SKSE::GetTaskInterface();
            if (tasks) {
                tasks->AddTask(RestoreTask{ loadedGameSession, scheduledTaskID });
            }
        }
        QueueWatch();
    }

    void Restore::Reset(bool a_autoRestoreAllowed)
    {
        std::lock_guard lock(restoreMutex);
        // Cancel old tasks and wait for MCM registration again.
        ++loadedGameSession;
        Clear();
        autoRestoreAllowed = a_autoRestoreAllowed;
        waitingForCharacterCreation = a_autoRestoreAllowed;
        HUD::GetSingleton()->HideMenuWarning();
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
                    waitingForCharacterCreation = false;
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
        if (operationMode == OperationMode::Automatic && waitingForCharacterCreation) {
            if (characterMenuOpen) {
                characterCreationOpen = true;
            }
            else if (characterCreationOpen) {
                characterCreationOpen = false;
                waitingForCharacterCreation = false;
                registryWait.Reset();
                logger::info("Character creation has closed; starting the MCM registry stability check");
            }

            if (waitingForCharacterCreation) {
                QueueRegistryCheck(registryCheckDelaySeconds);
                return;
            }
        }
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
        if (!callWatch.Acquire()) {
            started = true;
            status = OperationStatus::Idle;
            HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreStopped", "HUD.Failure.ScriptBusy");
            return;
        }
        // Build the per-MCM actions queue.
        BuildActionQueue();
        started = true;
        currentActionIndex = 0;
        if (actions.empty()) {
            callWatch.Release();
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
        if (!QueueWatch()) {
            return;
        }
        QueueNextAction(0.0F);
    }

    void Restore::FinishRestore()
    {
        callWatch.Release();
        restoring = false;
        status = OperationStatus::Idle;
        HUD::GetSingleton()->HideMenuWarning();
        logger::info("Persistent profile restoration completed: {} applied, {} unchanged, {} skipped", stats.appliedSettingCount, stats.unchangedSettingCount, stats.skippedSettingCount);
        const auto result = MCMResultsStatus(activityMods);
        Activity::GetSingleton()->RecordRestore(operationMode, stats, activityMods, result);
        if (result == OperationResult::Completed) {
            HUD::GetSingleton()->ShowRestoreSummary(stats);
        }
        else {
            HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreStopped", "HUD.Failure.RestoreIncomplete");
        }
    }

    void Restore::FinishMCMStats(size_t a_mcmIndex, OperationResult a_result)
    {
        if (a_mcmIndex >= restoreMCMs.size()) {
            return;
        }

        // Keep one short result for the MCM and add it to the final result.
        mcmStats.MCMCount = 1;
        const auto& identity = restoreMCMs[a_mcmIndex].identity;
        UpdateMCMResult(activityMods, ActivityModResult(identity, mcmStats, a_result), &ActivityModResult::restoreStats, stats);

        if (a_result == OperationResult::Completed) {
            HUD::GetSingleton()->ShowRestoreMCM(identity.modName, mcmStats, operationMode);
        }
        mcmStatsRecorded = true;
        logger::info("Recorded '{}' restore result '{}': {} applied, {} unchanged, {} skipped", identity.modName, OperationResultName(a_result), mcmStats.appliedSettingCount, mcmStats.unchangedSettingCount, mcmStats.skippedSettingCount);
        mcmStats.Reset();
    }

    void Restore::HandleExpiredCall()
    {
        callWatch.TracePending();
        callWatch.Abandon();
        ++scheduledTaskID;
        pendingActionIndex = static_cast<size_t>(-1);
        mcmOpen = false;
        if (status == OperationStatus::Stopping) {
            FinishCancellation();
            return;
        }
        if (!mcmStarted || activeMCMIndex >= restoreMCMs.size()) {
            FinishCancellation(OperationResult::Failed);
            return;
        }

        mcmFailed = true;
        requestFailed = false;
        const size_t closeIndex = FindMCMClose();
        currentActionIndex = std::min(closeIndex + 1, actions.size());
        logger::error("Persistent profile restore is abandoning '{}' and continuing with the remaining MCMs", restoreMCMs[activeMCMIndex].identity.modID);
        CompleteMCM();
        QueueNextAction(0.0F);
    }

    size_t Restore::FindMCMClose() const
    {
        size_t index = firstActionIndex;
        while (index < actions.size() && actions[index].type != RestoreActionType::CloseConfig) {
            ++index;
        }
        return index;
    }

    void Restore::FailMCM()
    {
        mcmFailed = true;
        requestFailed = false;
        const size_t closeIndex = FindMCMClose();
        // Never run more settings on a page whose preparation failed.
        currentActionIndex = closeIndex;
        if (!mcmOpen) {
            currentActionIndex = std::min(closeIndex + 1, actions.size());
            CompleteMCM();
        }
    }

    void Restore::CompleteMCM()
    {
        auto& mcm = restoreMCMs[activeMCMIndex];
        const size_t closeIndex = FindMCMClose();
        const bool canRetry = !MCMCallWatch::IsUnavailable(mcm.identity.modID);
        if (mcmFailed && canRetry && !mcm.retryQueued && closeIndex < actions.size()) {
            // Append once, after the remaining MCMs. Finished settings keep their flag.
            std::vector<RestoreAction> retry(actions.begin() + firstActionIndex, actions.begin() + closeIndex + 1);
            actions.insert(actions.end(), retry.begin(), retry.end());
            mcm.retryQueued = true;
            logger::warn("Profile restore queued '{}' for one final attempt after the remaining MCMs", mcm.identity.modID);
        }
        else if (mcmFailed) {
            for (size_t index = firstActionIndex; index < closeIndex; ++index) {
                if (IsRestoreApplyAction(actions[index].type) && !actions[index].completed) {
                    ++mcmStats.skippedSettingCount;
                }
            }
        }
        mcm.previousStats = mcmStats;
        FinishMCMStats(activeMCMIndex, mcmFailed ? OperationResult::Failed : OperationResult::Completed);
        mcmStarted = false;
        callWatch.EndRecovery();
    }

    void Restore::CloseJournalMenu()
    {
        if (!RequestJournalMenuClose()) {
            logger::error("Persistent profile restore could not close the Journal Menu");
            return;
        }

        HUD::GetSingleton()->ShowMenuWarning("HUD.Warning.Restore.Detail");
        logger::warn("Journal Menu opened during profile restore and was closed");
    }

    void Restore::RunNextAction(uint64_t a_loadedGameSession, uint64_t a_taskID)
    {
        std::lock_guard lock(restoreMutex);
        // Ignore callbacks from an older loaded game or an action that has already been replaced.
        if (a_loadedGameSession != loadedGameSession || a_taskID != scheduledTaskID || status == OperationStatus::Idle) {
            return;
        }
        const auto callStatus = callWatch.Check();
        if (callStatus == MCMCallStatus::Waiting) {
            return;
        }
        if (callStatus == MCMCallStatus::Expired) {
            HandleExpiredCall();
            return;
        }
        ++scheduledTaskID;
        if (callStatus == MCMCallStatus::Completed) {
            const bool closing = callWatch.IsClosing();
            const bool timedOut = callWatch.TimedOut();
            const bool confirmationDeclined = callWatch.ConfirmationDeclined();
            callWatch.Consume();
            if (confirmationDeclined && activeMCMIndex < restoreMCMs.size()) {
                restoreMCMs[activeMCMIndex].confirmationRequired = true;
            }
            if (pendingActionIndex < actions.size()) {
                auto& completedAction = actions[pendingActionIndex];
                if (IsRestoreApplyAction(completedAction.type)) {
                    if (confirmationDeclined) {
                        ++mcmStats.skippedSettingCount;
                        logger::warn("Restore of '{}' in '{}' needs user confirmation and was skipped", completedAction.optionLabel, restoreMCMs[completedAction.mcmIndex].identity.modID);
                    }
                    else {
                        ++mcmStats.appliedSettingCount;
                    }
                    completedAction.completed = true;
                }
                else if (completedAction.type == RestoreActionType::NotifySettingChanged) {
                    completedAction.completed = true;
                }
            }
            pendingActionIndex = static_cast<size_t>(-1);
            if (closing) {
                mcmOpen = false;
            }
            if (status != OperationStatus::Stopping) {
                if (timedOut || confirmationDeclined) {
                    mcmFailed = true;
                }
                if (closing) {
                    CompleteMCM();
                }
                else if (timedOut || confirmationDeclined) {
                    FailMCM();
                }
            }
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

        auto& action = actions[currentActionIndex];
        if (action.type == RestoreActionType::OpenConfig) {
            // Each MCM gets its own applied, unchanged and skipped counts.
            mcmStats = restoreMCMs[action.mcmIndex].previousStats;
            firstActionIndex = currentActionIndex;
            mcmFailed = restoreMCMs[action.mcmIndex].confirmationRequired;
            activeMCMIndex = action.mcmIndex;
            mcmStarted = true;
            mcmStatsRecorded = false;
        }

        // Some settings need one call to prepare their data and another call to apply the value.
        const bool applyAction = IsRestoreApplyAction(action.type);
        const bool requestAction = action.controlType != ControlType::Unknown && !applyAction;
        const bool directRequestUnneeded = requestAction && currentActionIndex + 1 < actions.size() && (actions[currentActionIndex + 1].completed || (action.controlType != ControlType::Menu && !IsActionNeeded(actions[currentActionIndex + 1])));
        bool runAction = true;
        if (action.completed || directRequestUnneeded) {
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
            pendingActionIndex = currentActionIndex;
            dispatched = RunAction(action, RestoreTask{ loadedGameSession, continuationTaskID });
            if (!dispatched) {
                pendingActionIndex = static_cast<size_t>(-1);
                logger::error("Profile restore action {} ({}) failed; this MCM needs recovery", currentActionIndex, RestoreActionFunctionName(action.type));
                if (action.type == RestoreActionType::CloseConfig) {
                    FinishCancellation(OperationResult::Failed, true);
                    return;
                }
                FailMCM();
                QueueNextAction(0.0F);
                return;
            }
            if (action.type == RestoreActionType::OpenConfig && dispatched) {
                mcmOpen = true;
            }
        }

        if (applyAction) {
            // A failed request only belongs to the apply action immediately after it.
            requestFailed = false;
        }
        if (applyAction && !dispatched) {
            action.completed = true;
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
            const uint64_t continuationTaskID = ++scheduledTaskID;
            pendingActionIndex = static_cast<size_t>(-1);
            if (CallMCMFunction(activeMCMIndex, "CloseConfig", RE::MakeFunctionArguments(), RestoreTask{ loadedGameSession, continuationTaskID })) {
                logger::info("Persistent profile restore is closing '{}' before cancellation", restoreMCMs[activeMCMIndex].identity.modID);
                return;
            }
            logger::warn("Persistent profile restore could not close '{}' during cancellation", restoreMCMs[activeMCMIndex].identity.modID);
            FinishCancellation(OperationResult::Cancelled, true);
            return;
        }

        FinishCancellation();
    }

    void Restore::FinishCancellation(OperationResult a_result, bool a_unsafe)
    {
        // Values already applied remain changed, so record the interrupted result.
        if (mcmStarted && !mcmStatsRecorded) {
            FinishMCMStats(activeMCMIndex, a_result);
        }

        ++scheduledTaskID;
        restoring = false;
        registryCheckQueued = false;
        status = OperationStatus::Idle;
        callWatch.Release(a_unsafe);
        mcmOpen = false;
        HUD::GetSingleton()->HideMenuWarning();
        Activity::GetSingleton()->RecordRestore(operationMode, stats, activityMods, a_result);
        if (a_unsafe || a_result == OperationResult::Failed) {
            HUD::GetSingleton()->ShowFailure("HUD.Failure.RestoreStopped", a_unsafe ? "HUD.Failure.ScriptBusy" : "HUD.Failure.RestoreIncomplete");
        }
        else {
            HUD::GetSingleton()->ShowRestoreCancelled(stats);
        }
        logger::info("Persistent profile restoration ended ({}): {} settings changed in {} MCMs", OperationResultName(a_result), stats.appliedSettingCount, stats.MCMCount);
    }
}
