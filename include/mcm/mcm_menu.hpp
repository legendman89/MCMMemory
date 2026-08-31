#pragma once

#include "plugin.hpp"

namespace MCMMemory
{
    class MCMMenu
    {
        
    public:

        // Reads the useful fields from the currently open MCM menu.
        static nlohmann::json ReadState();

        // Reads one live row without moving the cursor. Call only from a queued game task.
        static nlohmann::json ReadOption(int a_optionIndex);

        // Converts one simple Scaleform value into a JSON value.
        static std::optional<nlohmann::json> ValueToJson(const RE::GFxValue& a_value);

    private:

        // Reads the menu paths listed in menu_fields.hpp.
        static void ReadFields(RE::GFxMovieView& a_movie, nlohmann::json& a_output);

        // Reads every member from one Scaleform object.
        static void ReadObjectMembers(RE::GFxMovieView& a_movie, std::string_view a_path, nlohmann::json& a_output);

    };

    class MenuMemberCollector final : public RE::GFxValue::ObjectVisitor
    {
    public:

        explicit MenuMemberCollector(nlohmann::json& a_output) : output(a_output) {}

        // Copies one simple Scaleform member into JSON.
        // Overrides ObjectVisitor::Visit.
        void Visit(const char* a_name, const RE::GFxValue& a_value) override
        {
            if (!a_name) {
                return;
            }

            auto converted = MCMMenu::ValueToJson(a_value);
            if (converted) {
                output[a_name] = std::move(*converted);
            }
        }

    private:

        // Receives the menu members found by Visit.
        nlohmann::json& output;
    };
}
