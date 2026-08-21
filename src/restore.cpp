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

    void Restore::Reset()
    {
        std::lock_guard lock(restoreMutex);
        // Cancel old tasks and wait for MCM registration again.
        ++loadedGameSession;
        configLoaded = false;
        configValid = false;
        started = false;
        registryCheckQueued = false;
        currentActionIndex = 0;
        registryCheckCount = 0;
        lastRegistryModIDs.clear();
        restoreMCMs.clear();
        actions.clear();
        logger::info("Persistent profile restore event reset");
    }

    RE::BSEventNotifyControl Restore::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
    {
        if (!a_event || a_event->eventName != "SKICP_configManagerReady") {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::lock_guard lock(restoreMutex);
        if (started || !GetSettings().enabled) {
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
        if (started || !configValid || !GetSettings().enabled) {
            return;
        }

        // Registration arrives in stages, so wait until the list stops changing.
        ++registryCheckCount;
        auto registeredMCMs = MCMRegistry().ReadRegisteredMCMs();
        if (registeredMCMs.empty()) {
            logger::info("Persistent profile restore is waiting for MCM registry entries (check {})", registryCheckCount);
            return;
        }

        std::vector<std::string> registryModIDs;
        registryModIDs.reserve(registeredMCMs.size());
        for (const auto& mcm : registeredMCMs) {
            registryModIDs.push_back(mcm.identity.modID);
        }

        std::sort(registryModIDs.begin(), registryModIDs.end());

        if (registryModIDs != lastRegistryModIDs) {
            lastRegistryModIDs = std::move(registryModIDs);
            logger::info("Persistent profile restore caught {} registered MCMs and is waiting for one stable registration round", registeredMCMs.size());
            return;
        }

        MatchRegisteredMCMs(registeredMCMs);

        StartRestore();
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

    void Restore::RunNextAction(uint64_t a_loadedGameSession)
    {
        std::lock_guard lock(restoreMutex);
        // A session mismatch means another game was loaded after this task was queued.
        if (a_loadedGameSession != loadedGameSession || currentActionIndex >= actions.size()) {
            return;
        }

        const auto& action = actions[currentActionIndex];
        if (!RunAction(action)) {
            logger::error("Profile restore action {} ({}) failed; continuing with the remaining profile", currentActionIndex, RestoreActionFunctionName(action.type));
        }

        // Leave a short delay before the next MCM call.
        ++currentActionIndex;
        if (currentActionIndex >= actions.size()) {
            logger::info("Persistent profile restoration completed all {} queued actions", actions.size());
            return;
        }

        QueueNextAction(GetSettings().actionDelaySeconds);
    }
}
