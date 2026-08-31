#include "mcm/mcm_menu.hpp"

#include "mcm/menu_fields.hpp"

namespace MCMMemory
{

    nlohmann::json MCMMenu::ReadState()
    {
        nlohmann::json output = nlohmann::json::object();
        auto ui = RE::UI::GetSingleton();
        if (!ui) {
            output["error"] = "UI singleton unavailable";
            return output;
        }

        auto movie = ui->GetMovieView(RE::JournalMenu::MENU_NAME);
        if (!movie) {
            output["error"] = "Journal Menu movie unavailable";
            return output;
        }

        ReadFields(*movie, output["fields"]);
        ReadObjectMembers(*movie, "_root.ConfigPanelFader.configPanel", output["panelMembers"]);
        ReadObjectMembers(*movie, "_root.ConfigPanelFader.configPanel.optionCursor", output["optionCursorMembers"]);
        return output;
    }

    nlohmann::json MCMMenu::ReadOption(int a_optionIndex)
    {
        auto output = nlohmann::json::object();
        auto* ui = RE::UI::GetSingleton();
        if (a_optionIndex < 0 || !ui || !ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
            return output;
        }

        auto movie = ui->GetMovieView(RE::JournalMenu::MENU_NAME);
        if (!movie) {
            return output;
        }

        RE::GFxValue entries;
        RE::GFxValue option;
        if (movie->GetVariable(std::addressof(entries), "_root.ConfigPanelFader.configPanel._optionsList.entryList") && entries.IsArray()) {
            const auto index = static_cast<uint32_t>(a_optionIndex);
            if (index >= entries.GetArraySize() || !entries.GetElement(index, std::addressof(option))) {
                return output;
            }
        }
        else {
            // A replacement menu may expose only the public cursor, which must still name this row.
            RE::GFxValue cursorIndex;
            if (!movie->GetVariable(std::addressof(cursorIndex), "_root.ConfigPanelFader.configPanel.optionCursorIndex") || !cursorIndex.IsNumber() || cursorIndex.GetNumber() != a_optionIndex) {
                return output;
            }
            if (!movie->GetVariable(std::addressof(option), "_root.ConfigPanelFader.configPanel.optionCursor")) {
                return output;
            }
        }

        if (option.IsObject()) {
            MenuMemberCollector collector(output);
            option.VisitMembers(std::addressof(collector));
        }
        return output;
    }

    std::optional<nlohmann::json> MCMMenu::ValueToJson(const RE::GFxValue& a_value)
    {
        if (a_value.IsBool()) {
            return a_value.GetBool();
        }
        if (a_value.IsNumber()) {
            return a_value.GetNumber();
        }
        if (a_value.IsString()) {
            return std::string(a_value.GetString());
        }
        if (a_value.IsNull()) {
            return nullptr;
        }
        return std::nullopt;
    }

    void MCMMenu::ReadFields(RE::GFxMovieView& a_movie, nlohmann::json& a_output)
    {
        a_output = nlohmann::json::object();
#define READ_MENU_FIELD(name, path) \
        { \
            RE::GFxValue value; \
            if (a_movie.GetVariable(std::addressof(value), path)) { \
                auto converted = ValueToJson(value); \
                if (converted) { a_output[#name] = std::move(*converted); } \
            } \
        }
        FOREACH_MENU_FIELD(READ_MENU_FIELD)
#undef READ_MENU_FIELD
    }

    void MCMMenu::ReadObjectMembers(RE::GFxMovieView& a_movie, std::string_view a_path, nlohmann::json& a_output)
    {
        a_output = nlohmann::json::object();
        RE::GFxValue object;
        if (!a_movie.GetVariable(std::addressof(object), a_path.data()) || !object.IsObject()) {
            return;
        }

        MenuMemberCollector collector(a_output);
        object.VisitMembers(std::addressof(collector));
    }

}
