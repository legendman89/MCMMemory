#pragma once

#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_script.hpp"
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

    struct BackupPage
    {
        std::string name;

        int index{-1};
    };

    struct BackupTask
    {
        uint64_t loadedGameSession{};

        uint64_t taskID{};

        void operator()() const;
    };

    class Backup
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

        bool Cancel();

        void Reset();

    private:

        friend struct BackupTask;

        inline void QueueNext(float a_delaySeconds)
        {
            const uint64_t taskID = ++scheduledTaskID;
            if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(BackupTask{ loadedGameSession, taskID }, a_delaySeconds)) {
                logger::error("Full MCM backup could not schedule its next step");
                status = OperationStatus::Idle;
            }
        }

        bool Begin(MCMFilter a_filter);

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

        void FinishCancellation();

        void Clear();

        void AdvancePage();

        void SaveCurrentPage();

        bool CallAndContinue(const MCMScript& a_script, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, BackupStep a_nextStep);

        std::mutex backupMutex;

        std::vector<MCMRegistryEntry> registeredMCMs;

        std::vector<BackupPage> pages;

        std::vector<CapturedSetting> pageSettings;

        std::vector<CapturedSetting> mcmSettings;

        std::vector<size_t> menuSettings;

        std::vector<ActivityModResult> activityMods;

        MCMFilter mcmFilter;

        Profile profile;

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
    };

    inline void BackupTask::operator()() const
    {
        Backup::GetSingleton()->RunNextStep(loadedGameSession, taskID);
    }

}
