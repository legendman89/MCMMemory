#pragma once

#define FOREACH_RESTORE_ACTION(RESTORE_ACTION) \
    RESTORE_ACTION(OpenConfig, OpenConfig, None, false) \
    RESTORE_ACTION(SetPage, SetPage, Page, false) \
    RESTORE_ACTION(ApplyToggle, SelectOption, ToggleValue, true) \
    RESTORE_ACTION(RequestSliderDialogData, RequestSliderDialogData, OptionIndex, false) \
    RESTORE_ACTION(SetSliderValue, SetSliderValue, FloatValue, true) \
    RESTORE_ACTION(RequestMenuDialogData, RequestMenuDialogData, OptionIndex, false) \
    RESTORE_ACTION(SetMenuIndex, SetMenuIndex, IntegerValue, true) \
    RESTORE_ACTION(RequestColorDialogData, RequestColorDialogData, OptionIndex, false) \
    RESTORE_ACTION(SetColorValue, SetColorValue, IntegerValue, true) \
    RESTORE_ACTION(RequestInputDialogData, RequestInputDialogData, OptionIndex, false) \
    RESTORE_ACTION(SetInputText, SetInputText, StringValue, true) \
    RESTORE_ACTION(RemapKey, RemapKey, KeymapValue, true) \
    RESTORE_ACTION(CloseConfig, CloseConfig, None, false)
