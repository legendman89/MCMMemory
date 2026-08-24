#include "menu/activity_defs.hpp"
#include "menu/activity.hpp"
#include "menu/translate.hpp"
#include "utils/helper.hpp"

#include <ctime>

namespace MCMMemory::Menu
{
    inline constexpr std::array<std::string_view, ToIndex(OperationMode::Count)> operationModeNames
    {
        "Automatic",
        "Manual"
    };

#define MAKE_ACTIVITY_COLUMN(label, width, renderer, member) ActivityColumn{ label, width },
    
    inline constexpr std::array<ActivityColumn, 3> backupActivityColumns
    {
        FOREACH_BACKUP_ACTIVITY_COLUMN(MAKE_ACTIVITY_COLUMN)
    };

    inline constexpr std::array<ActivityColumn, 4> restoreActivityColumns
    {
        FOREACH_RESTORE_ACTIVITY_COLUMN(MAKE_ACTIVITY_COLUMN)
    };

#undef MAKE_ACTIVITY_COLUMN

    std::string ActivityMenu::FormatSummary(const ActivityEntry& a_entry) const
    {
        const auto mode = operationModeNames[ToIndex(a_entry.mode)];
        const bool cancelled = a_entry.result == OperationResult::Cancelled;
        const auto separator = cancelled ? " cancelled: " : ": ";
        if (a_entry.type == OperationType::Backup) {
            if (cancelled) {
                return std::format("{} backup{}{} MCMs, {} settings read", mode, separator, a_entry.backupStats.MCMCount, a_entry.backupStats.settingCount);
            }
            if (a_entry.backupStats.failedMCMCount > 0) {
                return std::format("{} backup: {} MCMs, {} settings, {} failed", mode, a_entry.backupStats.MCMCount,
                    a_entry.backupStats.settingCount, a_entry.backupStats.failedMCMCount);
            }
            return std::format("{} backup: {} MCMs, {} settings", mode, a_entry.backupStats.MCMCount, a_entry.backupStats.settingCount);
        }
        return std::format("{} restore{}{} MCMs, {} changed, {} already set", mode, separator, a_entry.restoreStats.MCMCount,
            a_entry.restoreStats.appliedSettingCount, a_entry.restoreStats.unchangedSettingCount);
    }

    std::string ActivityMenu::FormatRelativeTime(const ActivityEntry& a_entry) const
    {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - a_entry.when).count();
        seconds = std::max<int64_t>(seconds, 0);

        if (seconds < 5) {
            return "Just now";
        }

        if (seconds < 60) {
            return std::format("{}s ago", seconds);
        }

        const auto minutes = seconds / 60;
        if (minutes < 60) {
            return std::format("{}m ago", minutes);
        }

        const auto hours = minutes / 60;
        if (hours < 24) {
            return std::format("{}h ago", hours);
        }

        return std::format("{}d ago", hours / 24);
    }

    std::string ActivityMenu::FormatExactTime(const ActivityEntry& a_entry) const
    {
        const time_t time = std::chrono::system_clock::to_time_t(a_entry.when);
        std::tm localTime{};
        localtime_s(std::addressof(localTime), std::addressof(time));
        return std::format("{:04}-{:02}-{:02}  {:02}:{:02}:{:02}", localTime.tm_year + 1900, localTime.tm_mon + 1,
            localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
    }

    void ActivityMenu::Render()
    {
        const auto entries = Activity::GetSingleton()->ReadEntries();
        if (entries->empty()) {
            GUI::TextUnformatted(Trans::Tr("No activity yet.").c_str());
            return;
        }

        const auto flags = GUI::ImGuiTableFlags_RowBg | GUI::ImGuiTableFlags_BordersInnerH | GUI::ImGuiTableFlags_Resizable | GUI::ImGuiTableFlags_ScrollY;
        if (GUI::BeginTable("MCM Memory Activity", 2, flags, GUI::ImVec2(0.0F, 320.0F))) {

            GUI::TableSetupColumn(Trans::Tr("Result").c_str(), GUI::ImGuiTableColumnFlags_WidthStretch);
            GUI::TableSetupColumn(Trans::Tr("When").c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, 140.0F);

            GUI::TableHeadersRow();

            for (const auto& entry : *entries) {
                const auto summary = FormatSummary(entry);
                const auto id = std::format("Activity {}", entry.id);

                GUI::TableNextRow();
                GUI::TableSetColumnIndex(0);

                GUI::PushID(id.c_str());
                if (GUI::Selectable(summary.c_str(), selectedID == entry.id, GUI::ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedID = entry.id;
                    detailsOpen = true;
                }
                GUI::PopID();

                GUI::TableSetColumnIndex(1);

                const auto relativeTime = FormatRelativeTime(entry);
                GUI::TextUnformatted(relativeTime.c_str());
            }
            GUI::EndTable();
        }

        RenderDetails(*entries);
    }

    void ActivityMenu::RenderDetails(const std::vector<ActivityEntry>& a_entries)
    {
        if (!detailsOpen) {
            return;
        }

        const ActivityEntry* selected{};
        for (const auto& entry : a_entries) {
            if (entry.id == selectedID) {
                selected = std::addressof(entry);
                break;
            }
        }
        if (!selected) {
            detailsOpen = false;
            return;
        }

        GUI::SetNextWindowSize(GUI::ImVec2(760.0F, 430.0F), GUI::ImGuiCond_FirstUseEver);
        const auto windowTitle = std::format("{}###MCM Memory Activity Details", Trans::Tr("Activity Details"));
        if (GUI::Begin(windowTitle.c_str(), std::addressof(detailsOpen), GUI::ImGuiWindowFlags_NoCollapse)) {
            const auto summary = FormatSummary(*selected);
            const auto exactTime = FormatExactTime(*selected);
            GUI::TextWrapped("%s", summary.c_str());
            GUI::TextUnformatted(exactTime.c_str());
            
            GUI::Spacing();

            if (selected->type == OperationType::Backup) {
                RenderBackupMods(*selected);
            }
            else {
                RenderRestoreMods(*selected);
            }
        }
        GUI::End();
    }

    bool ActivityMenu::BeginModTable(const ActivityEntry& a_entry, const char* a_id, const ActivityColumn* a_columns, size_t a_columnCount) const
    {
        if (a_entry.mods.empty()) {
            GUI::TextUnformatted(Trans::Tr("No per-mod results were recorded.").c_str());
            return false;
        }

        const auto flags = GUI::ImGuiTableFlags_RowBg | GUI::ImGuiTableFlags_BordersInnerH | GUI::ImGuiTableFlags_Resizable | GUI::ImGuiTableFlags_ScrollY;
        if (!GUI::BeginTable(a_id, static_cast<int>(a_columnCount + 1), flags)) {
            return false;
        }

        GUI::TableSetupColumn(Trans::Tr("MCM").c_str(), GUI::ImGuiTableColumnFlags_WidthStretch);
        for (size_t index = 0; index < a_columnCount; ++index) {
            GUI::TableSetupColumn(Trans::Tr(a_columns[index].label).c_str(), GUI::ImGuiTableColumnFlags_WidthFixed, a_columns[index].width);
        }
        GUI::TableHeadersRow();
        return true;
    }

    void ActivityMenu::RenderModName(const ActivityModResult& a_mod) const
    {
        GUI::TableNextRow();
        GUI::TableSetColumnIndex(0);
        const auto modName = GetDisplayModName(a_mod.modName);
        GUI::TextUnformatted(modName.c_str());
    }

    void ActivityMenu::RenderValue(uint32_t a_value) const
    {
        GUI::TableNextColumn();
        GUI::Text("%u", a_value);
    }

    void ActivityMenu::RenderStatus(OperationResult a_result) const
    {
        GUI::TableNextColumn();
        GUI::TextUnformatted(Trans::Tr(OperationResultName(a_result)).c_str());
    }

    void ActivityMenu::RenderBackupMods(const ActivityEntry& a_entry) const
    {
        if (!BeginModTable(a_entry, "Backup Activity Details", backupActivityColumns.data(), backupActivityColumns.size())) {
            return;
        }

        for (const auto& mod : a_entry.mods) {
            RenderModName(mod);
#define RENDER_ACTIVITY_VALUE(label, width, renderer, member) renderer(mod.member);
            FOREACH_BACKUP_ACTIVITY_COLUMN(RENDER_ACTIVITY_VALUE)
#undef RENDER_ACTIVITY_VALUE
        }
        GUI::EndTable();
    }

    void ActivityMenu::RenderRestoreMods(const ActivityEntry& a_entry) const
    {
        if (!BeginModTable(a_entry, "Restore Activity Details", restoreActivityColumns.data(), restoreActivityColumns.size())) {
            return;
        }

        for (const auto& mod : a_entry.mods) {
            RenderModName(mod);
#define RENDER_ACTIVITY_VALUE(label, width, renderer, member) renderer(mod.member);
            FOREACH_RESTORE_ACTIVITY_COLUMN(RENDER_ACTIVITY_VALUE)
#undef RENDER_ACTIVITY_VALUE
        }
        GUI::EndTable();
    }
}

#undef FOREACH_BACKUP_ACTIVITY_COLUMN
#undef FOREACH_RESTORE_ACTIVITY_COLUMN
