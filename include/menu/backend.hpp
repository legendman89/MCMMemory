#pragma once

#include <cstddef>
#include <cstdint>

#define FOREACH_GUI_FUNCTIONS(F) \
    F(Checkbox, bool, (const char*, bool*)) \
    F(RadioButton, bool, (const char*, bool)) \
    F(BeginWindow, bool, (const char*, bool*, float, float, bool)) \
    F(EndWindow, void, ()) \
    F(MeasureText, TextSize, (const char*)) \
    F(MeasureButton, TextSize, (const char*, float, float)) \
    F(MeasureTableHeader, float, (const char*)) \
    F(GetAvailableWidth, float, ()) \
    F(GetAvailableHeight, float, ()) \
    F(ColoredText, void, (const Color4&, const char*)) \
    F(CollapsingHeader, bool, (const char*, int)) \
    F(InputTextWithHint, bool, (const char*, const char*, char*, size_t)) \
    F(BeginTable, bool, (const char*, int, int, float, float, float)) \
    F(TableSetupColumn, void, (const char*, int, float, uint32_t)) \
    F(TableSetColumnIndex, void, (int)) \
    F(ReadTableSort, bool, (TableSort&, bool)) \
    F(CenterNextItem, void, (float)) \
    F(Button, bool, (const char*, float, float)) \
    F(Combo, bool, (const char*, int*, const char* const*, int)) \
    F(ColoredButton, bool, (const char*, const ButtonColors&, bool, float, float)) \
    F(IconButton, bool, (const char*, uint32_t, const ButtonColors&, bool, const char*, float, float)) \
    F(Spacing, void, ()) \
    F(SameLine, void, (float, float)) \
    F(SeparatorText, void, (const char*)) \
    F(HelpMarker, void, (const char*)) \
    F(WrappedTooltip, void, (const char*, float)) \
    F(BoldTextColored, void, (const Color4&, const char*)) \
    F(AlignTextToFramePadding, void, ()) \
    F(SetNextItemWidth, void, (float)) \
    F(SetCursorPosX, void, (float)) \
    F(GetCursorPosX, float, ()) \
    F(SetCursorPosY, void, (float)) \
    F(GetCursorPosY, float, ()) \
    F(BeginDisabled, void, (bool)) \
    F(EndDisabled, void, ()) \
    F(PushIDString, void, (const char*)) \
    F(PushIDInt, void, (int)) \
    F(PopID, void, ()) \
    F(TextUnformatted, void, (const char*, const char*)) \
    F(Text, void, (const char*, ...)) \
    F(TextWrapped, void, (const char*, ...)) \
    F(TextDisabled, void, (const char*, ...)) \
    F(BeginTooltip, bool, ()) \
    F(EndTooltip, void, ()) \
    F(PushTextWrapPos, void, (float)) \
    F(PopTextWrapPos, void, ()) \
    F(IsItemActive, bool, ()) \
    F(IsItemClicked, bool, (int)) \
    F(IsItemDeactivatedAfterEdit, bool, ()) \
    F(GetFrameHeight, float, ()) \
    F(GetFrameHeightWithSpacing, float, ()) \
    F(EndTable, void, ()) \
    F(TableHeadersRow, void, ()) \
    F(TableNextRow, void, (int, float)) \
    F(TableNextColumn, bool, ()) \
    F(TableSetupScrollFreeze, void, (int, int)) \
    F(PopStyleColor, void, (int)) \
    F(PopStyleVar, void, (int)) \
    F(InputText, bool, (const char*, char*, size_t)) \
    F(SliderFloat, bool, (const char*, float*, float, float, const char*)) \
    F(SliderInt, bool, (const char*, int*, int, int, const char*))

#define GUI_DECLARE_TYPE(name, result, args) using name ## Function = result (*) args;
#define GUI_DECLARE_ID(name, result, args) name,
#define GUI_DECLARE_CALLBACK(name, result, args) name ## Function mem ## name{};
#define GUI_MATCH_ID(name, result, args) || a_function == Function::name

namespace MCMMemory::GUI
{
    struct TableSort
    {
        int column{ -1 };
        bool descending{};
    };

    struct TextSize
    {
        float width, height;
    };

    struct Color4
    {
        float r, g, b, a;
    };

    struct ButtonColors
    {
        Color4 background, hover, active, text;
    };

    enum class Function
    {
        FOREACH_GUI_FUNCTIONS(GUI_DECLARE_ID)
    };

    FOREACH_GUI_FUNCTIONS(GUI_DECLARE_TYPE)

    struct Backend
    {
        FOREACH_GUI_FUNCTIONS(GUI_DECLARE_CALLBACK)
    };

    inline Backend& GetBE()
    {
        static Backend backend;
        return backend;
    }

    template <Function a_function, class Callback>
    inline void RegisterFunction(Callback a_callback)
    {
        static_assert(false FOREACH_GUI_FUNCTIONS(GUI_MATCH_ID));

#define GUI_REGISTER_CALLBACK(name, result, args) \
        if constexpr (a_function == Function::name) { \
            GetBE().mem ## name = a_callback; \
        }
        FOREACH_GUI_FUNCTIONS(GUI_REGISTER_CALLBACK)
#undef GUI_REGISTER_CALLBACK

    }

    template <class Specs, class Direction>
    inline bool CopyTableSort(Specs* a_specs, TableSort& a_sort, bool a_force, Direction a_descending)
    {
        if (!a_specs || (!a_specs->SpecsDirty && !a_force)) {
            return false;
        }

        a_sort = {};
        if (a_specs->SpecsCount > 0) {
            a_sort.column = a_specs->Specs[0].ColumnIndex;
            a_sort.descending = a_specs->Specs[0].SortDirection == a_descending;
        }

        a_specs->SpecsDirty = false;
        
        return true;
    }

    inline bool Checkbox(const char* a_label, bool* a_value)
    {
        const auto checkbox = GetBE().memCheckbox;
        return checkbox && checkbox(a_label, a_value);
    }

    inline bool RadioButton(const char* a_label, bool a_selected)
    {
        const auto radioButton = GetBE().memRadioButton;
        return radioButton && radioButton(a_label, a_selected);
    }

    inline bool BeginWindow(const char* a_title, bool* a_open, float a_width, float a_height, bool a_opaque = false)
    {
        const auto beginWindow = GetBE().memBeginWindow;
        return beginWindow && beginWindow(a_title, a_open, a_width, a_height, a_opaque);
    }

    inline void EndWindow()
    {
        const auto endWindow = GetBE().memEndWindow;
        if (endWindow) {
            endWindow();
        }
    }

    inline TextSize MeasureText(const char* a_text)
    {
        const auto measureText = GetBE().memMeasureText;
        return measureText ? measureText(a_text) : TextSize{};
    }

    inline TextSize MeasureButton(const char* a_label, float a_width = 0.0F, float a_height = 0.0F)
    {
        const auto measureButton = GetBE().memMeasureButton;
        return measureButton ? measureButton(a_label, a_width, a_height) : TextSize{};
    }

    inline float MeasureTableHeader(const char* a_label)
    {
        const auto measureTableHeader = GetBE().memMeasureTableHeader;
        return measureTableHeader ? measureTableHeader(a_label) : 0.0F;
    }

    inline float GetAvailableWidth()
    {
        const auto getAvailableWidth = GetBE().memGetAvailableWidth;
        return getAvailableWidth ? getAvailableWidth() : 0.0F;
    }

    inline float GetAvailableHeight()
    {
        const auto getAvailableHeight = GetBE().memGetAvailableHeight;
        return getAvailableHeight ? getAvailableHeight() : 0.0F;
    }

    inline void ColoredText(const Color4& a_color, const char* a_text)
    {
        const auto coloredText = GetBE().memColoredText;
        if (coloredText) {
            coloredText(a_color, a_text);
        }
    }

    inline bool CollapsingHeader(const char* a_label, int a_flags = 0)
    {
        const auto collapsingHeader = GetBE().memCollapsingHeader;
        return collapsingHeader && collapsingHeader(a_label, a_flags);
    }

    inline bool InputTextWithHint(const char* a_label, const char* a_hint, char* a_buffer, size_t a_size)
    {
        const auto inputTextWithHint = GetBE().memInputTextWithHint;
        return inputTextWithHint && inputTextWithHint(a_label, a_hint, a_buffer, a_size);
    }

    inline bool BeginTable(const char* a_id, int a_columns, int a_flags = 0, float a_width = 0.0F, float a_height = 0.0F, float a_innerWidth = 0.0F)
    {
        const auto beginTable = GetBE().memBeginTable;
        return beginTable && beginTable(a_id, a_columns, a_flags, a_width, a_height, a_innerWidth);
    }

    inline void TableSetupColumn(const char* a_label, int a_flags = 0, float a_width = 0.0F, uint32_t a_id = 0)
    {
        const auto tableSetupColumn = GetBE().memTableSetupColumn;
        if (tableSetupColumn) {
            tableSetupColumn(a_label, a_flags, a_width, a_id);
        }
    }

    inline void TableSetColumnIndex(int a_column)
    {
        const auto tableSetColumnIndex = GetBE().memTableSetColumnIndex;
        if (tableSetColumnIndex) {
            tableSetColumnIndex(a_column);
        }
    }

    inline bool ReadTableSort(TableSort& a_sort, bool a_force = false)
    {
        const auto readTableSort = GetBE().memReadTableSort;
        return readTableSort && readTableSort(a_sort, a_force);
    }

    inline void CenterNextItem(float a_width)
    {
        const auto centerNextItem = GetBE().memCenterNextItem;
        if (centerNextItem) {
            centerNextItem(a_width);
        }
    }

    inline bool Button(const char* a_label, float a_width = 0.0F, float a_height = 0.0F)
    {
        const auto button = GetBE().memButton;
        return button && button(a_label, a_width, a_height);
    }

    inline bool Combo(const char* a_label, int* a_selected, const char* const* a_items, int a_count)
    {
        const auto combo = GetBE().memCombo;
        return combo && combo(a_label, a_selected, a_items, a_count);
    }

    inline bool ColoredButton(const char* a_label, const ButtonColors& a_colors, bool a_enabled = true, float a_width = 0.0F, float a_height = 0.0F)
    {
        const auto coloredButton = GetBE().memColoredButton;
        return coloredButton && coloredButton(a_label, a_colors, a_enabled, a_width, a_height);
    }

    inline bool IconButton(const char* a_label, uint32_t a_icon, const ButtonColors& a_colors, bool a_enabled, const char* a_id, float a_width = 0.0F, float a_height = 0.0F)
    {
        const auto iconButton = GetBE().memIconButton;
        return iconButton && iconButton(a_label, a_icon, a_colors, a_enabled, a_id, a_width, a_height);
    }

    inline void Spacing()
    {
        const auto spacing = GetBE().memSpacing;
        if (spacing) {
            spacing();
        }
    }

    inline void SameLine(float a_offset = 0.0F, float a_spacing = -1.0F)
    {
        const auto sameLine = GetBE().memSameLine;
        if (sameLine) {
            sameLine(a_offset, a_spacing);
        }
    }

    inline void SeparatorText(const char* a_label)
    {
        const auto separatorText = GetBE().memSeparatorText;
        if (separatorText) {
            separatorText(a_label);
        }
    }

    inline void HelpMarker(const char* a_text)
    {
        const auto helpMarker = GetBE().memHelpMarker;
        if (helpMarker) {
            helpMarker(a_text);
        }
    }

    inline void WrappedTooltip(const char* a_text, const float a_width = 500.0F)
    {
        const auto wrappedTooltip = GetBE().memWrappedTooltip;
        if (wrappedTooltip) {
            wrappedTooltip(a_text, a_width);
        }
    }

    inline void BoldTextColored(const Color4& a_color, const char* a_text)
    {
        const auto boldTextColored = GetBE().memBoldTextColored;
        if (boldTextColored) {
            boldTextColored(a_color, a_text);
        }
    }

    inline void AlignTextToFramePadding()
    {
        const auto alignTextToFramePadding = GetBE().memAlignTextToFramePadding;
        if (alignTextToFramePadding) {
            alignTextToFramePadding();
        }
    }

    inline void SetNextItemWidth(float a_width)
    {
        const auto setNextItemWidth = GetBE().memSetNextItemWidth;
        if (setNextItemWidth) {
            setNextItemWidth(a_width);
        }
    }

    inline void SetCursorPosX(float a_x)
    {
        const auto setCursorPosX = GetBE().memSetCursorPosX;
        if (setCursorPosX) {
            setCursorPosX(a_x);
        }
    }

    inline float GetCursorPosX()
    {
        const auto getCursorPosX = GetBE().memGetCursorPosX;
        return getCursorPosX ? getCursorPosX() : 0.0F;
    }

    inline void SetCursorPosY(float a_y)
    {
        const auto setCursorPosY = GetBE().memSetCursorPosY;
        if (setCursorPosY) {
            setCursorPosY(a_y);
        }
    }

    inline float GetCursorPosY()
    {
        const auto getCursorPosY = GetBE().memGetCursorPosY;
        return getCursorPosY ? getCursorPosY() : 0.0F;
    }

    inline void BeginDisabled(bool a_disabled = true)
    {
        const auto beginDisabled = GetBE().memBeginDisabled;
        if (beginDisabled) {
            beginDisabled(a_disabled);
        }
    }

    inline void EndDisabled()
    {
        const auto endDisabled = GetBE().memEndDisabled;
        if (endDisabled) {
            endDisabled();
        }
    }

    inline void PushID(const char* a_id)
    {
        const auto pushIDString = GetBE().memPushIDString;
        if (pushIDString) {
            pushIDString(a_id);
        }
    }

    inline void PushID(int a_id)
    {
        const auto pushIDInt = GetBE().memPushIDInt;
        if (pushIDInt) {
            pushIDInt(a_id);
        }
    }

    inline void PopID()
    {
        const auto popID = GetBE().memPopID;
        if (popID) {
            popID();
        }
    }

    inline void TextUnformatted(const char* a_text, const char* a_end = nullptr)
    {
        const auto textUnformatted = GetBE().memTextUnformatted;
        if (textUnformatted) {
            textUnformatted(a_text, a_end);
        }
    }

    template <class... Args>
    inline void Text(const char* a_format, Args... a_args)
    {
        const auto text = GetBE().memText;
        if (text) {
            text(a_format, a_args...);
        }
    }

    template <class... Args>
    inline void TextWrapped(const char* a_format, Args... a_args)
    {
        const auto textWrapped = GetBE().memTextWrapped;
        if (textWrapped) {
            textWrapped(a_format, a_args...);
        }
    }

    template <class... Args>
    inline void TextDisabled(const char* a_format, Args... a_args)
    {
        const auto textDisabled = GetBE().memTextDisabled;
        if (textDisabled) {
            textDisabled(a_format, a_args...);
        }
    }

    inline bool BeginTooltip()
    {
        const auto beginTooltip = GetBE().memBeginTooltip;
        return beginTooltip && beginTooltip();
    }

    inline void EndTooltip()
    {
        const auto endTooltip = GetBE().memEndTooltip;
        if (endTooltip) {
            endTooltip();
        }
    }

    inline void PushTextWrapPos(float a_wrapPos = 0.0F)
    {
        const auto pushTextWrapPos = GetBE().memPushTextWrapPos;
        if (pushTextWrapPos) {
            pushTextWrapPos(a_wrapPos);
        }
    }

    inline void PopTextWrapPos()
    {
        const auto popTextWrapPos = GetBE().memPopTextWrapPos;
        if (popTextWrapPos) {
            popTextWrapPos();
        }
    }

    inline bool IsItemActive()
    {
        const auto isItemActive = GetBE().memIsItemActive;
        return isItemActive && isItemActive();
    }

    inline bool IsItemClicked(int a_button = 0)
    {
        const auto isItemClicked = GetBE().memIsItemClicked;
        return isItemClicked && isItemClicked(a_button);
    }

    inline bool IsItemDeactivatedAfterEdit()
    {
        const auto isItemDeactivatedAfterEdit = GetBE().memIsItemDeactivatedAfterEdit;
        return isItemDeactivatedAfterEdit && isItemDeactivatedAfterEdit();
    }

    inline float GetFrameHeight()
    {
        const auto getFrameHeight = GetBE().memGetFrameHeight;
        return getFrameHeight ? getFrameHeight() : 0.0F;
    }

    inline float GetFrameHeightWithSpacing()
    {
        const auto getFrameHeightWithSpacing = GetBE().memGetFrameHeightWithSpacing;
        return getFrameHeightWithSpacing ? getFrameHeightWithSpacing() : 0.0F;
    }

    inline void EndTable()
    {
        const auto endTable = GetBE().memEndTable;
        if (endTable) {
            endTable();
        }
    }

    inline void TableHeadersRow()
    {
        const auto tableHeadersRow = GetBE().memTableHeadersRow;
        if (tableHeadersRow) {
            tableHeadersRow();
        }
    }

    inline void TableNextRow(int a_flags = 0, float a_minHeight = 0.0F)
    {
        const auto tableNextRow = GetBE().memTableNextRow;
        if (tableNextRow) {
            tableNextRow(a_flags, a_minHeight);
        }
    }

    inline bool TableNextColumn()
    {
        const auto tableNextColumn = GetBE().memTableNextColumn;
        return tableNextColumn && tableNextColumn();
    }

    inline void TableSetupScrollFreeze(int a_cols, int a_rows)
    {
        const auto tableSetupScrollFreeze = GetBE().memTableSetupScrollFreeze;
        if (tableSetupScrollFreeze) {
            tableSetupScrollFreeze(a_cols, a_rows);
        }
    }

    inline void PopStyleColor(int a_count = 1)
    {
        const auto popStyleColor = GetBE().memPopStyleColor;
        if (popStyleColor) {
            popStyleColor(a_count);
        }
    }

    inline void PopStyleVar(int a_count = 1)
    {
        const auto popStyleVar = GetBE().memPopStyleVar;
        if (popStyleVar) {
            popStyleVar(a_count);
        }
    }

    inline bool InputText(const char* a_label, char* a_buffer, size_t a_size)
    {
        const auto inputText = GetBE().memInputText;
        return inputText && inputText(a_label, a_buffer, a_size);
    }

    inline bool SliderFloat(const char* a_label, float* a_value, float a_min, float a_max, const char* a_format = "%.3f")
    {
        const auto sliderFloat = GetBE().memSliderFloat;
        return sliderFloat && sliderFloat(a_label, a_value, a_min, a_max, a_format);
    }

    inline bool SliderInt(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format = "%d")
    {
        const auto sliderInt = GetBE().memSliderInt;
        return sliderInt && sliderInt(a_label, a_value, a_min, a_max, a_format);
    }
}

#undef GUI_MATCH_ID
#undef GUI_DECLARE_ID
#undef GUI_DECLARE_CALLBACK
#undef GUI_DECLARE_TYPE
