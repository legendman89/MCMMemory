#include "settings.hpp"
#include "utils/helper.hpp"
#include "utils/json.hpp"

namespace MCMMemory
{
    bool SettingsStorage::Save()
    {
        nlohmann::json document;
        document["activeProfile"] = GetSettings().activeProfile;
        document["autoRestoreExcludedMCMs"] = GetSettings().autoRestoreExcludedMCMs;
#define WRITE_SETTING(type, name, defaultValue, ...) document[#name] = GetSettings().name;
        FOREACH_SETTING(WRITE_SETTING)
#undef WRITE_SETTING

        return JSON::WriteFile(Path(), document);
    }

    bool SettingsStorage::Load()
    {
        Settings settings;
        std::error_code error;
        bool exists = std::filesystem::exists(Path(), error);
        if (error) {
            logger::error("Failed to check settings file {}: {}", ToUTF8(Path()), error.message());
            return false;
        }
        if (!exists) {
            GetSettings() = settings;
            logger::info("Settings file does not exist at {}; using defaults", ToUTF8(Path()));
            return true;
        }

        std::ifstream stream(Path());
        if (!stream) {
            logger::error("Failed to open settings file {}", ToUTF8(Path()));
            return false;
        }

        try {
            auto document = nlohmann::json::parse(stream);
            JSON::ReadValue(document, "activeProfile", settings.activeProfile);
            JSON::ReadValue(document, "autoRestoreExcludedMCMs", settings.autoRestoreExcludedMCMs);
#define READ_SETTING(type, name, defaultValue, ...) JSON::ReadValue(document, #name, settings.name);
            FOREACH_SETTING(READ_SETTING)
#undef READ_SETTING
        }
        catch (const std::exception& exception) {
            logger::error("Failed to read settings file {}: {}", ToUTF8(Path()), exception.what());
            return false;
        }

        auto excluded = settings.autoRestoreExcludedMCMs.begin();
        while (excluded != settings.autoRestoreExcludedMCMs.end()) {
            if (excluded->empty()) {
                excluded = settings.autoRestoreExcludedMCMs.erase(excluded);
            }
            else {
                ++excluded;
            }
        }
        std::sort(settings.autoRestoreExcludedMCMs.begin(), settings.autoRestoreExcludedMCMs.end());
        settings.autoRestoreExcludedMCMs.erase(std::unique(settings.autoRestoreExcludedMCMs.begin(), settings.autoRestoreExcludedMCMs.end()), settings.autoRestoreExcludedMCMs.end());

        if (settings.actionTrialDelaySeconds < 0.05F || settings.actionTrialDelaySeconds > 10.0F) {
            logger::error("actionTrialDelaySeconds {} is outside the supported range 0.05 through 10.0", settings.actionTrialDelaySeconds);
            return false;
        }
#define VALIDATE_HUD_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
        if (settings.settingName < minimum || settings.settingName > maximum) { \
            logger::error(#settingName " {} is outside the supported range {} through {}", settings.settingName, minimum, maximum); \
            return false; \
        }
        FOREACH_HUD_SETTING(VALIDATE_HUD_SETTING)
#undef VALIDATE_HUD_SETTING

        GetSettings() = settings;

#define LOG_FORMATTER(type, name, defaultValue, ...) " {:<30s} : {}\n"
#define LOG_SETTING(type, name, defaultValue, ...) , #name, settings.name
        logger::info(
            "Loaded Settings:\n"
            FOREACH_SETTING(LOG_FORMATTER)
            FOREACH_SETTING(LOG_SETTING)
        );
#undef LOG_SETTING
#undef LOG_FORMATTER
        logger::info(" {:<30s} : {}", "Auto restore exclusions", settings.autoRestoreExcludedMCMs.size());
        logger::info(" {:<30s} : {}", "Active profile", settings.activeProfile);

        return true;
    }
}
