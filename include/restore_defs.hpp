#pragma once

#define FOREACH_RESTORE_ACTION(RESTORE_ACTION) \
    RESTORE_ACTION(OpenConfig, OpenConfig, None) \
    RESTORE_ACTION(SetPage, SetPage, Page) \
    RESTORE_ACTION(ApplyToggle, SelectOption, ToggleValue) \
    RESTORE_ACTION(RequestSliderDialogData, RequestSliderDialogData, OptionIndex) \
    RESTORE_ACTION(SetSliderValue, SetSliderValue, FloatValue) \
    RESTORE_ACTION(RequestMenuDialogData, RequestMenuDialogData, OptionIndex) \
    RESTORE_ACTION(SetMenuIndex, SetMenuIndex, IntegerValue) \
    RESTORE_ACTION(RequestColorDialogData, RequestColorDialogData, OptionIndex) \
    RESTORE_ACTION(SetColorValue, SetColorValue, IntegerValue) \
    RESTORE_ACTION(RequestInputDialogData, RequestInputDialogData, OptionIndex) \
    RESTORE_ACTION(SetInputText, SetInputText, StringValue) \
    RESTORE_ACTION(RemapKey, RemapKey, KeymapValue) \
    RESTORE_ACTION(CloseConfig, CloseConfig, None)
