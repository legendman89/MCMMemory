#include "mcm_menu.hpp"

#include "menu_fields.hpp"

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
