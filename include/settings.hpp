#pragma once

#include "plugin.hpp"
#include "profile/types.hpp"
#include "settings_defs.hpp"

namespace MCMMemory
{
    struct Settings
    {
        MCMFilter autoRestoreExcludedMCMs;

#define DECLARE_SETTING(type, name, defaultValue, ...) type name{ defaultValue };
        FOREACH_SETTING(DECLARE_SETTING)
#undef DECLARE_SETTING

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
