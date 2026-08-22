#pragma once

#include "scheduler.hpp"
#include "mcm_menu.hpp"
#include "mcm_registry.hpp"
#include "profile.hpp"
#include "settings.hpp"
#include "storage.hpp"

namespace MCMMemory
{
    struct FinishCaptureTask
    {
        // Identifies the captured event that needs a second menu read.
        // Some menu values update later, so the event ID connects the second read to the original event.
        uint64_t eventID{};

        // Identifies the loaded game in which this task was created.
        uint64_t loadedGameSession{};

        bool persist{};

        FinishCaptureTask(uint64_t a_eventID, uint64_t a_loadedGameSession, bool a_persist) : eventID(a_eventID), loadedGameSession(a_loadedGameSession), persist(a_persist) {}

        void operator()() const;
    };

    class Capture final : public RE::BSTEventSink<SKSE::ModCallbackEvent>, public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:

        static Capture* GetSingleton()
        {
            static Capture singleton;
            return std::addressof(singleton);
        }

        // Starts listening for MCM and Journal Menu events.
        bool Install();
        
        // Clears capture data when a game is started or loaded.
        void Reset();

        // Applies settings changed during a full scan before the profile is written.
        void MergeSettings(Profile& a_profile);

        // Receives MCM callbacks such as sliderAccepted and optionSelected.
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;

        // Receives Journal Menu open/close events.
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    private:

        // Friend it to call the private CompleteCaptureIfCurrentSession function.
        friend struct FinishCaptureTask;

        // Schedules the second menu read for one event.
        inline void QueueCaptureCompletion(uint64_t a_eventID, uint32_t a_delayFrames, bool a_persist)
        {
            if (!Scheduler::GetSingleton()->ScheduleAfterFrames(FinishCaptureTask{ a_eventID, loadedGameSession, a_persist }, a_delayFrames)) {
                CompleteCapture(a_eventID, a_persist);
            }
        }

        // Checks the loaded game session before finishing delayed capture work.
        inline void CompleteCaptureIfCurrentSession(uint64_t a_eventID, uint64_t a_loadedGameSession, bool a_persist)
        {
            std::scoped_lock lock(captureMutex);
            if (a_loadedGameSession != loadedGameSession) {
                return;
            }
            CompleteCapture(a_eventID, a_persist);
        }

        // Takes the second menu read and finishes one capture.
        void CompleteCapture(uint64_t a_eventID, bool a_persist);
        
        // Converts a raw record into a setting that can enter the profile.
        void ProcessCapturedEvent(CaptureRecord& a_record);

        // Propagates the stable ID into recent events for the same MCM that were recorded before the ID became available.
        void SyncMCMIdentity();

        // Gets the stable MCM name and ID from the active config script.
        // Ex. script type: TrueHUD_MCM, mod name: TrueHUD, then modID is TrueHUD_MCM::TrueHUD.
        void FindActiveMCMIdentity(EventType a_type, const nlohmann::json& a_state);

        // Updates the current mod, page or option from a callback.
        void UpdateSelectionFromEvent(EventType a_type, const SKSE::ModCallbackEvent& a_event);

        // Fills missing selection names from the visible menu.
        void UpdateSelectionFromMenu(EventType a_type, const nlohmann::json& a_state);
        
        // Saves the callback and first menu read as a raw record.
        uint64_t RecordEvent(EventType a_type, const SKSE::ModCallbackEvent& a_event, nlohmann::json a_state);

        // Stops capture events and delayed tasks from changing data at the same time.
        std::mutex captureMutex;

        // Holds the MCM, page and option currently being used.
        MCMSelection selection;

        // Remembers the name and stable ID found for each MCM menu index.
        std::unordered_map<int, MCMIdentity> mcmIdentities;

        // Holds raw events and their before and after menu reads.
        std::vector<CaptureRecord> records;

        // Holds the latest cleaned settings captured in this game session.
        std::vector<CapturedSetting> settings;

        // Gives each new callback its eventID.
        uint64_t eventCount{};

        // Changes whenever a new game starts or another save is loaded.
        uint64_t loadedGameSession{};

        // Prevents the event listeners from being installed twice.
        bool installed{};

    };

    inline void FinishCaptureTask::operator()() const
    {
        // Read the menu again after SkyUI has finished updating it.
        Capture::GetSingleton()->CompleteCaptureIfCurrentSession(eventID, loadedGameSession, persist);
    }
}
