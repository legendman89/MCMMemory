#pragma once

#include "profile/types.hpp"
#include "utils/helper.hpp"

namespace MCMMemory
{
    struct JSON
    {
        JSON() = delete;

        template <class T>
        static bool ReadValue(const nlohmann::json& a_object, std::string_view a_name, T& a_value)
        {
            auto value = a_object.find(std::string(a_name));
            if (value == a_object.end()) {
                return false;
            }
            value->get_to(a_value);
            return true;
        }

        static std::optional<double> ReadNumber(const nlohmann::json& a_object, std::string_view a_name)
        {
            auto value = a_object.find(std::string(a_name));
            if (value == a_object.end() || !value->is_number()) {
                return std::nullopt;
            }
            return value->get<double>();
        }

        static std::optional<std::string> ReadString(const nlohmann::json& a_object, std::string_view a_name)
        {
            auto value = a_object.find(std::string(a_name));
            if (value == a_object.end() || !value->is_string()) {
                return std::nullopt;
            }
            return value->get<std::string>();
        }

        static bool WriteFile(const std::filesystem::path& a_path, const nlohmann::json& a_document)
        {
            std::error_code error;
            std::filesystem::create_directories(a_path.parent_path(), error);
            if (error) {
                logger::error("Failed to create JSON directory for {}: {}", ToUTF8(a_path), error.message());
                return false;
            }

            auto temporaryPath = a_path;
            temporaryPath += ".tmp";
            std::ofstream stream(temporaryPath, std::ios::trunc);
            if (!stream) {
                logger::error("Failed to open temporary JSON file {}", ToUTF8(temporaryPath));
                return false;
            }
            stream << a_document.dump(2);
            stream.flush();
            if (!stream) {
                logger::error("Failed to finish writing temporary JSON file {}", ToUTF8(temporaryPath));
                return false;
            }
            stream.close();

            if (!MoveFileExW(temporaryPath.c_str(), a_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                logger::error("Failed to replace JSON file {} with Windows error {}", ToUTF8(a_path), GetLastError());
                std::filesystem::remove(temporaryPath, error);
                return false;
            }
            return true;
        }

        static nlohmann::json ToJson(const MCMSelection& a_selection)
        {
            return {
                { "modIndex", a_selection.modIndex },
                { "modName", a_selection.identity.modName },
                { "modID", a_selection.identity.modID },
                { "pageIndex", a_selection.pageIndex },
                { "pageName", a_selection.pageName },
                { "optionIndex", a_selection.optionIndex }
            };
        }

        static nlohmann::json ToJson(const CapturedSetting& a_setting, bool a_includeModIndex = true)
        {
            auto document = ToJson(a_setting.selection);
            if (!a_includeModIndex) {
                document.erase("modIndex");
            }
            document["sourceEventID"] = a_setting.sourceEventID;
            document["controlType"] = std::string(ControlTypeName(a_setting.type));
            document["optionLabel"] = a_setting.optionLabel;
            if (!a_setting.settingID.empty()) {
                document["settingID"] = a_setting.settingID;
            }
            document["stateName"] = a_setting.stateName;
            document["value"] = a_setting.value;
            document["valueSource"] = a_setting.valueSource;
            document["identityComplete"] = a_setting.identityComplete;
            return document;
        }

    };
}
