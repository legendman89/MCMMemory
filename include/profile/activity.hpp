#pragma once

#include "profile/stats.hpp"

#include <deque>

namespace MCMMemory
{
    enum class OperationType
    {
        Backup,
        Restore,
        Count
    };

    enum class OperationResult
    {
        Completed,
        Failed,
        Cancelled,
        Count
    };

    struct ActivityModResult
    {
        ActivityModResult() = default;

        ActivityModResult(std::string_view a_modName, const BackupStats& a_stats) :
            modName(a_modName), backupStats(a_stats)
        {}

        ActivityModResult(std::string_view a_modName, const RestoreStats& a_stats) :
            modName(a_modName), restoreStats(a_stats)
        {}

        std::string modName;

        BackupStats backupStats;

        RestoreStats restoreStats;
    };

    struct ActivityEntry
    {
        std::vector<ActivityModResult> mods;

        std::chrono::system_clock::time_point when{};

        uint64_t id{};

        BackupStats backupStats;

        RestoreStats restoreStats;

        OperationType type{ OperationType::Backup };

        OperationMode mode{ OperationMode::Automatic };

        OperationResult result{ OperationResult::Completed };
    };

    // Singleton class that manages the activity log for MCM backup and restore operations.
    class Activity
    {
    public:

        static Activity* GetSingleton()
        {
            static Activity singleton;
            return std::addressof(singleton);
        }

        bool Load();

        void RecordBackup(OperationMode a_mode, const BackupStats& a_stats, const std::vector<ActivityModResult>& a_mods);

        void RecordRestore(OperationMode a_mode, const RestoreStats& a_stats, const std::vector<ActivityModResult>& a_mods);

        inline std::shared_ptr<const std::vector<ActivityEntry>> ReadEntries() const
        {
            std::lock_guard lock(activityMutex);
            return newestEntries;
        }

    private:

        inline std::filesystem::path Path() const
        {
            return GetPluginDataPath() / "Activity.json";
        }

        void Record(ActivityEntry a_entry);

        void UpdateNewestEntries();

        bool Save(const std::vector<ActivityEntry>& a_entries) const;

        nlohmann::json ToJson(const std::vector<ActivityEntry>& a_entries) const;

        nlohmann::json ToJson(const BackupStats& a_stats) const;

        nlohmann::json ToJson(const RestoreStats& a_stats) const;

        void ReadStats(const nlohmann::json& a_document, BackupStats& a_stats) const;

        void ReadStats(const nlohmann::json& a_document, RestoreStats& a_stats) const;

        bool ReadEntry(const nlohmann::json& a_document, ActivityEntry& a_entry) const;

        mutable std::mutex activityMutex;

        std::deque<ActivityEntry> entries;

        std::shared_ptr<const std::vector<ActivityEntry>> newestEntries{ std::make_shared<const std::vector<ActivityEntry>>() };

        uint64_t nextID{ 1 };

        static constexpr size_t maximumEntries{ 50 };
    };
}
