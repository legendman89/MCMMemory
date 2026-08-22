#pragma once

#include "profile/types.hpp"

namespace MCMMemory
{
    class CaptureStorage
    {
    public:

        static inline std::filesystem::path Path() { return GetPluginDataPath() / "Capture.json"; }

        static bool Save(const std::vector<CaptureRecord>& a_records, const std::vector<CapturedSetting>& a_settings, bool a_includeRawRecords);

        static nlohmann::json ToJson(const CaptureRecord& a_record);

    };
}
