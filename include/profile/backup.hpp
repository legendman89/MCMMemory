#pragma once

#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_script.hpp"
#include "mcm/mcm_calls.hpp"
#include "mcm/mcm_support.hpp"
#include "profile/activity.hpp"
#include "profile/profile.hpp"
#include "profile/stats.hpp"
#include "settings.hpp"
#include "utils/scheduler.hpp"

namespace MCMMemory
{
    enum class BackupStep
    {
        ReadRegistry,
        OpenMCM,
        ReadPages,
        SetPage,
        ReadPage,
        RequestMenu,
        ReadMenu,
        CloseMCM,
        CommitMCM,
        Finish
    };

    struct BackupTask
    {
        uint64_t loadedGameSession{};

        uint64_t taskID{};

        void operator()() const;
    };

    class Backup : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:

        static Backup* GetSingleton()
        {
            static Backup singleton;
            return std::addressof(singleton);
        }

        inline bool Start()
        {
            return Begin({});
        }

        inline bool StartSelected(const MCMFilter& a_filter)
        {
            return !a_filter.empty() && Begin(a_filter);
        }

        inline OperationStatus GetStatus()
        {
            std::lock_guard lock(backupMutex);
            return status;
        }

        bool Install();

        bool Cancel();

        void Reset();

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    private:

        friend struct BackupTask;

        friend struct MCMWatchTask<Backup>;

        void CheckCalls(uint64_t a_loadedGameSession);

        void HandleExpiredCall();

        bool QueueWatch();

        inline void QueueNext(float a_delaySeconds)
        {
            const uint64_t taskID = ++scheduledTaskID;
            if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(BackupTask{ loadedGameSession, taskID }, a_delaySeconds)) {
                logger::error("Full MCM backup could not schedule its next step");
                FinishCancellation(OperationResult::Failed, callWatch.HasCall() || mcmOpen);
            }
        }

        bool Begin(MCMFilter a_filter);

        bool ReadExistingProfile(Profile& a_profile) const;

        void RunNextStep(uint64_t a_loadedGameSession, uint64_t a_taskID);

        void ReadRegistry();

        void OpenMCM();

        void ReadPages();

        void SetPage();

        void ReadPage();

        void RequestMenu();

        void ReadMenu();

        void CloseMCM();

        void CommitMCM();

        void Finish();

        void ContinueCancellation();

        void FinishCancellation(OperationResult a_result = OperationResult::Cancelled, bool a_unsafe = false);

        void CloseJournalMenu();

        void Clear();

        void AdvancePage();

        void SaveCurrentPage();

        bool CallAndContinue(const MCMScript& a_script, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, BackupStep a_nextStep);

        std::mutex backupMutex;

        std::vector<MCMRegistryEntry> registeredMCMs;

        std::vector<MCMPage> pages;

        std::vector<CapturedSetting> pageSettings;

        std::vector<CapturedSetting> mcmSettings;

        std::vector<size_t> menuSettings;

        std::vector<ActivityModResult> activityMods;

        MCMFilter mcmFilter;

        Profile profile;

        RegistryWait registryWait;

        MCMCallWatch callWatch;

        // For activation detection. I still working out a way to make this more generic.
        std::optional<MCMActivationState> mcmActivation;

        size_t firstPassCount{};

        size_t mcmIndex{};

        size_t pageIndex{};

        size_t menuIndex{};

        uint64_t loadedGameSession{};

        uint64_t scheduledTaskID{};

        BackupStats stats;

        BackupStats mcmStats;

        uint32_t scriptWaitCount{};

        BackupStep step{ BackupStep::ReadRegistry };

        OperationStatus status{ OperationStatus::Idle };

        bool mcmFailed{};

        bool mcmStarted{};

        bool mcmOpen{};

        bool installed{};
    };

    inline void BackupTask::operator()() const
    {
        Backup::GetSingleton()->RunNextStep(loadedGameSession, taskID);
    }

}
