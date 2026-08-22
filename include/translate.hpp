#pragma once

#include "logger.hpp"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>

namespace MCMMemory::Trans
{
    inline std::filesystem::path GetTranslationPath()
    {
        return GetPluginDataPath() / "Translation" / "Translation.json";
    }

    class Translator
    {
    public:

        bool Load();

        const std::string& Get(std::string_view a_key);

    private:

        std::unordered_map<std::string, std::string> table;

        std::unordered_map<std::string, std::string> missing;
    };

    inline Translator& GetTranslator()
    {
        static Translator translator;
        return translator;
    }

    inline const std::string& Tr(std::string_view a_key)
    {
        return GetTranslator().Get(a_key);
    }

    template <class... Args>
    std::string Format(std::string_view a_key, Args&&... a_args)
    {
        try {
            return std::vformat(Tr(a_key), std::make_format_args(a_args...));
        }
        catch (const std::format_error& error) {
            logger::error("Translation '{}' could not be formatted: {}", a_key, error.what());
            return Tr(a_key);
        }
    }
}
