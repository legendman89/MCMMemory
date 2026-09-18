#pragma once

#include "utils/time.hpp"
#include "mcm/mcm_menu.hpp"
#include "mcm/mcm_support.hpp"
#include "mcm/mcm_registry.hpp"
#include "profile/types.hpp"
#include "profile/stats.hpp"
#include "profile/profile.hpp"
#include "profile/storage.hpp"
#include "utils/scheduler.hpp"

#include "settings.hpp"

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

    struct ProfileSaveTask
    {
        uint64_t taskID{};

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

        // Saves the current changes before switching profiles and cancels older capture tasks.
        bool PrepareProfileChange();

        // Removes a profile from memory.
        void ForgetProfile(std::string_view a_name);

        // Keeps captured settings hidden from this MCM's scan, without replacing fresh reads.
        void MergeSettings(std::vector<CapturedSetting>& a_settings, std::string_view a_modID);

        // Returns an activation choice detected during this loaded game.
        std::optional<MCMActivationState> FindDetectedActivation(std::string_view a_modID);

        // True when a config close separates this change from the last one written for the MCM.
        bool IsConfigReopened(const std::string& a_profileName, const std::string& a_modID, uint32_t a_configSession) const;

        // Forget what this game still remembers about MCMs whose saved settings were removed.
        void ForgetMCMs(const MCMFilter& a_modIDs);

        // Receives MCM callbacks such as sliderAccepted and optionSelected.
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;

        // Receives Journal Menu open/close events.
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    private:

        friend struct ReadCaptureTask;

        friend struct FinishCaptureTask;

        friend struct ProfileSaveTask;

        // Leaves the current Scaleform callback before reading the menu.
        inline void QueueMenuRead(CaptureRequest a_request)
        {
            const bool queued = Scheduler::GetSingleton()->ScheduleUIAfterFrames(ReadCaptureTask{ a_request }, 0);
            if (auto* record = FindRecord(a_request.eventID)) {
                record->capturePending = queued && a_request.persist;
            }
            if (!queued) {
                logger::error("Captured MCM event {} could not reach the UI task queue", a_request.eventID);
            }
        }

        // Schedules the second menu read after SkyUI updates the control.
        inline void QueueCaptureCompletion(CaptureRequest a_request, uint32_t a_delayFrames)
        {
            const bool queued = Scheduler::GetSingleton()->ScheduleUIAfterFrames(FinishCaptureTask{ a_request }, a_delayFrames);
            if (auto* record = FindRecord(a_request.eventID)) {
                record->capturePending = queued && a_request.persist;
            }
            if (!queued) {
                logger::error("Captured MCM event {} could not schedule its completion", a_request.eventID);
            }
        }

        // Checks the loaded game session and Journal Menu visit before accessing Scaleform.
        inline bool IsCurrentRequest(const CaptureRequest& a_request) const
        {
            return journalMenuOpen && a_request.loadedGameSession == loadedGameSession && a_request.eventID > menuOpenedEventID;
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

        inline void CancelProfileSaveTask()
        {
            ++profileSaveTaskID;
            profileSaveTaskQueued = false;
        }

        // Saves the current changes to the active profile and clears the pending list.
        bool SaveProfileChanges();

        // Extends one queued save until settings have stopped changing.
        void DelayProfileSave();

        bool QueueProfileSave(float a_delaySeconds);

        void RunProfileSave(uint64_t a_taskID);

        // Takes the first safe menu read after the callback returns.
        void ReadMenu(const CaptureRequest& a_request);

        // Takes the second menu read and finishes one capture.
        void CompleteCapture(CaptureRequest a_request);

        // Queues one result per changed MCM after its profile is saved.
        void ShowAutoBackupResults();

        // Returns false when a toggle still needs another read before saving.
        bool ProcessCapturedEvent(CaptureRecord& a_record);

        // Saves a staged MCM enable choice separately from its normal settings.
        bool CaptureMCMActivation(CaptureRecord& a_record, const MCMScript& a_script);

        void RememberActivation(const MCMActivationState& a_activation);

        // Keeps the highlighted control or opened dropdown label and state before a redraw.
        // Needed for mods that clear a page to hide disabled controls.
        void RememberControl(CaptureRecord& a_record);

        // Stops old reads after navigation or a newer setting change.
        bool IsCapturePageCurrent(const CaptureRecord& a_record) const;

        bool ReadSelectedSetting(CaptureRecord& a_record, const MCMScript& a_script, CapturedSetting& a_setting) const;

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
        void UpdateSelectionFromMenu(const nlohmann::json& a_state);

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

        // Remembers staged MCM choices until a manual backup, even when automatic backup is off.
        std::vector<MCMActivationState> detectedActivations;

        // Captured settings awaiting a successful save notification.
        std::vector<CapturedSetting> pendingAutoBackupSettings;

        // Last recorded config session per profile and MCM. Reset on game load.
        std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> recordedConfigSessions;

        TimePoint profileSaveAt{};

        // Gives each new callback its eventID.
        uint64_t eventCount{};

        // Changes whenever a new game starts or another save is loaded.
        uint64_t loadedGameSession{};

        // Old reads must not run against a newly opened Journal Menu.
        uint64_t menuOpenedEventID{};

        // Invalidates older inactivity tasks after a save or session change.
        uint64_t profileSaveTaskID{};

        // Counts how many times an MCM config was opened. Settings recorded under different
        // counts are separated by an OnConfigClose that the restore has to replay.
        uint32_t configSession{ 1 };

        // Prevents the event listeners from being installed twice.
        bool installed{};

        // Rejects delayed reads after the Journal Menu closes.
        bool journalMenuOpen{};

        // Keeps inactivity saves on one queued task.
        bool profileSaveTaskQueued{};

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

    // A text row that shows its own value can be replayed by clicking it, but a command button
    // like Save or Reset must never become a setting.
    inline bool IsRecordableTextSetting(const CaptureRecord& a_record)
    {
        if (!GetSettings().recordActions || !a_record.control || !IsRecordableTextControl(*a_record.control)) {
            return false;
        }
        const auto& selection = a_record.selection;
        const auto& control = *a_record.control;
        return !MCMCommandSupport::IsIgnored(selection.identity.modID, selection.pageName, selection.pageIndex, control.type, control.stateName, control.optionLabel);
    }
}
