#pragma once

#include "menu/ui.hpp"
#include "menu/color.hpp"

namespace MCMMemory::Menu
{

    inline void WrappedTooltip(const char* a_text, const float& a_width = 420.0F)
    {
        if (GUI::IsItemHovered()) {
            GUI::BeginTooltip();
            GUI::PushTextWrapPos(a_width);
            GUI::TextUnformatted(a_text);
            GUI::PopTextWrapPos();
            GUI::EndTooltip();
        }
    }

    template <class Colors>
    inline void PushButtonColors(const Colors& a_colors)
    {
        GUI::PushStyleColor(GUI::ImGuiCol_Button, a_colors.background);
        GUI::PushStyleColor(GUI::ImGuiCol_ButtonHovered, a_colors.hover);
        GUI::PushStyleColor(GUI::ImGuiCol_ButtonActive, a_colors.active);
        GUI::PushStyleColor(GUI::ImGuiCol_Text, a_colors.text);
    }

    template <class Colors>
    inline bool RenderIconButton(const char* a_label, const unsigned a_icon, const Colors& a_colors, const bool a_enabled, const char* a_id)
    {
        constexpr float horizontalPadding{ 14.0F };
        constexpr float verticalPadding{ 6.0F };
        constexpr float iconSpacing{ 8.0F };

        const auto iconText = FontAwesome::UnicodeToUtf8(a_icon);
        const auto* labelFont = GUI::GetFont();
        const float labelFontSize = GUI::GetFontSize();
        const GUI::ImVec2 labelSize = GUI::CalcTextSize(a_label, nullptr, false, -1.0F);

        FontAwesome::PushSolid();
        const auto* iconFont = GUI::GetFont();
        const float iconFontSize = GUI::GetFontSize();
        const GUI::ImVec2 iconSize = GUI::CalcTextSize(iconText.c_str(), nullptr, false, -1.0F);
        FontAwesome::Pop();

        const float contentWidth = iconSize.x + iconSpacing + labelSize.x;
        const float contentHeight = std::max(iconSize.y, labelSize.y);
        const GUI::ImVec2 buttonSize{ contentWidth + horizontalPadding * 2.0F, contentHeight + verticalPadding * 2.0F };
        const auto buttonID = std::format("##{}-{}", a_id, a_label);

        GUI::PushStyleVar(GUI::ImGuiStyleVar_FrameRounding, 6.0F);
        PushButtonColors(a_colors);

        if (!a_enabled) {
            GUI::BeginDisabled();
        }

        const bool clicked = GUI::Button(buttonID.c_str(), buttonSize);
        const GUI::ImVec2 buttonMin = GUI::GetItemRectMin();
        const GUI::ImVec2 buttonMax = GUI::GetItemRectMax();
        const float contentX = buttonMin.x + ((buttonMax.x - buttonMin.x - contentWidth) * 0.5F);
        const GUI::ImVec2 iconPosition{
            contentX,
            buttonMin.y + ((buttonMax.y - buttonMin.y - iconSize.y) * 0.5F)
        };
        const GUI::ImVec2 labelPosition{
            contentX + iconSize.x + iconSpacing,
            buttonMin.y + ((buttonMax.y - buttonMin.y - labelSize.y) * 0.5F)
        };
        const auto contentColor = GUI::GetColorU32(a_colors.text);

        GUI::ImDrawListManager::AddText(
            GUI::GetWindowDrawList(),
            iconFont,
            iconFontSize,
            iconPosition,
            contentColor,
            iconText.c_str()
        );
        GUI::ImDrawListManager::AddText(
            GUI::GetWindowDrawList(),
            labelFont,
            labelFontSize,
            labelPosition,
            contentColor,
            a_label
        );

        if (!a_enabled) {
            GUI::EndDisabled();
        }

        GUI::PopStyleColor(4);
        GUI::PopStyleVar();
        return clicked && a_enabled;
    }

    inline bool CTAButton(const char* a_label, const bool a_enabled, const Color::CTAColors& a_colors)
    {
        const auto& colors = a_enabled ? a_colors : Color::kDisabledButtonColors;
        GUI::PushStyleVar(GUI::ImGuiStyleVar_FrameRounding, 6.0F);
        GUI::PushStyleVar(GUI::ImGuiStyleVar_FramePadding, GUI::ImVec2{ 14.0F, 6.0F });
        PushButtonColors(colors);

        if (!a_enabled) {
            GUI::BeginDisabled();
        }

        const bool clicked = GUI::Button(a_label);

        if (!a_enabled) {
            GUI::EndDisabled();
        }

        GUI::PopStyleColor(4);
        GUI::PopStyleVar(2);
        return clicked && a_enabled;
    }

    inline bool IconCTAButton(const char* a_label, const bool a_enabled, const unsigned a_icon, const Color::CTAColors& a_colors)
    {
        const auto& colors = a_enabled ? a_colors : Color::kDisabledButtonColors;
        return RenderIconButton(a_label, a_icon, colors, a_enabled, "CTAButton");
    }

    inline bool IconButton(const char* a_label, const unsigned a_icon, const Color::ButtonColors& a_colors)
    {
        return RenderIconButton(a_label, a_icon, a_colors, true, "IconButton");
    }

    void Register();

    void __stdcall RenderProfile();

}
