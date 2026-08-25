#pragma once

#include "profile/profile.hpp"

namespace MCMMemory
{
    class Profiles
    {
    public:

        static std::vector<std::string> ReadNames();

        static bool Create(std::string_view a_name, std::string_view a_source, std::string& a_error);

        static bool Delete(std::string_view a_name, std::string& a_error);

        static bool Select(std::string_view a_name, std::string& a_error);

        static bool CheckSelection();

        static bool IsValidName(std::string_view a_name);

        static inline std::filesystem::path Directory() { return GetPluginDataPath() / "Profiles"; }

    private:

        static bool IsReservedName(std::string_view a_name);
    };
}
