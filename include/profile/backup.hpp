#pragma once

#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_script.hpp"
#include "profile/profile.hpp"
#include "profile/stats.hpp"
#include "settings.hpp"
#include "utils/scheduler.hpp"

namespace MCMMemory
{
    enum class BackupStep
    {
        Registry,
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

    class Backup : public RE::BSTEventSink<SKSE::ModCallbackEvent>
    {
    public:

        static Backup* GetSingleton()
        {
            static Backup singleton;
            return std::addressof(singleton);
        }

        inline bool Start()
        {
            return Begin(OperationMode::Manual);
        }

        inline bool IsRunning()
        {
            std::lock_guard lock(backupMutex);
            return running;
        }

        bool Install();

        void Reset();

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;

    private:

        friend struct BackupTask;

        inline void QueueNext(float a_delaySeconds)
        {
            const uint64_t taskID = ++scheduledTaskID;
            if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(BackupTask{ loadedGameSession, taskID }, a_delaySeconds)) {
                logger::error("Full MCM backup could not schedule its next step");
                running = false;
            }
        }

        bool Begin(OperationMode a_operationMode);

        void RunNextStep(uint64_t a_loadedGameSession, uint64_t a_taskID);

        void CheckRegistry();

        void OpenMCM();

        void ReadPages();

        void SetPage();

        void ReadPage();

        void RequestMenu();

        void ReadMenu();

        void CloseMCM();

        void CommitMCM();

        void Finish();

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

        Profile profile;

        RegistryWait registryWait;

        size_t mcmIndex{};

        size_t pageIndex{};

        size_t menuIndex{};

        uint64_t loadedGameSession{};

        uint64_t scheduledTaskID{};

        BackupStats stats;

        BackupStats mcmStats;

        uint32_t scriptWaitCount{};

        BackupStep step{ BackupStep::Registry };

        OperationMode operationMode{ OperationMode::Manual };

        bool installed{};

        bool initialBackupChecked{};

        bool mcmFailed{};

        bool running{};
    };

    inline void BackupTask::operator()() const
    {
        Backup::GetSingleton()->RunNextStep(loadedGameSession, taskID);
    }

}
