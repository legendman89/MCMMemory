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

    // Hasing functions for page comparison.
    inline constexpr uint64_t hashBasis{ 0xCBF29CE484222325ULL };
    inline constexpr uint64_t hashPrime{ 0x100000001B3ULL };

    inline void AddToHash(uint64_t& a_hash, uint64_t a_value)
    {
        a_hash = (a_hash ^ a_value) * hashPrime;
    }

    inline void AddTextToHash(uint64_t& a_hash, std::string_view a_text)
    {
        for (const auto character : a_text) {
            AddToHash(a_hash, static_cast<unsigned char>(character));
        }
    }

    // Lets a string-keyed map be looked up with a string_view, without building a temporary key.
    struct StringHash
    {
        using is_transparent = void;

        inline size_t operator()(std::string_view a_text) const
        {
            return std::hash<std::string_view>{}(a_text);
        }
    };

    inline unsigned char ToLowerASCII(unsigned char a_character)
    {
        if (a_character >= 'A' && a_character <= 'Z') {
            return static_cast<unsigned char>(a_character + ('a' - 'A'));
        }
        return a_character;
    }

    inline bool EqualsCaseInsensitive(std::string_view a_left, std::string_view a_right)
    {
        if (a_left.size() != a_right.size()) {
            return false;
        }
        for (size_t index = 0; index < a_left.size(); ++index) {
            if (ToLowerASCII(static_cast<unsigned char>(a_left[index])) != ToLowerASCII(static_cast<unsigned char>(a_right[index]))) {
                return false;
            }
        }
        return true;
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

    inline bool IsWordCharacter(char a_character)
    {
        const auto value = static_cast<unsigned char>(a_character);
        return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
    }

    // Matches a whole word only, so "Disable" does not match some non-activation control.
    inline bool ContainsCaseInsensitiveWord(std::string_view a_text, std::string_view a_search)
    {
        if (a_search.empty()) {
            return false;
        }
        for (size_t start = 0; start + a_search.size() <= a_text.size(); ++start) {
            if (start > 0 && IsWordCharacter(a_text[start - 1])) {
                continue;
            }
            const size_t end = start + a_search.size();
            if (end < a_text.size() && IsWordCharacter(a_text[end])) {
                continue;
            }
            if (EqualsCaseInsensitive(a_text.substr(start, a_search.size()), a_search)) {
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

    inline std::string GetDisplayText(std::string_view a_text)
    {
        std::string text{ a_text };
        if (!text.starts_with('$')) {
            return text;
        }

        std::string translatedText;
        if (SKSE::Translation::Translate(text, translatedText) && !translatedText.empty()) {
            text = std::move(translatedText);
        }
        if (text.starts_with('$')) {
            text.erase(0, 1);
        }
        return text;
    }

    inline std::string GetDisplayModName(std::string_view a_modName)
    {
        return GetDisplayText(a_modName);
    }

    inline bool IsJournalMenuOpen()
    {
        auto* ui = RE::UI::GetSingleton();
        return ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);
    }

    inline bool RequestJournalMenuClose()
    {
        auto* messages = RE::UIMessageQueue::GetSingleton();
        if (!messages) {
            return false;
        }
        messages->AddMessage(RE::JournalMenu::MENU_NAME.data(), RE::UI_MESSAGE_TYPE::kHide, nullptr);
        return true;
    }
}
