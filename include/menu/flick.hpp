#pragma once

#include "plugin.hpp"
#include "menu/backend.hpp"
#include "menu/API/FUCK_API.h"
#include "utils/helper.hpp"

namespace MCMMemory::GUI::FLICK
{
    using namespace FUCK;

    inline constexpr PushIDStringFunction PushIDString = FUCK::PushID;
    inline constexpr PushIDIntFunction PushIDInt = FUCK::PushID;

    inline bool Checkbox(const char* a_label, bool* a_value)
    {
        return FUCK::Checkbox(a_label, a_value, false, false);
    }

    inline bool RadioButton(const char* a_label, bool a_selected)
    {
        return FUCK::Checkbox(a_label, &a_selected, false, false) && a_selected;
    }

    inline bool BeginWindow(const char* a_title, bool* a_open, float a_width, float a_height)
    {
        const auto displaySize = FUCK::GetDisplaySize();
        FUCK::SetNextWindowSize(ImVec2{ a_width, a_height }, ImGuiCond_FirstUseEver);
        FUCK::SetNextWindowPos(ImVec2{ displaySize.x * 0.5F, displaySize.y * 0.5F }, ImGuiCond_Appearing, ImVec2{ 0.5F, 0.5F });
        return FUCK::BeginWindow(a_title, a_open, ImGuiWindowFlags_NoCollapse);
    }

    inline TextSize MeasureText(const char* a_text)
    {
        const auto size = FUCK::CalcTextSize(a_text);
        return { size.x, size.y };
    }

    inline TextSize MeasureButton(const char* a_label, float, float)
    {
        const auto size = FUCK::CalcTextSize(a_label);
        const float scale = FUCK::GetTextLineHeight() / 30.0F;
        return { size.x + 16.0F * scale, std::max(FUCK::GetFrameHeight(), size.y + 14.0F * scale) };
    }

    inline void CenterNextItem(float a_width)
    {
        FUCK::SetCursorPosX(FUCK::GetCursorPos().x + std::max(0.0F, (FUCK::GetContentRegionAvail().x - a_width) * 0.5F));
    }

    inline bool Button(const char* a_label, float, float)
    {
        return FUCK::Button(a_label);
    }

    inline void Spacing()
    {
        FUCK::Spacing();
    }

    inline void HelpMarker(const char* a_text)
    {
        const auto itemMin = FUCK::GetItemRectMin();
        const auto itemSize = FUCK::GetItemRectSize();
        const auto textSize = FUCK::CalcTextSize("(?)");
        FUCK::SameLine(0.0F, 0.0F);
        const auto position = FUCK::GetCursorScreenPos();
        FUCK::SetCursorScreenPos(ImVec2{ position.x, itemMin.y + std::max(0.0F, (itemSize.y - textSize.y) * 0.5F) });
        FUCK::TextDisabled("(?)");
        GUI::WrappedTooltip(a_text);
    }

    inline void WrappedTooltip(const char* a_text, float a_width)
    {
        if (FUCK::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && FUCK::BeginTooltip()) {
            FUCK::PushTextWrapPos(FUCK::GetCursorPos().x + a_width);
            FUCK::TextUnformatted(a_text);
            FUCK::PopTextWrapPos();
            FUCK::EndTooltip();
        }
    }

    inline float GetCursorPosX()
    {
        return FUCK::GetCursorPos().x;
    }

    inline bool Combo(const char* a_label, int* a_selected, const char* const* a_items, int a_count)
    {
        return FUCK::Combo(a_label, a_selected, a_items, a_count);
    }

    inline ImVec4 ToColor(const Color4& a_color)
    {
        return { a_color.r, a_color.g, a_color.b, a_color.a };
    }

    inline void BoldTextColored(const Color4& a_color, const char* a_text)
    {
        FUCK::TextColored(ToColor(a_color), "%s", a_text);
    }

    inline bool ColoredButton(const char* a_label, const ButtonColors& a_colors, bool a_enabled, float, float)
    {
        FUCK::PushStyleColor(ImGuiCol_Button, ToColor(a_colors.background));
        FUCK::PushStyleColor(ImGuiCol_ButtonHovered, ToColor(a_colors.hover));
        FUCK::PushStyleColor(ImGuiCol_ButtonActive, ToColor(a_colors.active));
        FUCK::PushStyleColor(ImGuiCol_Text, ToColor(a_colors.text));
        FUCK::BeginDisabled(!a_enabled);
        const bool clicked = FUCK::Button(a_label);
        FUCK::EndDisabled();
        FUCK::PopStyleColor(4);
        return clicked && a_enabled;
    }

    inline bool IconButton(const char* a_label, uint32_t a_icon, const ButtonColors& a_colors, bool a_enabled, const char* a_id, float a_width, float a_height)
    {
        const auto iconText = ToUTF8(std::u32string(1, static_cast<char32_t>(a_icon)));
        const auto label = std::format("{} {}", iconText, a_label);
        FUCK::PushID(a_id);
        const bool clicked = FLICK::ColoredButton(label.c_str(), a_colors, a_enabled, a_width, a_height);
        FUCK::PopID();
        return clicked;
    }

    inline bool InputText(const char* a_label, char* a_buffer, size_t a_size)
    {
        const std::string_view label{ a_label };
        const auto visibleLabel = label.substr(0, label.find("##"));
        if (visibleLabel.empty()) {
            return FUCK::InputText(a_label, a_buffer, a_size);
        }

        const float width = FUCK::CalcItemWidth();
        FUCK::AlignTextToFramePadding();
        FUCK::TextUnformatted(visibleLabel.data(), visibleLabel.data() + visibleLabel.size());
        FUCK::SameLine(0.0F, 10.0F);
        FUCK::SetNextItemWidth(std::max(1.0F, std::min(width, FUCK::GetContentRegionAvail().x)));
        const auto id = std::format("##{}", a_label);
        return FUCK::InputText(id.c_str(), a_buffer, a_size);
    }
}

namespace MCMMemory::Menu
{
    class FLICKTool final : public FUCK::ITool
    {
    public:

        const char* Name() const override { return BEAUTIFUL_NAME; }

        void OnOpen() override;

        void Draw() override;
    };
}
