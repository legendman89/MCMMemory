#include "menu/hud.hpp"
#include "profile/activity.hpp"
#include "profile/capture.hpp"
#include "mcm/mcm_calls.hpp"

namespace MCMMemory
{

    // Some values may become readable a few frames after the callback.
    constexpr int QueueDelayFrames = 2;
    // Check busy toggles and opening pages every 0.1 seconds, for about five seconds.
    inline constexpr uint32_t captureReadDelayFrames = 6;
    inline constexpr uint32_t maximumCaptureReads = 50;
    // Recorded commands can wait for the player to answer a confirmation dialog.
    inline constexpr uint32_t maximumCommandCaptureReads = 600;
    // Raw records are only for debugging, so keep their memory use bounded.
    constexpr size_t maximumRecords = 4096;

    bool Capture::Install()
    {
        if (installed) {
            return true;
        }

        auto modEvents = SKSE::GetModCallbackEventSource();

        auto ui = RE::UI::GetSingleton();

        if (!modEvents || !ui) {
            logger::error("Capture could not acquire its event sources");
            return false;
        }

        // MCM callbacks tell us what the player changed.
        modEvents->AddEventSink(static_cast<RE::BSTEventSink<SKSE::ModCallbackEvent>*>(this));

        // The menu event lets us save the final capture when the journal closes.
        ui->AddEventSink<RE::MenuOpenCloseEvent>(static_cast<RE::BSTEventSink<RE::MenuOpenCloseEvent>*>(this));

        installed = true;

        logger::info("MCM capture event is installed, capture debugger is {}", GetSettings().captureRawRecords ? "enabled" : "disabled");

        return true;
    }

    void Capture::Reset()
    {
        std::scoped_lock lock(captureMutex);
        // Old scheduled tasks will stop when they see a different loaded game session.
        ++loadedGameSession;
        // To distinguish between MCMs opened in the same game session.
        ++configSession;
        eventCount = 0;
        menuOpenedEventID = 0;
        journalMenuOpen = false;
        selection = {};
        records.clear();
        settings.clear();
        mcmIdentities.clear();
        detectedActivations.clear();
        pendingAutoBackupSettings.clear();
        recordedConfigSessions.clear();
        logger::info("Capture session reset");
    }

    void Capture::MergeSettings(std::vector<CapturedSetting>& a_settings, std::string_view a_modID)
    {
        std::scoped_lock lock(captureMutex);
        for (const auto& setting : settings) {
            if (setting.identityComplete && !setting.command && setting.selection.identity.modID == a_modID && !MCMCommandSupport::IsExcludedPage(a_modID, setting.selection.pageName, setting.selection.pageIndex)) {
                Deduplicate(a_settings, setting, false);
            }
        }
    }

    std::optional<MCMActivationState> Capture::FindDetectedActivation(std::string_view a_modID)
    {
        std::scoped_lock lock(captureMutex);
        for (const auto& activation : detectedActivations) {
            if (activation.activation.selection.identity.modID == a_modID) {
                return activation;
            }
        }
        return std::nullopt;
    }

    RE::BSEventNotifyControl Capture::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
    {
        if (!a_event || MCMCallWatch::IsBusy()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::scoped_lock lock(captureMutex);
        auto type = ParseEventType(a_event->eventName.c_str());
        if (type == EventType::Unknown) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (type == EventType::MessageDialogClosed) {
            // SkyUI reports the user's answer before its waiting handler continues.
            for (auto record = records.rbegin(); record != records.rend(); ++record) {
                if (record->eventID <= menuOpenedEventID || record->type == EventType::ModSelected) {
                    break;
                }
                if (IsValueChange(record->type)) {
                    record->confirmationAccepted = record->confirmationAccepted || a_event->numArg != 0.0F;
                    record->confirmationCancelled = record->confirmationCancelled || a_event->numArg == 0.0F;
                    break;
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        // Keep the callback data now, then read Scaleform after this callback returns.
        UpdateSelectionFromEvent(type, *a_event);

        if (type == EventType::ModSelected) {
            // SkyUI holds one open config, so choosing a mod closes the previous one.
            ++configSession;
        }

        auto eventID = RecordEvent(type, *a_event);
        QueueMenuRead(CaptureRequest{ eventID, loadedGameSession, IsValueChange(type) });

        return RE::BSEventNotifyControl::kContinue;
    }

    void Capture::ReadMenu(const CaptureRequest& a_request)
    {
        auto* record = FindRecord(a_request.eventID);
        if (!record) {
            return;
        }

        record->state = MCMMenu::ReadState();
        if (record->selection.modIndex == selection.modIndex) {
            FindActiveMCMIdentity(record->type, record->state);
        }

        SyncOpeningPage(*record);
        if (MCMCommandSupport::IsExcludedPage(record->selection.identity.modID, record->selection.pageName, record->selection.pageIndex)) {
            return;
        }
        
        if (IsValueChange(record->type) && !record->selection.identity.modID.empty()) {
            // Only a change can rebuild a page, and an unidentified MCM simply records no rebuild.
            auto activeMCM = MCMRegistry().ReadActiveMCM();
            if (activeMCM && activeMCM->identity.modID == record->selection.identity.modID) {
                record->pageHash = MCMScript(activeMCM->mcmScript).ReadPageHash();
            }
        }
        if (record->type == EventType::OptionHighlighted || record->type == EventType::MenuSelected || ControlTypeForEvent(record->type) == ControlType::Option) {
            RememberControl(*record);
        }
        if (record->type == EventType::OptionSelected && record->control) {
            auto activeMCM = MCMRegistry().ReadActiveMCM();
            if (activeMCM && activeMCM->identity.modID == record->selection.identity.modID) {
                CaptureMCMActivation(*record, MCMScript(activeMCM->mcmScript));
            }
        }

        QueueCaptureCompletion(a_request, QueueDelayFrames);
    }

    void Capture::CompleteCapture(CaptureRequest a_request)
    {
        if (MCMCallWatch::IsBusy()) {
            return;
        }
        // Find the raw record made before the menu finished updating.
        auto* record = FindRecord(a_request.eventID);
        if (!record) {
            return;
        }

        const bool pageReady = SyncOpeningPage(*record);
        if (pageReady && ControlTypeForEvent(record->type) == ControlType::Option && !IsCapturePageCurrent(*record)) {
            logger::info("Stopped toggle capture {} after navigation or a newer change", a_request.eventID);
            return;
        }

        // The second read contains the new value and dialog text.
        record->stateAfter = MCMMenu::ReadState();
        if (record->selection.modIndex == selection.modIndex) {
            FindActiveMCMIdentity(record->type, record->stateAfter);
        }
        if (record->type == EventType::MenuSelected) {
            // The dropdown data request may still have been busy during the first read.
            RememberControl(*record);
        }
        if (IsValueChange(record->type)) {
            // Only accepted or selected values become profile settings.
            if (!pageReady || !ProcessCapturedEvent(*record)) {
                const bool commandEvent = GetSettings().recordActions && (record->type == EventType::OptionSelected || record->type == EventType::MenuAccepted);
                const uint32_t maximumReads = commandEvent ? maximumCommandCaptureReads : maximumCaptureReads;
                if (!record->stateAfter.contains("error") && a_request.readAttempts < maximumReads) {
                    if (a_request.readAttempts == 0) {
                        logger::debug("Waiting for MCM capture {}: mod: '{}', page: '{}', option: {}", a_request.eventID, record->selection.identity.modName, record->selection.pageName, record->selection.optionIndex);
                    }
                    ++a_request.readAttempts;
                    QueueCaptureCompletion(a_request, captureReadDelayFrames);
                    return;
                }
                logger::warn("MCM capture {} could not finish: mod: '{}', page: '{}', option: {}; saved profile left unchanged", a_request.eventID, record->selection.identity.modName, record->selection.pageName, record->selection.optionIndex);
            }
        }
        if (a_request.persist) {
            CaptureStorage::Save(records, settings, GetSettings().captureRawRecords);
        }
    }

    RE::BSEventNotifyControl Capture::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
    {
        if (!a_event || std::string_view(a_event->menuName.c_str()) != RE::JournalMenu::MENU_NAME) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::scoped_lock lock(captureMutex);
        if (a_event->opening) {
            journalMenuOpen = true;
            menuOpenedEventID = eventCount;
            pendingAutoBackupSettings.clear();
            logger::info("Journal Menu opened; watching for MCM configuration events");
        }
        else {
            journalMenuOpen = false;
            menuOpenedEventID = eventCount;
            ++configSession;
            if (!records.empty()) {
                CaptureStorage::Save(records, settings, GetSettings().captureRawRecords);
            }
            ShowAutoBackupResults();
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void Capture::ShowAutoBackupResults()
    {
        if (!GetSettings().autoBackup || pendingAutoBackupSettings.empty()) {
            pendingAutoBackupSettings.clear();
            return;
        }

        std::vector<AutoBackupResult> results;
        for (const auto& setting : pendingAutoBackupSettings) {
            auto result = results.begin();
            for (; result != results.end() && result->identity.modID != setting.selection.identity.modID; ++result) {}
            if (result == results.end()) {
                AutoBackupResult newResult;
                newResult.identity = setting.selection.identity;
                results.push_back(std::move(newResult));
                result = results.end();
                --result;
            }
            ++result->stats.settingCount;
        }

        std::vector<ActivityModResult> activityMods;
        activityMods.reserve(results.size());
        BackupStats total;
        for (auto& result : results) {
            result.stats.MCMCount = 1;
            total += result.stats;

            activityMods.emplace_back(result.identity, result.stats);

            HUD::GetSingleton()->ShowBackupMCM(result.identity.modName, result.stats, OperationMode::Automatic);
        }
        Activity::GetSingleton()->RecordBackup(OperationMode::Automatic, total, activityMods);
        HUD::GetSingleton()->ShowBackupSummary(total);
        logger::info("Automatic backup updated {} settings from {} MCMs", total.settingCount, total.MCMCount);
        pendingAutoBackupSettings.clear();
    }

    void Capture::ForgetMCMs(const MCMFilter& a_modIDs)
    {
        if (a_modIDs.empty()) {
            return;
        }

        std::scoped_lock lock(captureMutex);
        for (const auto& modID : a_modIDs) {
            recordedConfigSessions.erase(modID);
        }

        auto activation = detectedActivations.begin();
        while (activation != detectedActivations.end()) {
            if (ContainsMCMID(a_modIDs, activation->activation.selection.identity.modID)) {
                activation = detectedActivations.erase(activation);
            }
            else {
                ++activation;
            }
        }

        // Captured copies would otherwise be written back by the next automatic backup.
        auto setting = settings.begin();
        while (setting != settings.end()) {
            if (ContainsMCMID(a_modIDs, setting->selection.identity.modID)) {
                setting = settings.erase(setting);
            }
            else {
                ++setting;
            }
        }

        auto pending = pendingAutoBackupSettings.begin();
        while (pending != pendingAutoBackupSettings.end()) {
            if (ContainsMCMID(a_modIDs, pending->selection.identity.modID)) {
                pending = pendingAutoBackupSettings.erase(pending);
            }
            else {
                ++pending;
            }
        }
    }

    bool Capture::IsConfigReopened(const std::string& a_modID, uint32_t a_configSession) const
    {
        auto existing = recordedConfigSessions.find(a_modID);
        // Nothing written for this MCM yet in this game, so there is no break to replay.
        return existing != recordedConfigSessions.end() && existing->second != a_configSession;
    }

    uint64_t Capture::RecordEvent(EventType a_type, const SKSE::ModCallbackEvent& a_event)
    {
        if (records.size() == maximumRecords) {
            records.erase(records.begin());
        }

        CaptureRecord record;
        record.eventID = ++eventCount;
        record.type = a_type;
        // The meaning of these arguments depends on the event type.
        record.stringArgument = a_event.strArg.c_str();
        record.numberArgument = a_event.numArg;
        record.senderFormID = a_event.sender ? a_event.sender->GetFormID() : 0;
        record.selection = selection;
        record.configSession = configSession;
        records.push_back(std::move(record));

        logger::info("Captured {} with mod: '{}', modID: '{}', page: '{}', option: {}, str: '{}', num: {}", 
            EventName(a_type), selection.identity.modName, selection.identity.modID, selection.pageName, selection.optionIndex, a_event.strArg.c_str(), a_event.numArg);

        return eventCount;
    }

}
