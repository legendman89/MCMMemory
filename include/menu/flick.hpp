#pragma once

#include "plugin.hpp"
#include "menu/backend.hpp"
#include "menu/API/FUCK_API.h"
#include "utils/helper.hpp"

namespace FLK = FUCK;

namespace MCMMemory::GUI::FLICK
{
    using namespace FLK;

    inline constexpr PushIDStringFunction PushIDString = FLK::PushID;
    inline constexpr PushIDIntFunction PushIDInt = FLK::PushID;

    inline bool BeginTable(const char* a_id, int a_columns, int a_flags, float a_width, float a_height, float a_innerWidth)
    {
        return FLK::BeginTable(a_id, a_columns, static_cast<FLK::TableFlags>(a_flags), ImVec2{ a_width, a_height }, a_innerWidth);
    }

    inline void TableSetupColumn(const char* a_label, int a_flags, float a_width, uint32_t a_id)
    {
        FLK::TableSetupColumn(a_label, static_cast<FLK::TableColumnFlags>(a_flags), a_width, a_id);
    }

    inline bool ReadTableSort(TableSort& a_sort, bool a_force)
    {
        return CopyTableSort(FLK::GetTableSortSpecs(), a_sort, a_force, ImGuiSortDirection_Descending);
    }

    inline float MeasureTableHeader(const char* a_label)
    {
        const float padding = FLK::GetStyleVarVec(ImGuiStyleVar_FramePadding).x + 2.0F * FLK::GetStyleVarVec(ImGuiStyleVar_CellPadding).x;
        return FLK::CalcTextSize(a_label).x + FLK::GetTextLineHeight() + padding;
    }

    inline float GetAvailableWidth() { return FLK::GetContentRegionAvail().x; }

    inline float GetAvailableHeight() { return FLK::GetContentRegionAvail().y; }

    inline bool Checkbox(const char* a_label, bool* a_value)
    {
        return FLK::Checkbox(a_label, a_value, false, false);
    }

    inline bool RadioButton(const char* a_label, bool a_selected)
    {
        return FLK::Checkbox(a_label, &a_selected, false, false) && a_selected;
    }

    inline bool BeginWindow(const char* a_title, bool* a_open, float a_width, float a_height)
    {
        const auto displaySize = FLK::GetDisplaySize();
        FLK::SetNextWindowSize(ImVec2{ a_width, a_height }, ImGuiCond_Appearing);
        FLK::SetNextWindowPos(ImVec2{ displaySize.x * 0.5F, displaySize.y * 0.5F }, ImGuiCond_Appearing, ImVec2{ 0.5F, 0.5F });
        return FLK::BeginWindow(a_title, a_open, ImGuiWindowFlags_NoCollapse);
    }

    inline TextSize MeasureText(const char* a_text)
    {
        const auto size = FLK::CalcTextSize(a_text);
        return { size.x, size.y };
    }

    inline TextSize MeasureButton(const char* a_label, float, float)
    {
        const auto size = FLK::CalcTextSize(a_label);
        const float scale = FLK::GetTextLineHeight() / 30.0F;
        return { size.x + 16.0F * scale, std::max(FLK::GetFrameHeight(), size.y + 14.0F * scale) };
    }

    inline void CenterNextItem(float a_width)
    {
        FLK::SetCursorPosX(FLK::GetCursorPos().x + std::max(0.0F, (FLK::GetContentRegionAvail().x - a_width) * 0.5F));
    }

    inline bool Button(const char* a_label, float, float)
    {
        return FLK::Button(a_label);
    }

    inline void Spacing()
    {
        FLK::Spacing();
    }

    inline void HelpMarker(const char* a_text)
    {
        const auto itemMin = FLK::GetItemRectMin();
        const auto itemSize = FLK::GetItemRectSize();
        const auto textSize = FLK::CalcTextSize("(?)");
        FLK::SameLine(0.0F, 0.0F);
        const auto position = FLK::GetCursorScreenPos();
        FLK::SetCursorScreenPos(ImVec2{ position.x, itemMin.y + std::max(0.0F, (itemSize.y - textSize.y) * 0.5F) });
        FLK::TextDisabled("(?)");
        GUI::WrappedTooltip(a_text);
    }

    inline void WrappedTooltip(const char* a_text, const float a_width)
    {
        if (FLK::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && FLK::BeginTooltip()) {
            FLK::PushTextWrapPos(FLK::GetCursorPos().x + a_width + 50.0F);
            FLK::TextUnformatted(a_text);
            FLK::PopTextWrapPos();
            FLK::EndTooltip();
        }
    }

    inline float GetCursorPosX()
    {
        return FLK::GetCursorPos().x;
    }

    inline std::string InlineFieldLabel(const char* a_label)
    {
        const std::string_view label{ a_label };
        const auto visibleLabel = label.substr(0, label.find("##"));

        if (visibleLabel.empty()) {
            return std::string(label);
        }

        const float width = FLK::CalcItemWidth();

        FLK::AlignTextToFramePadding();
        FLK::TextUnformatted(visibleLabel.data(), visibleLabel.data() + visibleLabel.size());

        FLK::SameLine(0.0F, 10.0F);

        FLK::SetNextItemWidth(std::max(1.0F, std::min(width, FLK::GetContentRegionAvail().x)));
        return std::format("##{}", a_label);
    }

    inline bool Combo(const char* a_label, int* a_selected, const char* const* a_items, int a_count)
    {
        const auto id = InlineFieldLabel(a_label);
        return FLK::Combo(id.c_str(), a_selected, a_items, a_count);
    }

    inline ImVec4 ToColor(const Color4& a_color)
    {
        return { a_color.r, a_color.g, a_color.b, a_color.a };
    }

    inline void BoldTextColored(const Color4& a_color, const char* a_text)
    {
        FLK::TextColored(ToColor(a_color), "%s", a_text);
    }

    inline void ColoredText(const Color4& a_color, const char* a_text)
    {
        FLK::TextColored(ToColor(a_color), "%s", a_text);
    }

    inline bool ColoredButton(const char* a_label, const ButtonColors& a_colors, bool a_enabled, float, float)
    {
        FLK::PushStyleColor(ImGuiCol_Button, ToColor(a_colors.background));
        FLK::PushStyleColor(ImGuiCol_ButtonHovered, ToColor(a_colors.hover));
        FLK::PushStyleColor(ImGuiCol_ButtonActive, ToColor(a_colors.active));
        FLK::PushStyleColor(ImGuiCol_Text, ToColor(a_colors.text));
        FLK::BeginDisabled(!a_enabled);
        const bool clicked = FLK::Button(a_label);
        FLK::EndDisabled();
        FLK::PopStyleColor(4);
        return clicked && a_enabled;
    }

    inline bool IconButton(const char* a_label, uint32_t a_icon, const ButtonColors& a_colors, bool a_enabled, const char* a_id, float a_width, float a_height)
    {
        const auto iconText = ToUTF8(std::u32string(1, static_cast<char32_t>(a_icon)));
        const auto label = std::format("{} {}", iconText, a_label);
        FLK::PushID(a_id);
        const bool clicked = FLICK::ColoredButton(label.c_str(), a_colors, a_enabled, a_width, a_height);
        FLK::PopID();
        return clicked;
    }

    inline bool InputText(const char* a_label, char* a_buffer, size_t a_size)
    {
        const auto id = InlineFieldLabel(a_label);
        return FLK::InputText(id.c_str(), a_buffer, a_size);
    }

    inline bool InputTextWithHint(const char* a_label, const char* a_hint, char* a_buffer, size_t a_size)
    {
        FLK::PushID(a_label);
        const bool changed = FLICK::InputText(a_hint, a_buffer, a_size);
        FLK::PopID();
        return changed;
    }
}

namespace MCMMemory::Menu
{
    class FLICKTool final : public FLK::ITool
    {
    public:

        const char* Name() const override { return BEAUTIFUL_NAME; }

        void OnOpen() override;

        void Draw() override;
    };
}
