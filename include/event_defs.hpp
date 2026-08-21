#pragma once

#include "plugin.hpp"

// Navigation events identify the open MCM or page.
// OptionSelection events identify an option without saving a value.
// ValueChange events hold a changed value or let us read it from the updated menu.
#define FOREACH_MCM_EVENT(MCM_EVENT) \
    MCM_EVENT(ModSelected, "SKICP_modSelected", Navigation, Unknown) \
    MCM_EVENT(PageSelected, "SKICP_pageSelected", Navigation, Unknown) \
    MCM_EVENT(OptionHighlighted, "SKICP_optionHighlighted", OptionSelection, Unknown) \
    MCM_EVENT(OptionSelected, "SKICP_optionSelected", ValueChange, Option) \
    MCM_EVENT(OptionDefaulted, "SKICP_optionDefaulted", ValueChange, Option) \
    MCM_EVENT(KeymapChanged, "SKICP_keymapChanged", ValueChange, Keymap) \
    MCM_EVENT(SliderSelected, "SKICP_sliderSelected", OptionSelection, Unknown) \
    MCM_EVENT(SliderAccepted, "SKICP_sliderAccepted", ValueChange, Slider) \
    MCM_EVENT(MenuSelected, "SKICP_menuSelected", OptionSelection, Unknown) \
    MCM_EVENT(MenuAccepted, "SKICP_menuAccepted", ValueChange, Menu) \
    MCM_EVENT(ColorSelected, "SKICP_colorSelected", OptionSelection, Unknown) \
    MCM_EVENT(ColorAccepted, "SKICP_colorAccepted", ValueChange, Color) \
    MCM_EVENT(InputSelected, "SKICP_inputSelected", OptionSelection, Unknown) \
    MCM_EVENT(InputAccepted, "SKICP_inputAccepted", ValueChange, Input) \
    MCM_EVENT(DialogCanceled, "SKICP_dialogCanceled", Cancel, Unknown)


    #define DECLARE_EVENT_TYPE(name, eventName, role, controlType) name,
    #define DECLARE_EVENT_NAME(name, eventName, role, controlType) eventName,
    #define DECLARE_EVENT_ROLE(name, eventName, role, controlType) EventRole::role,

namespace MCMMemory
{
    enum class EventRole
    {
        Navigation,
        OptionSelection,
        ValueChange,
        Cancel
    };

    enum class EventType
    {
        Unknown,
        FOREACH_MCM_EVENT(DECLARE_EVENT_TYPE)
        Count
    };

    inline constexpr std::array<std::string_view, static_cast<size_t>(EventType::Count)> eventNames
    {
        "Unknown",
        FOREACH_MCM_EVENT(DECLARE_EVENT_NAME)
    };

    inline constexpr std::array<EventRole, static_cast<size_t>(EventType::Count)> eventRoles
    {
        EventRole::Cancel,
        FOREACH_MCM_EVENT(DECLARE_EVENT_ROLE)
    };

    inline EventType ParseEventType(std::string_view a_name)
    {
        for (size_t index = 1; index < eventNames.size(); ++index) {
            if (a_name == eventNames[index]) {
                return static_cast<EventType>(index);
            }
        }
        return EventType::Unknown;
    }

    inline std::string_view EventName(EventType a_type)
    {
        return eventNames[static_cast<size_t>(a_type)];
    }

    inline EventRole Role(EventType a_type)
    {
        return eventRoles[static_cast<size_t>(a_type)];
    }

    inline bool IsValueChange(EventType a_type) { return Role(a_type) == EventRole::ValueChange; }
}

#undef DECLARE_EVENT_TYPE
#undef DECLARE_EVENT_NAME
#undef DECLARE_EVENT_ROLE
