#include "backup.hpp"
#include "capture.hpp"
#include "helper.hpp"
#include "hud.hpp"

namespace MCMMemory
{
    inline constexpr uint32_t maximumScriptWaitChecks{ 20 };

    bool Backup::Install()
    {
        std::lock_guard lock(backupMutex);
        if (installed) {
            return true;
        }

        auto* source = SKSE::GetModCallbackEventSource();
        if (!source) {
            logger::error("Full MCM backup could not find the SKSE mod callback event source");
            return false;
        }

        source->AddEventSink(this);
        installed = true;
        logger::info("Full MCM backup event installed");
        return true;
    }

    bool Backup::Begin(BackupRequestType a_requestType)
    {
        std::lock_guard lock(backupMutex);
        if (running) {
            logger::warn("Full MCM backup is already running");
            return false;
        }

        Profile existingProfile;
        std::error_code error;
        const bool profileExists = std::filesystem::exists(ProfileStorage::Path(), error);
        if (error || (profileExists && !ProfileStorage::Load(existingProfile))) {
            logger::error("Full MCM backup refuses to replace an unreadable profile at {}", ToUTF8(ProfileStorage::Path()));
            HUD::GetSingleton()->ShowFailure("Backup failed", "Existing profile could not be read");
            return false;
        }

        Clear();
        requestType = a_requestType;
        profile = std::move(existingProfile);
        running = true;
        logger::info("Full MCM backup is waiting for a stable registry");
        QueueNext(0.0F);
        if (running && requestType == BackupRequestType::Manual) {
            HUD::GetSingleton()->ShowBackupStarted();
        }
        return running;
    }

    void Backup::Clear()
    {
        registeredMCMs.clear();
        registryWait.Reset();
        pages.clear();
        pageSettings.clear();
        mcmSettings.clear();
        menuSettings.clear();
        profile.clear();
        mcmIndex = 0;
        pageIndex = 0;
        menuIndex = 0;
        stats.Reset();
        mcmStats.Reset();
        scriptWaitCount = 0;
        scheduledTaskID = 0;
        step = BackupStep::Registry;
        requestType = BackupRequestType::Manual;
        mcmFailed = false;
        running = false;
    }

    void Backup::Reset()
    {
        std::lock_guard lock(backupMutex);
        ++loadedGameSession;
        Clear();
        initialBackupChecked = false;
        logger::info("Full MCM backup reset");
    }

    RE::BSEventNotifyControl Backup::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
    {
        if (!a_event || a_event->eventName != "SKICP_configManagerReady" || !GetSettings().autoBackup) {
            return RE::BSEventNotifyControl::kContinue;
        }

        bool shouldCheckProfile{};
        { // Trap the lock inside.
            std::lock_guard lock(backupMutex);
            if (!initialBackupChecked) {
                initialBackupChecked = true;
                shouldCheckProfile = true;
            }
        }

        if (!shouldCheckProfile) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::error_code error;
        const bool profileExists = std::filesystem::exists(ProfileStorage::Path(), error);
        if (error) {
            logger::error("Full MCM backup could not check profile {}: {}", ToUTF8(ProfileStorage::Path()), error.message());
            std::lock_guard lock(backupMutex);
            initialBackupChecked = false;
        }
        else if (!profileExists && !Begin(BackupRequestType::Automatic)) {
            std::lock_guard lock(backupMutex);
            initialBackupChecked = false;
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void Backup::RunNextStep(uint64_t a_loadedGameSession, uint64_t a_taskID)
    {
        std::lock_guard lock(backupMutex);
        if (!running || a_loadedGameSession != loadedGameSession || a_taskID != scheduledTaskID) {
            return;
        }

        switch (step) {
        case BackupStep::Registry:
            CheckRegistry();
            break;
        case BackupStep::OpenMCM:
            OpenMCM();
            break;
        case BackupStep::ReadPages:
            ReadPages();
            break;
        case BackupStep::SetPage:
            SetPage();
            break;
        case BackupStep::ReadPage:
            ReadPage();
            break;
        case BackupStep::RequestMenu:
            RequestMenu();
            break;
        case BackupStep::ReadMenu:
            ReadMenu();
            break;
        case BackupStep::CloseMCM:
            CloseMCM();
            break;
        case BackupStep::CommitMCM:
            CommitMCM();
            break;
        case BackupStep::Finish:
            Finish();
            break;
        }
    }

    bool Backup::CallAndContinue(const MCMScript& a_script, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, BackupStep a_nextStep)
    {
        const uint64_t taskID = ++scheduledTaskID;
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new MCMCallResult(BackupTask{ loadedGameSession, taskID }));
        step = a_nextStep;
        if (!a_script.Call(a_functionName, a_arguments, std::move(result))) {
            return false;
        }

        return true;
    }

    void Backup::CheckRegistry()
    {
        auto currentMCMs = MCMRegistry().ReadRegisteredMCMs();
        const auto result = registryWait.Update(currentMCMs);
        if (result == RegistryWaitResult::Ready) {
            registeredMCMs = std::move(currentMCMs);
            step = BackupStep::OpenMCM;
            logger::info("Full MCM backup found {} stable registered MCMs", registeredMCMs.size());
            if (requestType == BackupRequestType::Automatic) {
                HUD::GetSingleton()->ShowBackupStarted();
            }
            QueueNext(0.0F);
            return;
        }
        if (result == RegistryWaitResult::Expired) {
            logger::error("Full MCM backup stopped because the registry did not become stable");
            HUD::GetSingleton()->ShowFailure("Backup failed", "MCM registration did not finish");
            running = false;
            return;
        }
        if (result == RegistryWaitResult::Empty) {
            logger::info("Full MCM backup is waiting for MCM registry entries (check {})", registryWait.checkCount);
        }
        else if (result == RegistryWaitResult::Changed) {
            logger::info("Full MCM backup caught {} registered MCMs and is waiting for registration to settle", currentMCMs.size());
        }
        else {
            logger::info("Full MCM backup registry is unchanged (quiet check {} of {})", registryWait.quietCheckCount, requiredStableRegistryChecks);
        }

        QueueNext(registryCheckDelaySeconds);
    }

    void Backup::OpenMCM()
    {
        if (mcmIndex >= registeredMCMs.size()) {
            step = BackupStep::Finish;
            QueueNext(0.0F);
            return;
        }

        pages.clear();
        pageSettings.clear();
        mcmSettings.clear();
        menuSettings.clear();
        mcmStats.Reset();
        pageIndex = 0;
        menuIndex = 0;
        scriptWaitCount = 0;
        mcmFailed = false;

        MCMScript script(registeredMCMs[mcmIndex].mcmScript);
        if (!CallAndContinue(script, "OpenConfig", RE::MakeFunctionArguments(), BackupStep::ReadPages)) {
            logger::error("Full MCM backup could not open '{}'", registeredMCMs[mcmIndex].identity.modID);
            mcmFailed = true;
            step = BackupStep::CommitMCM;
            QueueNext(0.0F);
            return;
        }
    }

    void Backup::ReadPages()
    {
        MCMScript script(registeredMCMs[mcmIndex].mcmScript);
        if (!script.IsConfigOpen()) {
            if (++scriptWaitCount < maximumScriptWaitChecks) {
                QueueNext(GetSettings().actionTrialDelaySeconds);
            }
            else {
                logger::error("Full MCM backup timed out while opening '{}'", registeredMCMs[mcmIndex].identity.modID);
                mcmFailed = true;
                step = BackupStep::CommitMCM;
                QueueNext(0.0F);
            }
            return;
        }

        pages.push_back(BackupPage{ std::string{}, -1 });
        auto pageNames = script.ReadPages();
        pages.reserve(pageNames.size() + 1);
        for (size_t index = 0; index < pageNames.size(); ++index) {
            pages.push_back(BackupPage{ std::move(pageNames[index]), static_cast<int>(index) });
        }

        scriptWaitCount = 0;
        step = BackupStep::ReadPage;
        QueueNext(0.0F);
    }

    void Backup::SetPage()
    {
        if (pageIndex >= pages.size()) {
            step = BackupStep::CloseMCM;
            QueueNext(0.0F);
            return;
        }

        const auto& page = pages[pageIndex];
        MCMScript script(registeredMCMs[mcmIndex].mcmScript);
        if (!CallAndContinue(script, "SetPage", RE::MakeFunctionArguments(std::string{ page.name }, int{ page.index }), BackupStep::ReadPage)) {
            logger::error("Full MCM backup could not read page '{}' from '{}'", page.name, registeredMCMs[mcmIndex].identity.modID);
            mcmFailed = true;
            step = BackupStep::CloseMCM;
            QueueNext(0.0F);
            return;
        }

        scriptWaitCount = 0;
    }

    void Backup::ReadPage()
    {
        const auto& page = pages[pageIndex];
        pageSettings.clear();
        menuSettings.clear();
        menuIndex = 0;

        MCMScript script(registeredMCMs[mcmIndex].mcmScript);
        if (!script.IsPageReady(page.index)) {
            if (++scriptWaitCount < maximumScriptWaitChecks) {
                QueueNext(GetSettings().actionTrialDelaySeconds);
            }
            else {
                logger::error("Full MCM backup timed out while reading page '{}' from '{}'", page.name, registeredMCMs[mcmIndex].identity.modID);
                mcmFailed = true;
                step = BackupStep::CloseMCM;
                QueueNext(0.0F);
            }
            return;
        }

        if (!script.ReadPage(registeredMCMs[mcmIndex].identity, page.name, page.index, pageSettings)) {
            logger::error("Full MCM backup could not read the option buffers for '{}' page '{}'", registeredMCMs[mcmIndex].identity.modID, page.name);
            mcmFailed = true;
            step = BackupStep::CloseMCM;
            QueueNext(0.0F);
            return;
        }

        for (size_t index = 0; index < pageSettings.size(); ++index) {
            if (pageSettings[index].type == ControlType::Menu) {
                menuSettings.push_back(index);
            }
        }

        if (menuSettings.empty()) {
            SaveCurrentPage();
            AdvancePage();
            return;
        }

        step = BackupStep::RequestMenu;
        QueueNext(0.0F);
    }

    void Backup::RequestMenu()
    {
        if (menuIndex >= menuSettings.size()) {
            SaveCurrentPage();
            AdvancePage();
            return;
        }

        auto& setting = pageSettings[menuSettings[menuIndex]];
        MCMScript script(registeredMCMs[mcmIndex].mcmScript);
        if (!CallAndContinue(script, "RequestMenuDialogData", RE::MakeFunctionArguments(int{ setting.selection.optionIndex }), BackupStep::ReadMenu)) {
            setting.identityComplete = false;
            ++menuIndex;
            QueueNext(0.0F);
            return;
        }

        scriptWaitCount = 0;
    }

    void Backup::ReadMenu()
    {
        auto& setting = pageSettings[menuSettings[menuIndex]];
        MCMScript script(registeredMCMs[mcmIndex].mcmScript);
        if (!script.IsMenuReady(setting.selection.optionIndex)) {
            if (++scriptWaitCount < maximumScriptWaitChecks) {
                QueueNext(GetSettings().actionTrialDelaySeconds);
                return;
            }
            setting.identityComplete = false;
            logger::warn("Full MCM backup timed out while reading menu '{}' in '{}'", setting.optionLabel, setting.selection.identity.modID);
            ++menuIndex;
            step = BackupStep::RequestMenu;
            QueueNext(0.0F);
            return;
        }

        auto selectedIndex = script.ReadMenuIndex();
        if (selectedIndex && *selectedIndex >= 0) {
            setting.value = *selectedIndex;
            setting.valueSource = "script._menuParams";
        }
        else {
            setting.identityComplete = false;
            logger::warn("Full MCM backup skipped menu '{}' on page '{}' in '{}' because its selected index was unavailable", setting.optionLabel, setting.selection.pageName, setting.selection.identity.modID);
        }

        ++menuIndex;
        step = BackupStep::RequestMenu;
        QueueNext(0.0F);
    }

    void Backup::SaveCurrentPage()
    {
        for (auto& setting : pageSettings) {
            if (setting.identityComplete && !setting.value.is_null()) {
                Deduplicate(mcmSettings, std::move(setting));
            }
            else {
                ++mcmStats.skippedSettingCount;
            }
        }
    }

    void Backup::AdvancePage()
    {
        ++pageIndex;
        scriptWaitCount = 0;
        step = pageIndex < pages.size() ? BackupStep::SetPage : BackupStep::CloseMCM;
        QueueNext(0.0F);
    }

    void Backup::CloseMCM()
    {
        MCMScript script(registeredMCMs[mcmIndex].mcmScript);
        if (!CallAndContinue(script, "CloseConfig", RE::MakeFunctionArguments(), BackupStep::CommitMCM)) {
            mcmFailed = true;
            logger::error("Full MCM backup could not close '{}'", registeredMCMs[mcmIndex].identity.modID);
            step = BackupStep::CommitMCM;
            QueueNext(0.0F);
        }
    }

    void Backup::CommitMCM()
    {
        const auto& modID = registeredMCMs[mcmIndex].identity.modID;
        if (!mcmFailed) {
            auto setting = profile.begin();
            while (setting != profile.end()) {
                if (setting->selection.identity.modID == modID) {
                    setting = profile.erase(setting);
                }
                else {
                    ++setting;
                }
            }
            for (auto& backedUpSetting : mcmSettings) {
                profile.push_back(std::move(backedUpSetting));
            }
            mcmStats.MCMCount = 1;
            mcmStats.settingCount = static_cast<uint32_t>(mcmSettings.size());
            logger::info("Full MCM backup captured {} settings from '{}' ({} skipped)", mcmStats.settingCount, modID, mcmStats.skippedSettingCount);
        }
        else {
            mcmStats.failedMCMCount = 1;
            logger::warn("Full MCM backup kept the previous settings for '{}' after the MCM failed", modID);
        }

        HUD::GetSingleton()->ShowBackupMCM(registeredMCMs[mcmIndex].identity.modName, mcmStats);
        stats += mcmStats;

        ++mcmIndex;
        step = BackupStep::OpenMCM;
        QueueNext(0.0F);
    }

    void Backup::Finish()
    {
        Capture::GetSingleton()->MergeSettings(profile);
        if (!ProfileStorage::Save(profile)) {
            logger::error("Full MCM backup failed to save its completed profile");
            HUD::GetSingleton()->ShowFailure("Backup failed", "Existing profile was not changed");
            running = false;
            return;
        }

        logger::info("Full MCM backup completed: {} settings from {} MCMs, {} skipped, {} MCMs failed", stats.settingCount, stats.MCMCount, stats.skippedSettingCount, stats.failedMCMCount);
        HUD::GetSingleton()->ShowBackupSummary(stats);
        running = false;
    }
}
