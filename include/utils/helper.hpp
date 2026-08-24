#pragma once

#include "plugin.hpp"

namespace MCMMemory
{
    inline std::string ToUTF8(const std::filesystem::path& a_path)
    {
        auto utf8 = a_path.u8string();
        return std::string(reinterpret_cast<const char*>(utf8.c_str()));
    }

    inline std::string GetDisplayModName(std::string_view a_modName)
    {
        std::string modName{ a_modName };
        if (!modName.starts_with('$')) {
            return modName;
        }

        std::string translatedName;
        if (SKSE::Translation::Translate(modName, translatedName) && !translatedName.empty()) {
            modName = std::move(translatedName);
        }
        if (modName.starts_with('$')) {
            modName.erase(0, 1);
        }
        return modName;
    }
}
