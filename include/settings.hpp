#pragma once

#include "plugin.hpp"
#include "settings_defs.hpp"

namespace MCMMemory
{
    struct Settings
    {
#define DECLARE_SETTING(type, name, defaultValue) type name{ defaultValue };
        FOREACH_SETTING(DECLARE_SETTING)
#undef DECLARE_SETTING
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

        static inline std::filesystem::path Path() { return GetPluginDataPath() / "Settings.json"; }
    };
}
