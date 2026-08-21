#pragma once

#include "types.hpp"

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
            document["value"] = a_setting.value;
            document["valueSource"] = a_setting.valueSource;
            document["identityComplete"] = a_setting.identityComplete;
            return document;
        }

    };
}
