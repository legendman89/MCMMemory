#include "profile/restore.hpp"
#include "mcm/mcm_support.hpp"

#include <cmath>

namespace MCMMemory
{
    bool Restore::CallMCMFunction(size_t a_mcmIndex, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, SKSE::TaskInterface::TaskFn a_result)
    {
        if (a_mcmIndex >= restoreMCMs.size()) {
            delete a_arguments;
            return false;
        }
        const auto& mcm = restoreMCMs[a_mcmIndex];
        return callWatch.Call(MCMScript(mcm.mcmScript), mcm.identity.modID, a_functionName, a_arguments, std::move(a_result));
    }

    bool Restore::RestoreToggle(const RestoreAction& a_action, SKSE::TaskInterface::TaskFn a_result)
    {
        return CallMCMFunction(a_action.mcmIndex, RestoreActionFunctionName(a_action.type), RE::MakeFunctionArguments(int{ a_action.optionIndex }), std::move(a_result));
    }

    bool Restore::IsActionValid(const RestoreAction& a_action) const
    {
        if (a_action.controlType == ControlType::Unknown || a_action.mcmIndex >= restoreMCMs.size()) {
            return true;
        }
        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
        if (a_action.type == RestoreActionType::ApplyCycle) {
            const auto* cycle = VioLensSupport::FindCycle(a_action.stringValue);
            auto page = script.ReadCurrentPage();
            if (!cycle || !VioLensSupport::IsSupported(script) || !page || !page->Matches(a_action.pageName, a_action.pageIndex) || a_action.optionLabel != cycle->optionLabel || !a_action.stateName.empty()) {
                return false;
            }
            if (a_action.refreshingCycle || VioLensSupport::FindCycleIndex(script, *cycle, true)) {
                return true;
            }
            // A matching disabled value needs no click. A different one must still be skipped safely.
            auto value = VioLensSupport::ReadCycleValue(script, *cycle);
            return value && *value == a_action.integerValue && VioLensSupport::FindCycleIndex(script, *cycle).has_value();
        }
        if (VioLensSupport::IsSupported(script)) {
            auto liveState = script.ReadStateName(a_action.optionIndex);
            auto liveLabel = script.ReadOptionLabel(a_action.optionIndex);
            auto page = script.ReadCurrentPage();
            const auto& modID = restoreMCMs[a_action.mcmIndex].identity.modID;
            // Older profiles may not contain the state name. Check the live row as well.
            const bool savedCommand = VioLensSupport::IsCommand(modID, a_action.stateName, a_action.pageIndex, a_action.optionLabel);
            const bool liveCommand = VioLensSupport::IsCommand(modID, liveState.value_or(""), page ? page->index : -1, liveLabel.value_or(""));
            if (savedCommand || liveCommand) {
                return false;
            }
        }
        if (NLMCMSupport::IsSupported(script)) {
            // A matching state on another NL_MCM page belongs to a different module.
            auto page = script.ReadCurrentPage();
            if (!page || !page->Matches(a_action.pageName, a_action.pageIndex)) {
                return false;
            }
        }
        return script.MatchesControl(a_action.controlType, a_action.optionIndex, a_action.stateName);
    }

    bool Restore::IsActionNeeded(const RestoreAction& a_action) const
    {
        if (a_action.controlType == ControlType::Unknown || a_action.mcmIndex >= restoreMCMs.size()) {
            return true;
        }

        // Do not call a setting unchanged by reading the wrong page or control.
        if (!IsActionValid(a_action)) {
            return true;
        }

        // The displayed key can match while its stored setting or runtime binding still needs the callback.
        if (a_action.type == RestoreActionType::SetIntegerSetting || a_action.type == RestoreActionType::ChangeKeymap || a_action.type == RestoreActionType::ChangeStateKeymap) {
            return true;
        }

        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
        if (a_action.type == RestoreActionType::ApplyCycle) {
            const auto* cycle = VioLensSupport::FindCycle(a_action.stringValue);
            auto value = cycle ? VioLensSupport::ReadCycleValue(script, *cycle) : std::nullopt;
            return a_action.refreshingCycle || !value || *value != a_action.integerValue;
        }
        if (a_action.type == RestoreActionType::SetMenuIndex) {
            auto currentIndex = script.ReadMenuIndex();
            return !currentIndex || *currentIndex != a_action.integerValue;
        }

        auto currentValue = script.ReadCurrentValue(a_action.controlType, a_action.optionIndex);
        if (!currentValue) {
            return true;
        }

        switch (a_action.controlType) {
        case ControlType::Option:
            return !currentValue->is_boolean() || currentValue->get<bool>() != a_action.boolValue;
        case ControlType::Slider:
            return !currentValue->is_number() || std::abs(currentValue->get<float>() - a_action.floatValue) > 0.0001F;
        case ControlType::Color:
        case ControlType::Keymap:
            return !currentValue->is_number_integer() || currentValue->get<int>() != a_action.integerValue;
        case ControlType::Input:
            return !currentValue->is_string() || currentValue->get<std::string>() != a_action.stringValue;
        default:
            return true;
        }
    }

    void Restore::CompleteCycleAction(RestoreAction& a_action, bool a_continue)
    {
        if (a_continue && !a_action.refreshingCycle) {
            // ForcePageReset normally goes through the visible menu, which is closed during restore.
            a_action.refreshingCycle = true;
            currentActionIndex = pendingActionIndex;
            return;
        }
        a_action.refreshingCycle = false;
        const auto* cycle = VioLensSupport::FindCycle(a_action.stringValue);
        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
        auto value = cycle ? VioLensSupport::ReadCycleValue(script, *cycle) : std::nullopt;
        if (a_continue && value && *value == a_action.integerValue) {
            ++mcmStats.appliedSettingCount;
            logger::info("Restored cycling setting '{}' in {} clicks (value {})", a_action.stringValue, a_action.cycleClicks, *value);
        }
        else if (a_continue && cycle && value && *value != a_action.previousCycleValue && a_action.cycleClicks < cycle->valueCount - 1) {
            currentActionIndex = pendingActionIndex;
            return;
        }
        else {
            ++mcmStats.skippedSettingCount;
            logger::warn("Cycling setting '{}' stopped after {} clicks without a confirmed restore", a_action.stringValue, a_action.cycleClicks);
        }
        a_action.completed = true;
    }

    bool Restore::RunAction(RestoreAction& a_action, SKSE::TaskInterface::TaskFn a_result)
    {
        if (a_action.mcmIndex >= restoreMCMs.size()) {
            return false;
        }

        if (a_action.type == RestoreActionType::ApplyCycle) {
            if (a_action.refreshingCycle) {
                return CallMCMFunction(a_action.mcmIndex, "SetPage", RE::MakeFunctionArguments(std::string{ a_action.pageName }, int{ a_action.pageIndex }), std::move(a_result));
            }
            MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
            const auto* cycle = VioLensSupport::FindCycle(a_action.stringValue);
            auto index = cycle ? VioLensSupport::FindCycleIndex(script, *cycle, true) : std::nullopt;
            auto value = cycle ? VioLensSupport::ReadCycleValue(script, *cycle) : std::nullopt;
            if (!cycle || !index || !value || a_action.cycleClicks >= cycle->valueCount - 1) {
                return false;
            }
            a_action.previousCycleValue = *value;
            ++a_action.cycleClicks;
            return CallMCMFunction(a_action.mcmIndex, "SelectOption", RE::MakeFunctionArguments(int{ *index }), std::move(a_result));
        }

        auto functionName = RestoreActionFunctionName(a_action.type);
        if (a_action.type == RestoreActionType::SetIntegerSetting) {
            logger::debug("Profile restore calls '{}' on '{}' for setting '{}' (key {})", functionName, restoreMCMs[a_action.mcmIndex].identity.modID, a_action.stringValue, a_action.integerValue);
        }
        else if (a_action.controlType == ControlType::Keymap && IsRestoreApplyAction(a_action.type)) {
            logger::debug("Profile restore calls '{}' on '{}' for '{}' (option {}, key {})", functionName, restoreMCMs[a_action.mcmIndex].identity.modID, a_action.optionLabel, a_action.optionIndex, a_action.integerValue);
        }
        // Build the argument list expected by this script call.
        switch (GetRestoreArgumentType(a_action.type)) {
        case RestoreArgumentType::None:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(), std::move(a_result));
        case RestoreArgumentType::Page:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(std::string{ a_action.pageName }, int{ a_action.pageIndex }), std::move(a_result));
        case RestoreArgumentType::OptionIndex:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.optionIndex }), std::move(a_result));
        case RestoreArgumentType::IntegerValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.integerValue }), std::move(a_result));
        case RestoreArgumentType::FloatValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(float{ a_action.floatValue }), std::move(a_result));
        case RestoreArgumentType::StringValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(std::string{ a_action.stringValue }), std::move(a_result));
        case RestoreArgumentType::SettingIntegerValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(std::string{ a_action.stringValue }, int{ a_action.integerValue }), std::move(a_result));
        case RestoreArgumentType::KeymapValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.optionIndex }, int{ a_action.integerValue }, std::string{}, std::string{}), std::move(a_result));
        case RestoreArgumentType::ToggleValue:
            return RestoreToggle(a_action, std::move(a_result));
        }
        return false;
    }
}
