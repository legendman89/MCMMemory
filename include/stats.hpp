#pragma once

#include <cstdint>
#include <cstring>

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

namespace MCMMemory
{
    struct BackupStats
    {
#define DECLARE_STAT(name) uint32_t name{};
        FOREACH_BACKUP_STAT(DECLARE_STAT)
#undef DECLARE_STAT

        void Reset()
        {
            std::memset(this, 0, sizeof(*this));
        }

        BackupStats& operator+=(const BackupStats& a_other)
        {
#define ADD_STAT(name) name += a_other.name;
            FOREACH_BACKUP_STAT(ADD_STAT)
#undef ADD_STAT
            return *this;
        }
    };

    struct RestoreStats
    {
#define DECLARE_STAT(name) uint32_t name{};
        FOREACH_RESTORE_STAT(DECLARE_STAT)
#undef DECLARE_STAT

        void Reset()
        {
            std::memset(this, 0, sizeof(*this));
        }

        RestoreStats& operator+=(const RestoreStats& a_other)
        {
#define ADD_STAT(name) name += a_other.name;
            FOREACH_RESTORE_STAT(ADD_STAT)
#undef ADD_STAT
            return *this;
        }
    };
}

#undef FOREACH_BACKUP_STAT
#undef FOREACH_RESTORE_STAT
