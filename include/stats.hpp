#pragma once

#include <cstdint>
#include <cstring>

namespace MCMMemory
{
    struct BackupStats
    {
        uint32_t MCMCount{};

        uint32_t settingCount{};

        uint32_t skippedSettingCount{};

        uint32_t failedMCMCount{};

        void Reset()
        {
            std::memset(this, 0, sizeof(*this));
        }

        BackupStats& operator+=(const BackupStats& a_other)
        {
            MCMCount += a_other.MCMCount;
            settingCount += a_other.settingCount;
            skippedSettingCount += a_other.skippedSettingCount;
            failedMCMCount += a_other.failedMCMCount;
            return *this;
        }
    };

    struct RestoreStats
    {
        uint32_t appliedSettingCount{};

        uint32_t unchangedSettingCount{};

        uint32_t skippedSettingCount{};

        void Reset()
        {
            std::memset(this, 0, sizeof(*this));
        }

        RestoreStats& operator+=(const RestoreStats& a_other)
        {
            appliedSettingCount += a_other.appliedSettingCount;
            unchangedSettingCount += a_other.unchangedSettingCount;
            skippedSettingCount += a_other.skippedSettingCount;
            return *this;
        }
    };
}
