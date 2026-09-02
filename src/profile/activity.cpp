#include "profile/activity.hpp"
#include "utils/helper.hpp"
#include "utils/json.hpp"

namespace MCMMemory
{
    bool Activity::Load()
    {
        std::error_code error;
        const bool exists = std::filesystem::exists(Path(), error);
        if (error) {
            logger::error("Failed to check activity history {}: {}", ToUTF8(Path()), error.message());
            return false;
        }
        if (!exists) {
            logger::info("Activity history does not exist yet at {}", ToUTF8(Path()));
            return true;
        }

        std::ifstream stream(Path());
        if (!stream) {
            logger::error("Failed to open activity history {}", ToUTF8(Path()));
            return false;
        }

        std::deque<ActivityEntry> loadedEntries;
        uint64_t loadedNextID{ 1 };
        try {
            const auto document = JSON::DecodeDocumentText(nlohmann::json::parse(stream));
            int formatVersion{};
            if (!document.is_object() || !JSON::ReadValue(document, "formatVersion", formatVersion) || formatVersion != 1 ||
                !document.contains("entries") || !document["entries"].is_array()) {
                logger::error("Activity history entries are missing or invalid in {}", ToUTF8(Path()));
                return false;
            }
            for (const auto& entryDocument : document["entries"]) {
                ActivityEntry entry;
                if (!ReadEntry(entryDocument, entry)) {
                    logger::warn("Skipped an invalid activity history entry");
                    continue;
                }
                loadedNextID = std::max(loadedNextID, entry.id + 1);
                loadedEntries.push_back(std::move(entry));
                if (loadedEntries.size() > maximumEntries) {
                    loadedEntries.pop_front();
                }
            }
        }
        catch (const std::exception& exception) {
            logger::error("Failed to read activity history {}: {}", ToUTF8(Path()), exception.what());
            return false;
        }

        std::lock_guard lock(activityMutex);
        entries = std::move(loadedEntries);
        nextID = loadedNextID;
        UpdateNewestEntries();
        logger::info("Loaded {} activity history entries", entries.size());
        return true;
    }

    void Activity::RecordBackup(OperationMode a_mode, const BackupStats& a_stats, const std::vector<ActivityModResult>& a_mods, OperationResult a_result)
    {
        ActivityEntry entry;
        entry.mods = a_mods;
        entry.when = std::chrono::system_clock::now();
        entry.backupStats = a_stats;
        entry.type = OperationType::Backup;
        entry.mode = a_mode;
        entry.result = a_result;
        Record(std::move(entry));
    }

    void Activity::RecordRestore(OperationMode a_mode, const RestoreStats& a_stats, const std::vector<ActivityModResult>& a_mods, OperationResult a_result)
    {
        ActivityEntry entry;
        entry.mods = a_mods;
        entry.when = std::chrono::system_clock::now();
        entry.restoreStats = a_stats;
        entry.type = OperationType::Restore;
        entry.mode = a_mode;
        entry.result = a_result;
        Record(std::move(entry));
    }

    void Activity::Record(ActivityEntry a_entry)
    {
        std::vector<ActivityEntry> savedEntries;
        {
            std::lock_guard lock(activityMutex);
            a_entry.id = nextID++;
            entries.push_back(std::move(a_entry));
            if (entries.size() > maximumEntries) {
                entries.pop_front();
            }
            savedEntries.assign(entries.begin(), entries.end());
            UpdateNewestEntries();
        }

        if (!Save(savedEntries)) {
            logger::error("Activity history could not be saved");
        }
    }

    void Activity::UpdateNewestEntries()
    {
        auto updatedEntries = std::make_shared<std::vector<ActivityEntry>>();
        updatedEntries->reserve(entries.size());
        for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
            updatedEntries->push_back(*entry);
        }
        newestEntries = std::move(updatedEntries);
    }

    bool Activity::Save(const std::vector<ActivityEntry>& a_entries) const
    {
        if (!JSON::WriteFile(Path(), ToJson(a_entries))) {
            return false;
        }
        logger::info("Saved {} activity history entries to {}", a_entries.size(), ToUTF8(Path()));
        return true;
    }

    nlohmann::json Activity::ToJson(const std::vector<ActivityEntry>& a_entries) const
    {
        nlohmann::json document;
        document["formatVersion"] = 1;
        document["entries"] = nlohmann::json::array();
        for (const auto& entry : a_entries) {
            nlohmann::json entryDocument
            {
                { "id", entry.id },
                { "timestamp", std::chrono::duration_cast<std::chrono::seconds>(entry.when.time_since_epoch()).count() },
                { "type", static_cast<int>(entry.type) },
                { "mode", static_cast<int>(entry.mode) },
                { "result", static_cast<int>(entry.result) },
                { "backupStats", ToJson(entry.backupStats) },
                { "restoreStats", ToJson(entry.restoreStats) },
                { "mods", nlohmann::json::array() }
            };
            for (const auto& mod : entry.mods) {
                entryDocument["mods"].push_back({
                    { "modName", mod.modName },
                    { "modID", mod.modID },
                    { "result", static_cast<int>(mod.result) },
                    { "backupStats", ToJson(mod.backupStats) },
                    { "restoreStats", ToJson(mod.restoreStats) }
                });
            }
            document["entries"].push_back(std::move(entryDocument));
        }
        return document;
    }

    nlohmann::json Activity::ToJson(const BackupStats& a_stats) const
    {
        nlohmann::json document;
#define WRITE_ACTIVITY_STAT(name) document[#name] = a_stats.name;
        FOREACH_BACKUP_STAT(WRITE_ACTIVITY_STAT)
#undef WRITE_ACTIVITY_STAT
        return document;
    }

    nlohmann::json Activity::ToJson(const RestoreStats& a_stats) const
    {
        nlohmann::json document;
#define WRITE_ACTIVITY_STAT(name) document[#name] = a_stats.name;
        FOREACH_RESTORE_STAT(WRITE_ACTIVITY_STAT)
#undef WRITE_ACTIVITY_STAT
        return document;
    }

    void Activity::ReadStats(const nlohmann::json& a_document, BackupStats& a_stats) const
    {
#define READ_ACTIVITY_STAT(name) JSON::ReadValue(a_document, #name, a_stats.name);
        FOREACH_BACKUP_STAT(READ_ACTIVITY_STAT)
#undef READ_ACTIVITY_STAT
    }

    void Activity::ReadStats(const nlohmann::json& a_document, RestoreStats& a_stats) const
    {
#define READ_ACTIVITY_STAT(name) JSON::ReadValue(a_document, #name, a_stats.name);
        FOREACH_RESTORE_STAT(READ_ACTIVITY_STAT)
#undef READ_ACTIVITY_STAT
    }

    bool Activity::ReadEntry(const nlohmann::json& a_document, ActivityEntry& a_entry) const
    {
        if (!a_document.is_object()) {
            return false;
        }

        int type{}, mode{}, result{};
        int64_t timestamp{};
        if (!JSON::ReadValue(a_document, "id", a_entry.id) || !JSON::ReadValue(a_document, "timestamp", timestamp) ||
            !JSON::ReadValue(a_document, "type", type) || !JSON::ReadValue(a_document, "mode", mode) || !JSON::ReadValue(a_document, "result", result)) {
            return false;
        }
        if (type < 0 || type >= static_cast<int>(OperationType::Count) || mode < 0 || mode >= static_cast<int>(OperationMode::Count) ||
            result < 0 || result >= static_cast<int>(OperationResult::Count)) {
            return false;
        }

        a_entry.when = std::chrono::system_clock::time_point(std::chrono::seconds(timestamp));
        a_entry.type = static_cast<OperationType>(type);
        a_entry.mode = static_cast<OperationMode>(mode);
        a_entry.result = static_cast<OperationResult>(result);

        if (a_document.contains("backupStats") && a_document["backupStats"].is_object()) {
            ReadStats(a_document["backupStats"], a_entry.backupStats);
        }
        if (a_document.contains("restoreStats") && a_document["restoreStats"].is_object()) {
            ReadStats(a_document["restoreStats"], a_entry.restoreStats);
        }
        if (!a_document.contains("mods") || !a_document["mods"].is_array()) {
            return true;
        }

        for (const auto& modDocument : a_document["mods"]) {
            if (!modDocument.is_object()) {
                continue;
            }
            ActivityModResult mod;
            if (!JSON::ReadValue(modDocument, "modName", mod.modName)) {
                continue;
            }
            JSON::ReadValue(modDocument, "modID", mod.modID);
            int modResult{};
            const bool resultAvailable = JSON::ReadValue(modDocument, "result", modResult);
            if (resultAvailable) {
                if (modResult < 0 || modResult >= static_cast<int>(OperationResult::Count)) {
                    continue;
                }
                mod.result = static_cast<OperationResult>(modResult);
            }
            if (modDocument.contains("backupStats") && modDocument["backupStats"].is_object()) {
                ReadStats(modDocument["backupStats"], mod.backupStats);
            }
            if (modDocument.contains("restoreStats") && modDocument["restoreStats"].is_object()) {
                ReadStats(modDocument["restoreStats"], mod.restoreStats);
            }
            if (!resultAvailable && mod.backupStats.failedMCMCount > 0) {
                mod.result = OperationResult::Failed;
            }
            a_entry.mods.push_back(std::move(mod));
        }
        return true;
    }
}
