#pragma once

// Paths to values stored in SkyUI Scaleform menu.
#define FOREACH_MENU_FIELD(MENU_FIELD) \
    MENU_FIELD(PanelState, "_root.ConfigPanelFader.configPanel._state") \
    MENU_FIELD(PageResetRequested, "_root.ConfigPanelFader.configPanel._bRequestPageReset") \
    MENU_FIELD(ModListSelectedIndex, "_root.ConfigPanelFader.configPanel.modList.selectedIndex") \
    MENU_FIELD(ModListSelectedText, "_root.ConfigPanelFader.configPanel.modList.selectedEntry.text") \
    MENU_FIELD(ModListSelectedLabel, "_root.ConfigPanelFader.configPanel.modList.selectedEntry.label") \
    MENU_FIELD(PageListSelectedIndex, "_root.ConfigPanelFader.configPanel.pageList.selectedIndex") \
    MENU_FIELD(PageListSelectedText, "_root.ConfigPanelFader.configPanel.pageList.selectedEntry.text") \
    MENU_FIELD(OptionListSelectedIndex, "_root.ConfigPanelFader.configPanel.optionList.selectedIndex") \
    MENU_FIELD(OptionListSelectedText, "_root.ConfigPanelFader.configPanel.optionList.selectedEntry.text") \
    MENU_FIELD(OptionListSelectedStringValue, "_root.ConfigPanelFader.configPanel.optionList.selectedEntry.strValue") \
    MENU_FIELD(OptionListSelectedNumberValue, "_root.ConfigPanelFader.configPanel.optionList.selectedEntry.numValue") \
    MENU_FIELD(OptionCursorIndex, "_root.ConfigPanelFader.configPanel.optionCursorIndex") \
    MENU_FIELD(OptionCursorText, "_root.ConfigPanelFader.configPanel.optionCursor.text") \
    MENU_FIELD(OptionCursorStringValue, "_root.ConfigPanelFader.configPanel.optionCursor.strValue") \
    MENU_FIELD(OptionCursorNumberValue, "_root.ConfigPanelFader.configPanel.optionCursor.numValue")
