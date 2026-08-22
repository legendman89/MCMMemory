#include "settings.hpp"
#include "helper.hpp"
#include "json.hpp"

namespace MCMMemory
{
    bool SettingsStorage::Save()
    {
        nlohmann::json document;
#define WRITE_SETTING(type, name, defaultValue) document[#name] = GetSettings().name;
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
#define READ_SETTING(type, name, defaultValue) JSON::ReadValue(document, #name, settings.name);
            FOREACH_SETTING(READ_SETTING)
#undef READ_SETTING
        }
        catch (const std::exception& exception) {
            logger::error("Failed to read settings file {}: {}", ToUTF8(Path()), exception.what());
            return false;
        }

        if (settings.actionTrialDelaySeconds < 0.05F || settings.actionTrialDelaySeconds > 10.0F) {
            logger::error("actionTrialDelaySeconds {} is outside the supported range 0.05 through 10.0", settings.actionTrialDelaySeconds);
            return false;
        }

        GetSettings() = settings;
        logger::info("Loaded settings: autoBackup={}, autoRestore={}, notifications={}, actionTrialDelaySeconds={}, captureRawRecords={}", settings.autoBackup, settings.autoRestore, settings.notifications, settings.actionTrialDelaySeconds, settings.captureRawRecords);
        return true;
    }
}
