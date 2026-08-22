#pragma once

#include "plugin.hpp"

namespace MCMMemory
{
    inline std::string ToUTF8(const std::filesystem::path& a_path)
    {
        auto utf8 = a_path.u8string();
        return std::string(reinterpret_cast<const char*>(utf8.c_str()));
    }
}
