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

    // Returns the name stored in JSON for this control type.
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

    // The page actually loaded in the script, which may be an opening page.
    struct MCMPage
    {
        std::string name;

        int index{-1};

        bool Matches(std::string_view a_name, int a_index) const
        {
            return name == a_name && index == a_index;
        }
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

    // Remembers the control that enables an MCM before its normal settings appear.
    struct MCMActivation
    {
        MCMSelection selection;

        std::string optionLabel;

        std::string stateName;

        std::string enabledText;

        ControlType type{ ControlType::Unknown };

        bool startCommand{};
    };

    // One setting stored in a profile.
    struct CapturedSetting
    {
        MCMSelection selection;

        std::string optionLabel;

        // MCM Helper setting name, or a known cycling control ID supplied by compatibility support.
        std::string settingID;

        // Stable Papyrus state used by state-based MCM options.
        std::string stateName;

        nlohmann::json value;

        // Text a menu row shows for the stored index. A menu is the only control whose live value
        // costs a script call to read, so this lets restore compare it for free.
        std::string valueText;

        std::string valueSource;

        uint64_t sourceEventID{};

        ControlType type{ ControlType::Unknown };

        bool identityComplete{};

        // NL_MCM pages have separate scripts and can reuse the same state names.
        bool pageScopedState{};

        // A SkyUI text row used as a setting. It has no typed value, so the shown text is the value.
        bool textControl{};

        // Replay this captured click once; it has no persistent value to compare.
        bool command{};

        bool confirmedCommand{};

        // Flag if player was seen doing recording.
        // Scanned settings (captured values) have no position and can be restored at once afterwards.
        bool recorded{};

        // Orders a mod recorded settings (e.g. next = sequence + 1). 
        // Scanned ones ignore it.
        int sequence{};

        // This change rebuilt its page, so the controls recorded after it depend on it.
        bool rebuildsPage{};

        // The MCM was closed and opened again before this change, so its handler had
        // everything recorded earlier. Capture decides this, since a profile is reloaded from
        // disk on every update and cannot remember the change before this one.
        bool reopensConfig{};

        // A dropdown can select dependent controls before its delayed redraw is captured.
        // Keep its recorded position even when the page hash did not catch that redraw.
        bool RequiresOrderedReplay() const { return command || type == ControlType::Menu || rebuildsPage || reopensConfig; }

        // Checks whether another captured setting refers to the same MCM option.
        // This avoids duplicate profile settings.
        bool IsSameSetting(const CapturedSetting& a_other) const
        {
            if (type != a_other.type || selection.identity.modID != a_other.selection.identity.modID) {
                return false;
            }
            if ((pageScopedState || a_other.pageScopedState) && selection.pageName != a_other.selection.pageName) {
                return false;
            }
            if (type == ControlType::Cycle && !settingID.empty() && !a_other.settingID.empty()) {
                return settingID == a_other.settingID;
            }
            if (!stateName.empty() && !a_other.stateName.empty()) {
                // A redraw can put a different control at the old row index.
                return stateName == a_other.stateName;
            }
            return selection.pageIndex == a_other.selection.pageIndex && selection.pageName == a_other.selection.pageName && selection.optionIndex == a_other.selection.optionIndex;
        }
    };

    // Keeps a row identity while its MCM rebuilds the page.
    struct MCMControl
    {
        std::string optionLabel;

        std::string stateName;

        // The value SkyUI displayed on this row, used by controls that have no typed value.
        std::string valueText;

        ControlType type{ ControlType::Unknown };

        std::optional<bool> toggleValue;
    };

    // A cycling setting shows a value beside its label. A command button shows no value, or has no
    // label and puts the command in the value instead.
    inline bool IsRecordableTextControl(const MCMControl& a_control)
    {
        return a_control.type == ControlType::Unknown && !a_control.valueText.empty() && !a_control.optionLabel.empty() && a_control.optionLabel != a_control.valueText;
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

        // Read before a redraw; not part of the saved profile format.
        std::optional<MCMControl> control;

        // Instead of checking every control per page just to know if the page changed (cleared, had new controls, etc.), 
        // we can just check the page hash. If it changed, the page was rebuilt.
        std::optional<uint64_t> pageHash;

        uint64_t eventID{};

        // Taken when the event arrived, since the menu can close before the capture finishes.
        uint32_t configSession{};

        EventType type{ EventType::Unknown };

        float numberArgument{};

        RE::FormID senderFormID{};

        bool activationEvent{};

        bool confirmationAccepted{};

        bool confirmationCancelled{};
    };

    // Adds a setting to the vector, replacing an existing one if it matches and a_replaceExisting is true.
    inline void Deduplicate(std::vector<CapturedSetting>& a_settings, CapturedSetting a_setting, bool a_replaceExisting = true)
    {
        auto existing = a_settings.begin();
        if (a_setting.identityComplete) {
            for (; existing != a_settings.end() && (!existing->identityComplete || !existing->IsSameSetting(a_setting)); ++existing) {}
        }
        else {
            for (; existing != a_settings.end() && existing->sourceEventID != a_setting.sourceEventID; ++existing) {}
        }
        if (existing != a_settings.end()) {
            if (a_replaceExisting) {
                *existing = std::move(a_setting);
            }
        }
        else {
            a_settings.push_back(std::move(a_setting));
        }
    }
}

#undef DECLARE_CONTROL_TYPE
#undef DECLARE_CONTROL_TYPE_NAME
