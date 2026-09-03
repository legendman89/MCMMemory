#pragma once

#define FOREACH_CONTROL_TYPE(CONTROL_TYPE) \
    CONTROL_TYPE(Unknown, "unknown") \
    CONTROL_TYPE(Option, "option") \
    CONTROL_TYPE(Slider, "slider") \
    CONTROL_TYPE(Menu, "menu") \
    CONTROL_TYPE(Color, "color") \
    CONTROL_TYPE(Input, "input") \
    CONTROL_TYPE(Keymap, "keymap") \
    CONTROL_TYPE(Cycle, "cycle")
