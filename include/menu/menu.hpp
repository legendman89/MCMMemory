#pragma once

#include "menu/ui.hpp"
#include "menu/color.hpp"

namespace MCMMemory::Menu
{
    inline constexpr float CTAButtonHorizontalPadding{ 14.0F };
    inline constexpr float CTAButtonVerticalPadding{ 6.0F };
    inline constexpr float IconButtonSpacing{ 8.0F };

    struct IconButtonMetrics
    {
        GUI::ImVec2 buttonSize;

        GUI::ImVec2 iconSize;

        GUI::ImVec2 labelSize;
    };

    inline void CenterNextWindow()
    {
        const auto* viewport = GUI::GetMainViewport();
        if (viewport) {
            const GUI::ImVec2 center{ viewport->Pos.x + viewport->Size.x * 0.5F, viewport->Pos.y + viewport->Size.y * 0.5F };
            GUI::SetNextWindowPos(center, GUI::ImGuiCond_Appearing, GUI::ImVec2{ 0.5F, 0.5F });
        }
    }

    inline void CenterNextItem(float a_width)
    {
        GUI::SetCursorPosX(GUI::GetCursorPosX() + std::max(0.0F, (GUI::GetContentRegionAvail().x - a_width) * 0.5F));
    }

    // We still need to use GUI::End() ourselves.
    inline bool BeginOpaqueWindow(const char* a_title, bool* a_open, GUI::ImGuiWindowFlags a_flags = GUI::ImGuiWindowFlags_NoCollapse)
    {
        GUI::PushStyleColor(GUI::ImGuiCol_WindowBg, Color::kOpaqueBackground);
        const bool visible = GUI::Begin(a_title, a_open, a_flags);
        GUI::PopStyleColor();
        return visible;
    }

    inline bool BeginOpaqueCombo(const char* a_label, const char* a_preview)
    {
        GUI::PushStyleColor(GUI::ImGuiCol_PopupBg, Color::kOpaqueBackground);
        const bool open = GUI::BeginCombo(a_label, a_preview);
        GUI::PopStyleColor();
        return open;
    }

    inline void HelpMarker(const char* a_text)
    {
        GUI::SameLine(0.0F, 6.0F);
        GUI::TextDisabled("(?)");
        GUI::WrappedTooltip(a_text);
    }

    inline void BoldTextColored(const GUI::ImVec4& a_color, const char* a_text)
    {
        GUI::TextColored(a_color, "%s", a_text);
        auto* drawList = GUI::GetWindowDrawList();
        auto* font = GUI::GetFont();
        if (drawList && font) {
            const auto position = GUI::GetItemRectMin();
            GUI::ImDrawListManager::AddText(drawList, font, GUI::GetFontSize(), GUI::ImVec2{ position.x + 1.0F, position.y }, GUI::GetColorU32(a_color), a_text);
        }
    }

    inline void ColoredText(const GUI::ImVec4& a_color, const char* a_text)
    {
        GUI::ColoredText({ a_color.x, a_color.y, a_color.z, a_color.w }, a_text);
    }

    template <class Colors>
    inline void PushButtonColors(const Colors& a_colors)
    {
        GUI::PushStyleColor(GUI::ImGuiCol_Button, a_colors.background);
        GUI::PushStyleColor(GUI::ImGuiCol_ButtonHovered, a_colors.hover);
        GUI::PushStyleColor(GUI::ImGuiCol_ButtonActive, a_colors.active);
        GUI::PushStyleColor(GUI::ImGuiCol_Text, a_colors.text);
    }

    inline IconButtonMetrics MeasureIconButton(const char* a_label, const uint32_t a_icon)
    {
        IconButtonMetrics metrics;
        metrics.labelSize = GUI::CalcTextSize(a_label, nullptr, false, -1.0F);

        const auto iconText = FontAwesome::UnicodeToUtf8(a_icon);
        FontAwesome::PushSolid();
        metrics.iconSize = GUI::CalcTextSize(iconText.c_str(), nullptr, false, -1.0F);
        FontAwesome::Pop();

        const float contentWidth = metrics.iconSize.x + IconButtonSpacing + metrics.labelSize.x;
        const float contentHeight = std::max(metrics.iconSize.y, metrics.labelSize.y);
        metrics.buttonSize = GUI::ImVec2{ contentWidth + CTAButtonHorizontalPadding * 2.0F, contentHeight + CTAButtonVerticalPadding * 2.0F };
        return metrics;
    }

    inline GUI::ImVec2 MeasureCTAButton(const char* a_label)
    {
        const auto size = GUI::MeasureButton(a_label);
        return GUI::ImVec2{ size.width, size.height };
    }

    template <class Colors>
    inline bool RenderIconButton(const char* a_label, const uint32_t a_icon, const Colors& a_colors, const bool a_enabled, const char* a_id, const GUI::ImVec2& a_size = {})
    {
        const auto iconText = FontAwesome::UnicodeToUtf8(a_icon);
        const auto* labelFont = GUI::GetFont();
        const float labelFontSize = GUI::GetFontSize();
        const auto metrics = MeasureIconButton(a_label, a_icon);

        FontAwesome::PushSolid();
        const auto* iconFont = GUI::GetFont();
        const float iconFontSize = GUI::GetFontSize();
        FontAwesome::Pop();

        const float contentWidth = metrics.iconSize.x + IconButtonSpacing + metrics.labelSize.x;
        const auto buttonID = std::format("##{}-{}", a_id, a_label);

        GUI::PushStyleVar(GUI::ImGuiStyleVar_FrameRounding, 6.0F);
        PushButtonColors(a_colors);

        if (!a_enabled) {
            GUI::BeginDisabled();
        }

        const GUI::ImVec2 buttonSize{ a_size.x > 0.0F ? a_size.x : metrics.buttonSize.x, a_size.y > 0.0F ? a_size.y : metrics.buttonSize.y };
        const bool clicked = GUI::Button(buttonID.c_str(), buttonSize);
        const GUI::ImVec2 buttonMin = GUI::GetItemRectMin();
        const GUI::ImVec2 buttonMax = GUI::GetItemRectMax();
        const float contentX = buttonMin.x + ((buttonMax.x - buttonMin.x - contentWidth) * 0.5F);
        const GUI::ImVec2 iconPosition{
            contentX,
            buttonMin.y + ((buttonMax.y - buttonMin.y - metrics.iconSize.y) * 0.5F)
        };
        const GUI::ImVec2 labelPosition{
            contentX + metrics.iconSize.x + IconButtonSpacing,
            buttonMin.y + ((buttonMax.y - buttonMin.y - metrics.labelSize.y) * 0.5F)
        };
        const auto contentColor = GUI::GetColorU32(a_colors.text);

        GUI::ImDrawListManager::AddText( GUI::GetWindowDrawList(), iconFont, iconFontSize, iconPosition, contentColor, iconText.c_str() );
        GUI::ImDrawListManager::AddText( GUI::GetWindowDrawList(), labelFont, labelFontSize, labelPosition, contentColor, a_label );

        if (!a_enabled) {
            GUI::EndDisabled();
        }

        GUI::PopStyleColor(4);
        GUI::PopStyleVar();
        return clicked && a_enabled;
    }

    template <class Colors>
    inline GUI::ButtonColors ToButtonColors(const Colors& a_colors)
    {
        return {
            { a_colors.background.x, a_colors.background.y, a_colors.background.z, a_colors.background.w },
            { a_colors.hover.x, a_colors.hover.y, a_colors.hover.z, a_colors.hover.w },
            { a_colors.active.x, a_colors.active.y, a_colors.active.z, a_colors.active.w },
            { a_colors.text.x, a_colors.text.y, a_colors.text.z, a_colors.text.w }
        };
    }

    inline bool CTAButton(const char* a_label, const bool a_enabled, const Color::CTAColors& a_colors, const GUI::ImVec2& a_size = {})
    {
        const auto& colors = a_enabled ? a_colors : Color::kDisabledButtonColors;
        return GUI::ColoredButton(a_label, ToButtonColors(colors), a_enabled, a_size.x, a_size.y);
    }

    inline bool IconCTAButton(const char* a_label, const bool a_enabled, const uint32_t a_icon, const Color::CTAColors& a_colors, const GUI::ImVec2& a_size = {})
    {
        const auto& colors = a_enabled ? a_colors : Color::kDisabledButtonColors;
        return GUI::IconButton(a_label, a_icon, ToButtonColors(colors), a_enabled, "CTAButton", a_size.x, a_size.y);
    }

    inline bool IconButton(const char* a_label, const uint32_t a_icon, const Color::ButtonColors& a_colors)
    {
        return GUI::IconButton(a_label, a_icon, ToButtonColors(a_colors), true, "IconButton");
    }

}
