#pragma once

#define FOREACH_BACKUP_ACTIVITY_COLUMN(COLUMN) \
    COLUMN("Settings", 90.0F, RenderValue, backupStats.settingCount) \
    COLUMN("Skipped", 90.0F, RenderValue, backupStats.skippedSettingCount) \
    COLUMN("Status", 90.0F, RenderStatus, result)

#define FOREACH_RESTORE_ACTIVITY_COLUMN(COLUMN) \
    COLUMN("Changed", 90.0F, RenderValue, restoreStats.appliedSettingCount) \
    COLUMN("Already set", 100.0F, RenderValue, restoreStats.unchangedSettingCount) \
    COLUMN("Skipped", 90.0F, RenderValue, restoreStats.skippedSettingCount) \
    COLUMN("Status", 90.0F, RenderStatus, result)
