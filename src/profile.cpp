#include "profile.hpp"
#include "helper.hpp"
#include "json.hpp"

namespace MCMMemory
{
    bool ProfileStorage::Load(Profile& a_profile)
    {
        std::ifstream stream(Path());
        if (!stream) {
            return false;
        }

        try {
            auto document = nlohmann::json::parse(stream);
            auto settings = document.find("settings");
            if (settings != document.end() && settings->is_array()) {
                for (const auto& settingDocument : *settings) {
                    CapturedSetting setting;
                    if (FromJson(settingDocument, setting) && setting.identityComplete) {
                        a_profile.push_back(std::move(setting));
                    }
                }
            }
        } catch (const std::exception& error) {
            logger::error("Failed to read profile {}: {}", ToUTF8(Path()), error.what());
            return false;
        }
        return true;
    }

    bool ProfileStorage::UpdateSetting(const CapturedSetting& a_setting)
    {
        if (!a_setting.identityComplete) {
            return false;
        }

        Profile profile;
        std::error_code error;
        bool profileExists = std::filesystem::exists(Path(), error);
        if (error || (profileExists && !Load(profile))) {
            logger::error("Refusing to overwrite an unreadable persistent profile at {}", ToUTF8(Path()));
            return false;
        }

        Deduplicate(profile, a_setting);

        return Save(profile);
    }

    bool ProfileStorage::Save(const Profile& a_profile)
    {
        std::error_code error;
        std::filesystem::create_directories(GetPluginDataPath(), error);
        if (error) {
            logger::error("Failed to create profile directory: {}", error.message());
            return false;
        }

        std::ofstream stream(Path(), std::ios::trunc);
        if (!stream) {
            logger::error("Failed to open profile file: {}", ToUTF8(Path()));
            return false;
        }
        stream << ToJson(a_profile).dump(2);
        logger::info("Saved {} persistent profile settings to {}", a_profile.size(), ToUTF8(Path()));
        return true;
    }

    bool ProfileStorage::FromJson(const nlohmann::json& a_document, CapturedSetting& a_setting)
    {
        if (!a_document.is_object()) {
            return false;
        }

        std::string controlType;
        JSON::ReadValue(a_document, "sourceEventID", a_setting.sourceEventID);
        JSON::ReadValue(a_document, "controlType", controlType);
        JSON::ReadValue(a_document, "modIndex", a_setting.selection.modIndex);
        JSON::ReadValue(a_document, "modName", a_setting.selection.identity.modName);
        JSON::ReadValue(a_document, "modID", a_setting.selection.identity.modID);
        JSON::ReadValue(a_document, "pageIndex", a_setting.selection.pageIndex);
        JSON::ReadValue(a_document, "pageName", a_setting.selection.pageName);
        JSON::ReadValue(a_document, "optionIndex", a_setting.selection.optionIndex);
        JSON::ReadValue(a_document, "optionLabel", a_setting.optionLabel);
        JSON::ReadValue(a_document, "valueSource", a_setting.valueSource);
        JSON::ReadValue(a_document, "identityComplete", a_setting.identityComplete);
        JSON::ReadValue(a_document, "value", a_setting.value);
        
        a_setting.type = ParseControlType(controlType);

        return a_setting.type != ControlType::Unknown && !a_setting.selection.identity.modID.empty() && a_setting.selection.optionIndex >= 0 && !a_setting.value.is_null();
    }

    nlohmann::json ProfileStorage::ToJson(const Profile& a_profile)
    {
        // Keep selection fields flat and omit the temporary MCM list index.
        nlohmann::json document;
        document["formatVersion"] = 1;
        document["purpose"] = "Persistent MCM settings profile";
        document["settings"] = nlohmann::json::array();
        for (const auto& setting : a_profile) {
            document["settings"].push_back(JSON::ToJson(setting, false));
        }
        return document;
    }
}
