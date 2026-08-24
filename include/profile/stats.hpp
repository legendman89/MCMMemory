#pragma once

#include "profile/stats_defs.hpp"

#include <cstdint>
#include <cstring>

namespace MCMMemory
{
    enum class OperationMode
    {
        Automatic,
        Manual,
        Count
    };

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
