#include "profile/profile.hpp"
#include "profile/profiles.hpp"
#include "settings.hpp"
#include "utils/helper.hpp"
#include "utils/json.hpp"

namespace MCMMemory
{
    bool ProfileStorage::Load(Profile& a_profile)
    {
        return Load(GetSettings().activeProfile, a_profile);
    }

    bool ProfileStorage::Load(std::string_view a_name, Profile& a_profile)
    {
        a_profile.clear();
        const auto path = Path(a_name);
        std::ifstream stream(path);
        if (!stream) {
            return false;
        }

        try {
            auto document = JSON::DecodeDocumentText(nlohmann::json::parse(stream));
            if (!document.is_object()) {
                logger::error("Profile root is not an object in {}", ToUTF8(path));
                return false;
            }
            auto settings = document.find("settings");
            if (settings == document.end() || !settings->is_array()) {
                logger::error("Profile settings are missing or invalid in {}", ToUTF8(path));
                return false;
            }
            for (const auto& settingDocument : *settings) {
                CapturedSetting setting;
                if (FromJson(settingDocument, setting) && setting.identityComplete) {
                    a_profile.push_back(std::move(setting));
                }
            }
        } catch (const std::exception& error) {
            logger::error("Failed to read profile {}: {}", ToUTF8(path), error.what());
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
        return Save(GetSettings().activeProfile, a_profile);
    }

    bool ProfileStorage::Save(std::string_view a_name, const Profile& a_profile)
    {
        const auto path = Path(a_name);
        if (!JSON::WriteFile(path, ToJson(a_profile))) {
            return false;
        }
        logger::info("Saved {} persistent profile settings to {}", a_profile.size(), ToUTF8(path));
        return true;
    }

    std::filesystem::path ProfileStorage::Path()
    {
        return Path(GetSettings().activeProfile);
    }

    std::filesystem::path ProfileStorage::Path(std::string_view a_name)
    {
        const auto name = Profiles::IsValidName(a_name) ? a_name : std::string_view{ "Default" };
        return Profiles::Directory() / FromUTF8(std::format("{}.json", name));
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
        JSON::ReadValue(a_document, "settingID", a_setting.settingID);
        JSON::ReadValue(a_document, "stateName", a_setting.stateName);
        JSON::ReadValue(a_document, "pageScopedState", a_setting.pageScopedState);
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
