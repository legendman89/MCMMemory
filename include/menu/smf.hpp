#pragma once

#include "menu/menu.hpp"

namespace MCMMemory::GUI::SMF
{
    using namespace ImGuiMCP;

    inline constexpr PushIDStringFunction PushIDString = ImGuiMCP::PushID;
    inline constexpr PushIDIntFunction PushIDInt = ImGuiMCP::PushID;
    inline constexpr EndWindowFunction EndWindow = ImGuiMCP::End;
    inline constexpr RadioButtonFunction RadioButton = ImGuiMCP::RadioButton;

    inline bool BeginWindow(const char* a_title, bool* a_open, float a_width, float a_height)
    {
        ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{ a_width, a_height }, ImGuiMCP::ImGuiCond_FirstUseEver);
        Menu::CenterNextWindow();
        return ImGuiMCP::Begin(a_title, a_open, ImGuiMCP::ImGuiWindowFlags_NoCollapse);
    }

    inline TextSize MeasureText(const char* a_text)
    {
        const auto size = ImGuiMCP::CalcTextSize(a_text);
        return { size.x, size.y };
    }

    inline TextSize MeasureButton(const char* a_label, float a_width, float a_height)
    {
        const auto size = ImGuiMCP::CalcTextSize(a_label, nullptr, true);
        return { a_width > 0.0F ? a_width : size.x + Menu::CTAButtonHorizontalPadding * 2.0F, a_height > 0.0F ? a_height : size.y + Menu::CTAButtonVerticalPadding * 2.0F };
    }

    inline void CenterNextItem(float a_width)
    {
        Menu::CenterNextItem(a_width);
    }

    inline void HelpMarker(const char* a_text)
    {
        Menu::HelpMarker(a_text);
    }

    inline void WrappedTooltip(const char* a_text, float a_width)
    {
        if (ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Menu::Color::kOpaqueBackground);
            if (ImGuiMCP::BeginTooltip()) {
                ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetCursorPosX() + a_width);
                ImGuiMCP::TextUnformatted(a_text);
                ImGuiMCP::PopTextWrapPos();
                ImGuiMCP::EndTooltip();
            }
            ImGuiMCP::PopStyleColor();
        }
    }

    inline bool Button(const char* a_label, float a_width, float a_height)
    {
        return ImGuiMCP::Button(a_label, ImGuiMCP::ImVec2{ a_width, a_height });
    }

    inline bool Combo(const char* a_label, int* a_selected, const char* const* a_items, int a_count)
    {
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_PopupBg, Menu::Color::kOpaqueBackground);
        const bool changed = ImGuiMCP::Combo(a_label, a_selected, a_items, a_count);
        ImGuiMCP::PopStyleColor();
        return changed;
    }

    inline ImGuiMCP::ImVec4 ToColor(const Color4& a_color)
    {
        return { a_color.r, a_color.g, a_color.b, a_color.a };
    }

    inline void BoldTextColored(const Color4& a_color, const char* a_text)
    {
        Menu::BoldTextColored(ToColor(a_color), a_text);
    }

    inline bool ColoredButton(const char* a_label, const ButtonColors& a_colors, bool a_enabled, float a_width, float a_height)
    {
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FrameRounding, 6.0F);
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImGuiMCP::ImVec2{ Menu::CTAButtonHorizontalPadding, Menu::CTAButtonVerticalPadding });
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, ToColor(a_colors.background));
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonHovered, ToColor(a_colors.hover));
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ButtonActive, ToColor(a_colors.active));
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ToColor(a_colors.text));
        ImGuiMCP::BeginDisabled(!a_enabled);
        const bool clicked = ImGuiMCP::Button(a_label, ImGuiMCP::ImVec2{ a_width, a_height });
        ImGuiMCP::EndDisabled();
        ImGuiMCP::PopStyleColor(4);
        ImGuiMCP::PopStyleVar(2);
        return clicked && a_enabled;
    }

    inline bool IconButton(const char* a_label, uint32_t a_icon, const ButtonColors& a_colors, bool a_enabled, const char* a_id, float a_width, float a_height)
    {
        const Menu::Color::ButtonColors colors{ ToColor(a_colors.background), ToColor(a_colors.hover), ToColor(a_colors.active), ToColor(a_colors.text) };
        return Menu::RenderIconButton(a_label, a_icon, colors, a_enabled, a_id, ImGuiMCP::ImVec2{ a_width, a_height });
    }

    inline bool InputText(const char* a_label, char* a_buffer, size_t a_size)
    {
        return ImGuiMCP::InputText(a_label, a_buffer, a_size);
    }

    inline bool SliderFloat(const char* a_label, float* a_value, float a_min, float a_max, const char* a_format)
    {
        return ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);
    }

    inline bool SliderInt(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format)
    {
        return ImGuiMCP::SliderInt(a_label, a_value, a_min, a_max, a_format);
    }
}
