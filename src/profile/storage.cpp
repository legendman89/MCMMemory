#include "profile/storage.hpp"
#include "utils/helper.hpp"
#include "utils/json.hpp"

namespace MCMMemory
{
    bool CaptureStorage::Save(const std::vector<CaptureRecord>& a_records, const std::vector<CapturedSetting>& a_settings, bool a_includeRawRecords)
    {
        std::error_code error;
        std::filesystem::create_directories(GetPluginDataPath(), error);
        if (error) {
            logger::error("Failed to create capture directory: {}", error.message());
            return false;
        }

        nlohmann::json document;
        document["formatVersion"] = 1;
        document["purpose"] = "Current-session MCM registry capture debugging";
        document["rawRecordsIncluded"] = a_includeRawRecords;
        document["records"] = nlohmann::json::array();
        document["settings"] = nlohmann::json::array();
        if (a_includeRawRecords) {
            for (const auto& record : a_records) {
                document["records"].push_back(ToJson(record));
            }
        }
        for (const auto& setting : a_settings) {
            document["settings"].push_back(JSON::ToJson(setting));
        }

        std::ofstream stream(Path(), std::ios::trunc);
        if (!stream) {
            logger::error("Failed to open capture file: {}", ToUTF8(Path()));
            return false;
        }

        stream << document.dump(2);
        logger::info("Saved {} settings and {} raw records to {}", a_settings.size(), a_includeRawRecords ? a_records.size() : 0, ToUTF8(Path()));
        return true;
    }

    nlohmann::json CaptureStorage::ToJson(const CaptureRecord& a_record)
    {
        // Raw records keep the full selection and both menu reads.
        return {
            { "eventID", a_record.eventID },
            { "event", std::string(EventName(a_record.type)) },
            { "stringArgument", a_record.stringArgument },
            { "numberArgument", a_record.numberArgument },
            { "senderFormID", std::format("{:08X}", a_record.senderFormID) },
            { "selection", JSON::ToJson(a_record.selection) },
            { "state", a_record.state },
            { "stateAfter", a_record.stateAfter }
        };
    }

}
