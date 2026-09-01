#pragma once

#include "profile/stats.hpp"
#include "profile/types.hpp"
#include "utils/helper.hpp"

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

    inline constexpr std::array<std::string_view, ToIndex(OperationResult::Count)> operationResultNames
    {
        "Completed",
        "Failed",
        "Cancelled"
    };

    inline std::string_view OperationResultName(OperationResult a_result)
    {
        return operationResultNames[ToIndex(a_result)];
    }

    struct ActivityModResult
    {
        ActivityModResult() = default;

        ActivityModResult(const MCMIdentity& a_identity, const BackupStats& a_stats, OperationResult a_result = OperationResult::Completed) :
            modName(a_identity.modName), modID(a_identity.modID), backupStats(a_stats), result(a_result)
        {}

        ActivityModResult(const MCMIdentity& a_identity, const RestoreStats& a_stats, OperationResult a_result = OperationResult::Completed) :
            modName(a_identity.modName), modID(a_identity.modID), restoreStats(a_stats), result(a_result)
        {}

        std::string modName;

        std::string modID;

        BackupStats backupStats;

        RestoreStats restoreStats;

        OperationResult result{ OperationResult::Completed };
    };

    inline OperationResult MCMResultsStatus(const std::vector<ActivityModResult>& a_results)
    {
        for (const auto& result : a_results) {
            if (result.result != OperationResult::Completed) {
                return OperationResult::Failed;
            }
        }
        return OperationResult::Completed;
    }

    // A final retry replaces the earlier attempt instead of counting the MCM twice.
    template <class Stats>
    inline void UpdateMCMResult(std::vector<ActivityModResult>& a_results, ActivityModResult a_result, Stats ActivityModResult::* a_member, Stats& a_total)
    {
        for (auto& result : a_results) {
            if (result.modID == a_result.modID) {
                a_total -= result.*a_member;
                a_total += a_result.*a_member;
                result = std::move(a_result);
                return;
            }
        }
        a_total += a_result.*a_member;
        a_results.push_back(std::move(a_result));
    }

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

        void RecordBackup(OperationMode a_mode, const BackupStats& a_stats, const std::vector<ActivityModResult>& a_mods, OperationResult a_result = OperationResult::Completed);

        void RecordRestore(OperationMode a_mode, const RestoreStats& a_stats, const std::vector<ActivityModResult>& a_mods, OperationResult a_result = OperationResult::Completed);

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
