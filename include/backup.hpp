#pragma once

#include "mcm_registry.hpp"
#include "mcm_script.hpp"
#include "profile.hpp"
#include "scheduler.hpp"
#include "settings.hpp"
#include "stats.hpp"

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

    enum class BackupRequestType
    {
        Manual,
        Automatic
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
            return Begin(BackupRequestType::Manual);
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

        bool Begin(BackupRequestType a_requestType);

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

        BackupRequestType requestType{ BackupRequestType::Manual };

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
