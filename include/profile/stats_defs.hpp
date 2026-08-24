#pragma once

#define FOREACH_BACKUP_STAT(STAT) \
    STAT(MCMCount) \
    STAT(settingCount) \
    STAT(skippedSettingCount) \
    STAT(failedMCMCount)

#define FOREACH_RESTORE_STAT(STAT) \
    STAT(MCMCount) \
    STAT(appliedSettingCount) \
    STAT(unchangedSettingCount) \
    STAT(skippedSettingCount)
