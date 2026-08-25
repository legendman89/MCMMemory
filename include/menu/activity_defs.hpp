#pragma once

#define FOREACH_BACKUP_ACTIVITY_COLUMN(COLUMN) \
    COLUMN("Activity.Column.Settings", 90.0F, RenderValue, backupStats.settingCount) \
    COLUMN("Activity.Column.Skipped", 90.0F, RenderValue, backupStats.skippedSettingCount) \
    COLUMN("Activity.Column.Status", 90.0F, RenderStatus, result)

#define FOREACH_RESTORE_ACTIVITY_COLUMN(COLUMN) \
    COLUMN("Activity.Column.Changed", 90.0F, RenderValue, restoreStats.appliedSettingCount) \
    COLUMN("Activity.Column.AlreadySet", 100.0F, RenderValue, restoreStats.unchangedSettingCount) \
    COLUMN("Activity.Column.Skipped", 90.0F, RenderValue, restoreStats.skippedSettingCount) \
    COLUMN("Activity.Column.Status", 90.0F, RenderStatus, result)
