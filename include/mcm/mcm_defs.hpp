#pragma once

// SkyUI SKI_ConfigBase option types and their profile control types.
#define FOREACH_SKYUI_OPTION_TYPE(OPTION_TYPE) \
    OPTION_TYPE(Empty, 0, Unknown) \
    OPTION_TYPE(Header, 1, Unknown) \
    OPTION_TYPE(Text, 2, Unknown) \
    OPTION_TYPE(Toggle, 3, Option) \
    OPTION_TYPE(Slider, 4, Slider) \
    OPTION_TYPE(Menu, 5, Menu) \
    OPTION_TYPE(Color, 6, Color) \
    OPTION_TYPE(Keymap, 7, Keymap) \
    OPTION_TYPE(Input, 8, Input)

#define DECLARE_SKYUI_OPTION_TYPE(name, value, control) name = value,

namespace MCMMemory
{
    enum class SkyUIOptionType : int
    {
        FOREACH_SKYUI_OPTION_TYPE(DECLARE_SKYUI_OPTION_TYPE)
        Count
    };
}

#undef DECLARE_SKYUI_OPTION_TYPE
