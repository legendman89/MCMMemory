#pragma once

#include "plugin.hpp"
#include "profile/types.hpp"
#include "settings_defs.hpp"

namespace MCMMemory
{
    struct Settings
    {
        // Chooses the profile file used by capture, backup and restore.
        std::string activeProfile{ "Default" };

        MCMFilter autoRestoreExcludedMCMs;

#define DECLARE_SETTING(type, name, defaultValue, ...) type name{ defaultValue };
        FOREACH_SETTING(DECLARE_SETTING)
#undef DECLARE_SETTING

        inline bool AreNotificationSettingsDefault() const
        {
            const Settings defaults;
#define CHECK_NOTIFICATION_SETTING(type, name, defaultValue, ...) \
            if (name != defaults.name) { \
                return false; \
            }
            FOREACH_NOTIFICATION_TOGGLE_SETTING(CHECK_NOTIFICATION_SETTING)
            FOREACH_HUD_SETTING(CHECK_NOTIFICATION_SETTING)
#undef CHECK_NOTIFICATION_SETTING
            return true;
        }

        inline void ResetNotificationSettings()
        {
            const Settings defaults;
#define RESET_NOTIFICATION_SETTING(type, name, defaultValue, ...) name = defaults.name;
            FOREACH_NOTIFICATION_TOGGLE_SETTING(RESET_NOTIFICATION_SETTING)
            FOREACH_HUD_SETTING(RESET_NOTIFICATION_SETTING)
#undef RESET_NOTIFICATION_SETTING
        }

        inline bool IsAutoRestoreEnabled(std::string_view a_modID) const
        {
            return !ContainsMCMID(autoRestoreExcludedMCMs, a_modID);
        }

        inline void SetAutoRestoreEnabled(std::string_view a_modID, bool a_enabled)
        {
            auto excluded = autoRestoreExcludedMCMs.begin();
            for (; excluded != autoRestoreExcludedMCMs.end() && *excluded != a_modID; ++excluded) {}
            if (a_enabled && excluded != autoRestoreExcludedMCMs.end()) {
                autoRestoreExcludedMCMs.erase(excluded);
            }
            else if (!a_enabled && excluded == autoRestoreExcludedMCMs.end()) {
                autoRestoreExcludedMCMs.emplace_back(a_modID);
                std::sort(autoRestoreExcludedMCMs.begin(), autoRestoreExcludedMCMs.end());
            }
        }
    };

    inline Settings& GetSettings()
    {
        static Settings settings;
        return settings;
    }

    class SettingsStorage
    {
    public:

        static bool Load();

        static bool Save();

        static inline std::filesystem::path Path() { return GetPluginDataPath() / "Settings.json"; }
    };
}
