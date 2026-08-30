#include "menu/hud.hpp"
#include "profile/activity.hpp"
#include "profile/capture.hpp"

namespace MCMMemory
{

    // Some values may become readable a few frames after the callback.
    constexpr int QueueDelayFrames = 2;
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

        logger::info("MCM capture event installed; capture debugger is {}", GetSettings().captureRawRecords ? "enabled" : "disabled");

        return true;
    }

    void Capture::Reset()
    {
        std::scoped_lock lock(captureMutex);
        // Old scheduled tasks will stop when they see a different loaded game session.
        ++loadedGameSession;
        eventCount = 0;
        selection = {};
        mcmIdentities.clear();
        records.clear();
        settings.clear();
        pendingAutoBackupSettings.clear();
        logger::info("Capture session reset");
    }

    void Capture::MergeSettings(Profile& a_profile)
    {
        std::scoped_lock lock(captureMutex);
        for (const auto& setting : settings) {
            if (setting.identityComplete) {
                Deduplicate(a_profile, setting);
            }
        }
    }

    RE::BSEventNotifyControl Capture::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
    {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::scoped_lock lock(captureMutex);
        auto type = ParseEventType(a_event->eventName.c_str());
        if (type == EventType::Unknown) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Keep the callback data now, then read Scaleform after this callback returns.
        UpdateSelectionFromEvent(type, *a_event);

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
            UpdateSelectionFromMenu(record->type, record->state);
        }

        QueueCaptureCompletion(a_request, QueueDelayFrames);
    }

    void Capture::CompleteCapture(uint64_t a_eventID, bool a_persist)
    {
        // Find the raw record made before the menu finished updating.
        auto* record = FindRecord(a_eventID);
        if (!record) {
            return;
        }

        // The second read contains the new value and dialog text.
        record->stateAfter = MCMMenu::ReadState();
        if (record->selection.modIndex == selection.modIndex) {
            FindActiveMCMIdentity(record->type, record->stateAfter);
        }
        if (IsValueChange(record->type)) {
            // Only accepted or selected values become profile settings.
            ProcessCapturedEvent(*record);
        }
        if (a_persist) {
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
            pendingAutoBackupSettings.clear();
            logger::info("Journal Menu opened; watching for MCM configuration events");
        }
        else {
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
        records.push_back(std::move(record));

        logger::info("Captured {}: mod='{}', modID='{}', page='{}', option={}, str='{}', num={}", EventName(a_type), selection.identity.modName, selection.identity.modID, selection.pageName, selection.optionIndex, a_event.strArg.c_str(), a_event.numArg);

        return eventCount;
    }

}
