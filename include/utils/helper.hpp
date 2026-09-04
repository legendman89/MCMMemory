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

    inline bool ContainsCaseInsensitiveWordStart(std::string_view a_text, std::string_view a_search)
    {
        for (size_t start = 0; start + a_search.size() <= a_text.size(); ++start) {
            if (start > 0) {
                const auto previous = static_cast<unsigned char>(a_text[start - 1]);
                if ((previous >= 'A' && previous <= 'Z') || (previous >= 'a' && previous <= 'z') || (previous >= '0' && previous <= '9')) {
                    continue;
                }
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
