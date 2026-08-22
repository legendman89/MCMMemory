#include "translate.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace MCMMemory::Trans
{
    bool Translator::Load()
    {
        const auto path = GetTranslationPath();
        table.clear();
        missing.clear();

        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (error) {
            logger::error("Failed to check translation file {}: {}", ToUTF8(path), error.message());
            return false;
        }
        if (!exists) {
            logger::info("Translation file does not exist at {}; using built-in text", ToUTF8(path));
            return true;
        }

        std::ifstream input(path);
        if (!input) {
            logger::error("Translation file could not be opened at {}", ToUTF8(path));
            return false;
        }

        try {
            const auto document = nlohmann::json::parse(input, nullptr, true, true);
            if (!document.is_object()) {
                logger::error("Translation file root is not an object at {}", ToUTF8(path));
                return false;
            }
            for (const auto& [key, value] : document.items()) {
                if (value.is_string()) {
                    table.emplace(key, value.get<std::string>());
                }
            }
        }
        catch (const std::exception& exception) {
            logger::error("Failed to read translation file {}: {}", ToUTF8(path), exception.what());
            table.clear();
            return false;
        }

        logger::info("Loaded {} MCM Memory translations", table.size());
        return true;
    }

    const std::string& Translator::Get(std::string_view a_key)
    {
        const auto found = table.find(std::string(a_key));
        if (found != table.end()) {
            return found->second;
        }
        return missing.try_emplace(std::string(a_key), a_key).first->second;
    }
}
