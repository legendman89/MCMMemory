#pragma once

#include "mcm/mcm_menu.hpp"
#include "mcm/mcm_registry.hpp"
#include "profile/profile.hpp"
#include "profile/stats.hpp"
#include "profile/storage.hpp"
#include "settings.hpp"
#include "utils/scheduler.hpp"

namespace MCMMemory
{
    class MCMScript;

    struct CaptureRequest
    {
        // Identifies the event being processed.
        uint64_t eventID{};

        // Identifies the loaded game in which the event happened.
        uint64_t loadedGameSession{};

        // Bounds the extra reads while a toggle handler or page reset is running.
        uint32_t readAttempts{};

        // Saves the capture after a modified setting event finishes.
        bool persist{};

        CaptureRequest(uint64_t a_eventID, uint64_t a_loadedGameSession, bool a_persist) : eventID(a_eventID), loadedGameSession(a_loadedGameSession), persist(a_persist) {}
    };

    struct ReadCaptureTask
    {
        // Carries the event into the game task queue.
        CaptureRequest request;

        explicit ReadCaptureTask(CaptureRequest a_request) : request(a_request) {}

        void operator()() const;
    };

    struct FinishCaptureTask
    {
        // Carries the same event into its delayed second read.
        CaptureRequest request;

        explicit FinishCaptureTask(CaptureRequest a_request) : request(a_request) {}

        void operator()() const;
    };

    struct AutoBackupResult
    {
        MCMIdentity identity;

        BackupStats stats;
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

        // Keeps captured settings hidden from this MCM's scan, without replacing fresh reads.
        void MergeSettings(Profile& a_profile, std::string_view a_modID);

        // Receives MCM callbacks such as sliderAccepted and optionSelected.
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;

        // Receives Journal Menu open/close events.
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    private:

        friend struct ReadCaptureTask;

        friend struct FinishCaptureTask;

        // Leaves the current Scaleform callback before reading the menu.
        inline void QueueMenuRead(CaptureRequest a_request)
        {
            if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(ReadCaptureTask{ a_request }, 0.0F)) {
                logger::error("Captured MCM event {} could not reach the game task queue", a_request.eventID);
            }
        }

        // Schedules the second menu read after SkyUI updates the control.
        inline void QueueCaptureCompletion(CaptureRequest a_request, uint32_t a_delayFrames)
        {
            if (!Scheduler::GetSingleton()->ScheduleAfterFrames(FinishCaptureTask{ a_request }, a_delayFrames)) {
                logger::error("Captured MCM event {} could not schedule its completion", a_request.eventID);
            }
        }

        // Checks the loaded game session before accessing the menu.
        inline bool IsCurrentRequest(const CaptureRequest& a_request) const
        {
            return a_request.loadedGameSession == loadedGameSession && a_request.eventID > menuOpenedEventID;
        }

        inline void ReadMenuIfCurrentSession(const CaptureRequest& a_request)
        {
            std::scoped_lock lock(captureMutex);
            if (IsCurrentRequest(a_request)) {
                ReadMenu(a_request);
            }
        }

        inline void CompleteCaptureIfCurrentSession(const CaptureRequest& a_request)
        {
            std::scoped_lock lock(captureMutex);
            if (IsCurrentRequest(a_request)) {
                CompleteCapture(a_request);
            }
        }

        inline CaptureRecord* FindRecord(uint64_t a_eventID)
        {
            auto record = records.rbegin();
            for (; record != records.rend() && record->eventID != a_eventID; ++record) {}
            return record != records.rend() ? std::addressof(*record) : nullptr;
        }

        // Takes the first safe menu read after the callback returns.
        void ReadMenu(const CaptureRequest& a_request);

        // Takes the second menu read and finishes one capture.
        void CompleteCapture(CaptureRequest a_request);

        // Queues one result per changed MCM when the Journal Menu closes.
        void ShowAutoBackupResults();

        // Returns false when a toggle still needs another read before saving.
        bool ProcessCapturedEvent(CaptureRecord& a_record);

        // Keeps the highlighted toggle label and state before its handler redraws the page.
        // Needed for mods that clear a page to hide disabled controls.
        void RememberControl(CaptureRecord& a_record);

        // Stops old reads after navigation or a newer setting change.
        bool IsCapturePageCurrent(const CaptureRecord& a_record) const;

        bool ReadToggleSetting(CaptureRecord& a_record, const MCMScript& a_script, CapturedSetting& a_setting) const;

        // Reads the option label from the current menu state or an earlier read of the same row.
        std::string ReadOptionLabel(const CaptureRecord& a_record) const;

        // Reads an option label from one menu state when the cursor still points to that option.
        std::string ReadOptionLabel(const nlohmann::json& a_state, int a_optionIndex, bool a_dialogControl) const;

        // Propagates the stable ID into recent events for the same MCM that were recorded before the ID became available.
        void SyncMCMIdentity();

        // Returns false while an opening page is still being rebuilt.
        bool SyncOpeningPage(CaptureRecord& a_record);

        // Gets the stable MCM name and ID from the active config script.
        // Ex. script type: TrueHUD_MCM, mod name: TrueHUD, then modID is TrueHUD_MCM::TrueHUD.
        void FindActiveMCMIdentity(EventType a_type, const nlohmann::json& a_state);

        // Updates the current mod, page or option from a callback.
        void UpdateSelectionFromEvent(EventType a_type, const SKSE::ModCallbackEvent& a_event);

        // Fills missing selection names from the visible menu.
        void UpdateSelectionFromMenu(EventType a_type, const nlohmann::json& a_state);

        // Saves the callback as a raw record before its menu reads are queued.
        uint64_t RecordEvent(EventType a_type, const SKSE::ModCallbackEvent& a_event);

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

        // Holds settings automatically saved during the current Journal Menu visit.
        std::vector<CapturedSetting> pendingAutoBackupSettings;

        // Gives each new callback its eventID.
        uint64_t eventCount{};

        // Changes whenever a new game starts or another save is loaded.
        uint64_t loadedGameSession{};

        // Old reads must not run against a newly opened Journal Menu.
        uint64_t menuOpenedEventID{};

        // Prevents the event listeners from being installed twice.
        bool installed{};

    };

    inline void ReadCaptureTask::operator()() const
    {
        Capture::GetSingleton()->ReadMenuIfCurrentSession(request);
    }

    inline void FinishCaptureTask::operator()() const
    {
        // Read the menu again after SkyUI has finished updating it.
        Capture::GetSingleton()->CompleteCaptureIfCurrentSession(request);
    }
}
