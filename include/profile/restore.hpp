#pragma once

#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_script.hpp"
#include "profile/activity.hpp"
#include "profile/profile.hpp"
#include "profile/restore_defs.hpp"
#include "profile/stats.hpp"
#include "settings.hpp"
#include "utils/scheduler.hpp"

#define DECLARE_RESTORE_ACTION_ENUM(actionName, functionName, argumentType, applyAction) actionName,
#define DECLARE_RESTORE_FUNCTION_NAME(actionName, functionName, argumentType, applyAction) #functionName,
#define DECLARE_RESTORE_ARGUMENT_TYPE(actionName, functionName, argumentType, applyAction) RestoreArgumentType::argumentType,
#define DECLARE_RESTORE_APPLY_ACTION(actionName, functionName, argumentType, applyAction) applyAction,

namespace MCMMemory
{
    // Says which type of argument a restore script function expects.
    enum class RestoreArgumentType
    {
        None,
        Page,
        OptionIndex,
        IntegerValue,
        FloatValue,
        StringValue,
        SettingIntegerValue,
        KeymapValue,
        ToggleValue
    };

    // Gives every supported restore script call a C++ name.
    enum class RestoreActionType
    {
        FOREACH_RESTORE_ACTION(DECLARE_RESTORE_ACTION_ENUM)
        Count
    };

    // These names match the functions on the MCM config script.
    inline constexpr std::array<std::string_view, ToIndex(RestoreActionType::Count)> restoreActionFunctionNames
    {
        FOREACH_RESTORE_ACTION(DECLARE_RESTORE_FUNCTION_NAME)
    };

    // Matches every restore action with the argument it needs.
    inline constexpr std::array<RestoreArgumentType, ToIndex(RestoreActionType::Count)> restoreArgumentTypes
    {
        FOREACH_RESTORE_ACTION(DECLARE_RESTORE_ARGUMENT_TYPE)
    };

    inline constexpr std::array<bool, ToIndex(RestoreActionType::Count)> restoreApplyActions
    {
        FOREACH_RESTORE_ACTION(DECLARE_RESTORE_APPLY_ACTION)
    };

    // Returns the Papyrus function name for one restore action.
    inline std::string_view RestoreActionFunctionName(RestoreActionType a_type)
    {
        return restoreActionFunctionNames[ToIndex(a_type)];
    }

    // Returns the argument type needed by one restore action.
    inline RestoreArgumentType GetRestoreArgumentType(RestoreActionType a_type)
    {
        return restoreArgumentTypes[ToIndex(a_type)];
    }

    inline bool IsRestoreApplyAction(RestoreActionType a_type)
    {
        return restoreApplyActions[ToIndex(a_type)];
    }

    // One small description of a script call waiting to be dispatched.
    struct RestoreAction
    {
        // Page name passed to SetPage.
        std::string pageName;

        // Text passed to a text-setting script call.
        std::string stringValue;

        // Visible label used to verify the current control before changing it.
        std::string optionLabel;

        // Stable Papyrus state used to verify state-based controls.
        std::string stateName;

        // Index of the RestoreMCM that should receive this call.
        size_t mcmIndex{};

        // Says which MCM script function should be called.
        RestoreActionType type{ RestoreActionType::OpenConfig };

        // Says which control this action belongs to.
        ControlType controlType{ ControlType::Unknown };

        // Page index passed to SetPage.
        int pageIndex{-1};

        // Option index used by option, toggle and keymap calls.
        int optionIndex{-1};

        // Integer passed to menu, color or keymap calls.
        int integerValue{};

        // Number passed to a slider call.
        float floatValue{};

        // Desired state used when restoring a toggle.
        bool boolValue{};
    };

    // Creates an action that does not need a value.
    inline RestoreAction MakeRestoreAction(RestoreActionType a_type, size_t a_mcmIndex)
    {
        RestoreAction action;
        action.type = a_type;
        action.mcmIndex = a_mcmIndex;
        return action;
    }

    // Creates a SetPage action from a captured selection.
    inline RestoreAction MakePageAction(size_t a_mcmIndex, const MCMSelection& a_selection)
    {
        auto action = MakeRestoreAction(RestoreActionType::SetPage, a_mcmIndex);
        action.pageName = a_selection.pageName;
        action.pageIndex = a_selection.pageIndex;
        return action;
    }

    // Creates an action that needs an option index.
    inline RestoreAction MakeOptionAction(RestoreActionType a_type, size_t a_mcmIndex, int a_optionIndex)
    {
        auto action = MakeRestoreAction(a_type, a_mcmIndex);
        action.optionIndex = a_optionIndex;
        return action;
    }

    // Creates an action that needs one integer value.
    inline RestoreAction MakeIntegerAction(RestoreActionType a_type, size_t a_mcmIndex, int a_value)
    {
        auto action = MakeRestoreAction(a_type, a_mcmIndex);
        action.integerValue = a_value;
        return action;
    }

    // Creates an action that needs one slider value.
    inline RestoreAction MakeFloatAction(RestoreActionType a_type, size_t a_mcmIndex, float a_value)
    {
        auto action = MakeRestoreAction(a_type, a_mcmIndex);
        action.floatValue = a_value;
        return action;
    }

    // Creates an action that needs one text value.
    inline RestoreAction MakeStringAction(RestoreActionType a_type, size_t a_mcmIndex, std::string_view a_value)
    {
        auto action = MakeRestoreAction(a_type, a_mcmIndex);
        action.stringValue = a_value;
        return action;
    }

    // Calls normal keymaps directly while state-based controls keep SkyUI's state.
    inline RestoreAction MakeKeymapAction(size_t a_mcmIndex, int a_optionIndex, int a_keyCode, bool a_stateControl)
    {
        auto type = a_stateControl ? RestoreActionType::ChangeStateKeymap : RestoreActionType::ChangeKeymap;
        auto action = MakeOptionAction(type, a_mcmIndex, a_optionIndex);
        action.integerValue = a_keyCode;
        return action;
    }

    // Creates a setting call that needs its stable ID and an integer value.
    inline RestoreAction MakeSettingIntegerAction(size_t a_mcmIndex, std::string_view a_settingID, int a_value)
    {
        auto action = MakeStringAction(RestoreActionType::SetIntegerSetting, a_mcmIndex, a_settingID);
        action.integerValue = a_value;
        return action;
    }

    // Creates a toggle action with its desired state.
    inline RestoreAction MakeToggleAction(size_t a_mcmIndex, int a_optionIndex, bool a_value)
    {
        auto action = MakeOptionAction(RestoreActionType::ApplyToggle, a_mcmIndex, a_optionIndex);
        action.boolValue = a_value;
        return action;
    }

    struct RestoreMCM
    {
        // Identifies the profile MCM that will be matched with the registry.
        MCMIdentity identity;

        // Name of the last page added to this MCM action list.
        std::string queuedPageName;

        // Setting actions kept in their original profile order.
        std::vector<RestoreAction> settingActions;

        // MCM Registry gives us this live MCM script after registration.
        RE::BSTSmartPointer<RE::BSScript::Object> mcmScript;

        int queuedPageIndex{-1};

        bool hasQueuedPage{};
    };

    struct RegistryCheckTask
    {
        // Stops a check from an earlier loaded game from reading the new registry.
        uint64_t loadedGameSession{};

        void operator()() const;
    };

    struct RestoreTask
    {
        uint64_t loadedGameSession{};

        uint64_t taskID{};

        void operator()() const;
    };

    class Restore : public RE::BSTEventSink<SKSE::ModCallbackEvent>, public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:

        static Restore* GetSingleton()
        {
            static Restore singleton;
            return std::addressof(singleton);
        }

        inline OperationStatus GetStatus()
        {
            std::lock_guard lock(restoreMutex);
            return status;
        }

        bool Install();

        inline bool Start()
        {
            return Begin({});
        }

        inline bool StartSelected(const MCMFilter& a_filter)
        {
            return !a_filter.empty() && Begin(a_filter);
        }

        bool Cancel();

        void Reset(bool a_autoRestoreAllowed);

        // Waits for a stable MCM registry before starting restoration.
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;

        // Watches character creation and prevents the Journal Menu from opening during restore.
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    private:

        // Lets scheduled tasks call the matching private restore functions.
        friend struct RegistryCheckTask;

        friend struct RestoreTask;

        // Finds the RestoreMCM for a setting or creates it.
        size_t GetOrAddMCM(const CapturedSetting& a_setting);

        bool Begin(MCMFilter a_filter);

        bool LoadProfile();

        // Validates one captured setting and converts it into restore actions.
        bool AddSettingActions(const CapturedSetting& a_setting);

        // Adds SetPage unless the previous setting already selected that page.
        void AddPageAction(size_t a_mcmIndex, const MCMSelection& a_selection);

        // Push the available MCM action lists into one final queue.
        void BuildActionQueue();

        // Sends one queued action to its matching MCM script.
        bool RunAction(const RestoreAction& a_action, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result);

        bool IsActionNeeded(const RestoreAction& a_action) const;

        bool IsActionValid(const RestoreAction& a_action) const;

        // Calls one function on a live MCM script.
        bool CallMCMFunction(size_t a_mcmIndex, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result);

        // Flips a toggle only when its current state differs from the profile.
        bool RestoreToggle(const RestoreAction& a_action, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result);

        // Schedules the next action after the requested delay.
        inline void QueueNextAction(float a_delaySeconds)
        {
            const uint64_t taskID = ++scheduledTaskID;
            if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(RestoreTask{ loadedGameSession, taskID }, a_delaySeconds)) {
                logger::error("Persistent profile restore could not schedule action {}", currentActionIndex);
                restoring = false;
                status = OperationStatus::Idle;
            }
        }

        // Moves the registry read from the event callback to Skyrim task queue.
        inline void QueueRegistryCheck(float a_delaySeconds = 0.0F)
        {
            registryCheckQueued = true;
            if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(RegistryCheckTask{ loadedGameSession }, a_delaySeconds)) {
                registryCheckQueued = false;
                status = OperationStatus::Idle;
                logger::error("Persistent profile restore could not schedule its registry check");
            }
        }

        // Matches profile MCM IDs with their live config scripts.
        void MatchRegisteredMCMs(const std::vector<MCMRegistryEntry>& a_registeredMCMs);

        // Reads the current registry on the game task queue.
        void CheckRegistry(uint64_t a_loadedGameSession);

        // Runs one action if it still belongs to the loaded game.
        void RunNextAction(uint64_t a_loadedGameSession, uint64_t a_taskID);

        // Builds the queue and schedules its first action.
        void StartRestore();

        // Releases the menu only after the last script call returns.
        void FinishRestore();

        void ContinueCancellation();

        void FinishCancellation();

        // Adds the current MCM result to the full restore result.
        void FinishMCMStats(size_t a_mcmIndex, OperationResult a_result = OperationResult::Completed);

        void CloseJournalMenu();

        void Clear();

        // Stops registration events and scheduled tasks from changing restore state together.
        std::mutex restoreMutex;

        // Holds each MCM and the actions prepared for it.
        std::vector<RestoreMCM> restoreMCMs;

        // Holds the final action queue currently being run.
        std::vector<RestoreAction> actions;

        std::vector<ActivityModResult> activityMods;

        MCMFilter mcmFilter;

        RegistryWait registryWait;

        // Points to the next action in the final queue.
        size_t currentActionIndex{};

        size_t activeMCMIndex{};

        // Changes whenever a new game starts or another save is loaded.
        uint64_t loadedGameSession{};

        uint64_t scheduledTaskID{};

        RestoreStats stats;

        RestoreStats mcmStats;

        OperationMode operationMode{ OperationMode::Automatic };

        OperationStatus status{ OperationStatus::Idle };

        // Prevents the registration listener from being installed twice.
        bool installed{};

        // Says whether the selected profile has already been read for this game.
        bool configLoaded{};

        // Says whether the loaded profile can be restored.
        bool configValid{};

        // Automatic restore is only fired for a new game.
        bool autoRestoreAllowed{};

        // Prevents the restore queue from starting more than once.
        bool started{};

        // Keeps the Journal Menu closed until the final script call finishes.
        bool restoring{};

        // Prevents repeated ready events from queuing the same registry check.
        bool registryCheckQueued{};

        bool requestFailed{};

        bool journalMenuOpen{};

        bool characterCreationOpen{};

        bool mcmStarted{};

        bool mcmOpen{};

        bool mcmStatsRecorded{};
    };

    inline void RegistryCheckTask::operator()() const
    {
        Restore::GetSingleton()->CheckRegistry(loadedGameSession);
    }

    inline void RestoreTask::operator()() const
    {
        // Run one restore action on the game task scheduler.
        Restore::GetSingleton()->RunNextAction(loadedGameSession, taskID);
    }
}

#undef DECLARE_RESTORE_ACTION_ENUM
#undef DECLARE_RESTORE_FUNCTION_NAME
#undef DECLARE_RESTORE_ARGUMENT_TYPE
#undef DECLARE_RESTORE_APPLY_ACTION
