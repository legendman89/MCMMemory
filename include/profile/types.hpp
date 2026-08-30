#pragma once

#include "mcm/control_defs.hpp"
#include "mcm/event_defs.hpp"
#include "utils/helper.hpp"

#define DECLARE_CONTROL_TYPE(name, text) name,
#define DECLARE_CONTROL_TYPE_NAME(name, text) text,

namespace MCMMemory
{
    enum class ControlType
    {
        FOREACH_CONTROL_TYPE(DECLARE_CONTROL_TYPE)
        Count
    };

    inline constexpr std::array<std::string_view, ToIndex(ControlType::Count)> controlTypeNames
    {
        FOREACH_CONTROL_TYPE(DECLARE_CONTROL_TYPE_NAME)
    };

    inline constexpr std::array<ControlType, ToIndex(EventType::Count)> eventControlTypes
    {
        ControlType::Unknown,
#define DECLARE_EVENT_CONTROL_TYPE(name, eventName, role, controlType) ControlType::controlType,
        FOREACH_MCM_EVENT(DECLARE_EVENT_CONTROL_TYPE)
#undef DECLARE_EVENT_CONTROL_TYPE
    };

    // Says whether this is a toggle, slider, menu, color, input or keymap.
    inline std::string_view ControlTypeName(ControlType a_type)
    {
        return controlTypeNames[ToIndex(a_type)];
    }

    inline ControlType ParseControlType(std::string_view a_name)
    {
        for (size_t index = 1; index < controlTypeNames.size(); ++index) {
            if (a_name == controlTypeNames[index]) {
                return static_cast<ControlType>(index);
            }
        }
        return ControlType::Unknown;
    }

    inline ControlType ControlTypeForEvent(EventType a_type)
    {
        return eventControlTypes[ToIndex(a_type)];
    }

    using MCMFilter = std::vector<std::string>;

    inline bool ContainsMCMID(const MCMFilter& a_filter, std::string_view a_modID)
    {
        for (const auto& modID : a_filter) {
            if (modID == a_modID) {
                return true;
            }
        }
        return false;
    }

    inline bool AllowsMCM(const MCMFilter& a_filter, std::string_view a_modID)
    {
        return a_filter.empty() || ContainsMCMID(a_filter, a_modID);
    }

    // Identifies one MCM by its visible name and stable ID.
    struct MCMIdentity
    {
        std::string modName;
        std::string modID;
    };

    // Tracks the MCM page and option the player is currently using.
    struct MCMSelection
    {
        // Identifies the selected MCM.
        MCMIdentity identity;

        // The name of the selected MCM page.
        std::string pageName;

        // The selected row in the MCM list.
        int modIndex{-1};

        // The selected row in the page list.
        int pageIndex{-1};

        // The selected option on the page.
        int optionIndex{-1};
    };

    // One setting stored in a profile.
    struct CapturedSetting
    {
        MCMSelection selection;

        std::string optionLabel;

        // MCM Helper setting name.
        std::string settingID;

        // Stable Papyrus state used by state-based MCM options.
        std::string stateName;

        nlohmann::json value;

        std::string valueSource;

        uint64_t sourceEventID{};

        ControlType type{ ControlType::Unknown };

        bool identityComplete{};

        // Checks whether another captured setting refers to the same MCM option.
        // This avoids duplicate profile entries.
        bool IsSameSetting(const CapturedSetting& a_other) const
        {
            if (type != a_other.type || selection.identity.modID != a_other.selection.identity.modID) {
                return false;
            }
            if (!stateName.empty() && !a_other.stateName.empty() && stateName == a_other.stateName) {
                return true;
            }
            return selection.pageIndex == a_other.selection.pageIndex && selection.pageName == a_other.selection.pageName && selection.optionIndex == a_other.selection.optionIndex;
        }
    };

    // Adds a setting or replaces an older capture of the same setting.
    inline void Deduplicate(std::vector<CapturedSetting>& a_settings, CapturedSetting a_setting)
    {
        auto existing = a_settings.begin();
        if (a_setting.identityComplete) {
            for (; existing != a_settings.end() && (!existing->identityComplete || !existing->IsSameSetting(a_setting)); ++existing) {}
        }
        else {
            for (; existing != a_settings.end() && existing->sourceEventID != a_setting.sourceEventID; ++existing) {}
        }
        if (existing != a_settings.end()) {
            *existing = std::move(a_setting);
        }
        else {
            a_settings.push_back(std::move(a_setting));
        }
    }

    // Keeps the raw event and menu state for Capture.json debugging.
    struct CaptureRecord
    {
        // Stores strArg from the callback.
        std::string stringArgument;

        MCMSelection selection;

        // Stores menu states read safely after the callback and after a short delay.
        nlohmann::json state;
        nlohmann::json stateAfter;

        uint64_t eventID{};

        EventType type{ EventType::Unknown };

        float numberArgument{};

        RE::FormID senderFormID{};
    };
}

#undef DECLARE_CONTROL_TYPE
#undef DECLARE_CONTROL_TYPE_NAME
