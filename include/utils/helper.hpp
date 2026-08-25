#pragma once

#include "plugin.hpp"

#include <cstddef>
#include <type_traits>

namespace MCMMemory
{
    template <class Enum> requires std::is_enum_v<Enum>
    inline constexpr size_t ToIndex(Enum a_value) noexcept
    {
        return static_cast<size_t>(a_value);
    }

    inline unsigned char ToLowerASCII(unsigned char a_character)
    {
        if (a_character >= 'A' && a_character <= 'Z') {
            return static_cast<unsigned char>(a_character + ('a' - 'A'));
        }
        return a_character;
    }

    inline bool ContainsCaseInsensitive(std::string_view a_text, std::string_view a_search)
    {
        if (a_search.empty()) {
            return true;
        }
        if (a_search.size() > a_text.size()) {
            return false;
        }
        for (size_t start = 0; start + a_search.size() <= a_text.size(); ++start) {
            size_t index{};
            for (; index < a_search.size(); ++index) {
                const auto textCharacter = static_cast<unsigned char>(a_text[start + index]);
                const auto searchCharacter = static_cast<unsigned char>(a_search[index]);
                if (ToLowerASCII(textCharacter) != ToLowerASCII(searchCharacter)) {
                    break;
                }
            }
            if (index == a_search.size()) {
                return true;
            }
        }
        return false;
    }

    inline std::string ToUTF8(const std::filesystem::path& a_path)
    {
        auto utf8 = a_path.u8string();
        return std::string(reinterpret_cast<const char*>(utf8.c_str()));
    }

    inline std::filesystem::path FromUTF8(std::string_view a_text)
    {
        const auto* first = reinterpret_cast<const char8_t*>(a_text.data());
        return std::filesystem::path(std::u8string(first, first + a_text.size()));
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
