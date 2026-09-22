#include "profile/restore.hpp"
#include "mcm/mcm_support.hpp"

#include <cmath>

namespace MCMMemory
{
    bool Restore::CallMCMFunction(size_t a_mcmIndex, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, std::function<void()> a_result, bool a_acceptConfirmation, bool a_allowLongCall)
    {
        if (a_mcmIndex >= restoreMCMs.size()) {
            delete a_arguments;
            return false;
        }
        const auto& mcm = restoreMCMs[a_mcmIndex];
        return callWatch.Call(MCMScript(mcm.mcmScript), mcm.identity.modID, a_functionName, a_arguments, std::move(a_result), a_acceptConfirmation, a_allowLongCall);
    }

    bool Restore::RestoreToggle(const RestoreAction& a_action, std::function<void()> a_result)
    {
        return CallMCMFunction(a_action.mcmIndex, RestoreActionFunctionName(a_action.type), RE::MakeFunctionArguments(int{ a_action.optionIndex }), std::move(a_result));
    }

    bool Restore::IsActionValid(const RestoreAction& a_action) const
    {
        if ((!a_action.command && a_action.controlType == ControlType::Unknown) || a_action.mcmIndex >= restoreMCMs.size()) {
            return true;
        }

        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
        auto page = script.ReadCurrentPage();
        if (!page || !page->Matches(a_action.pageName, a_action.pageIndex) || !script.IsPageReady(a_action.pageIndex)) {
            return false;
        }

        if (a_action.command) {
            auto control = script.ReadControl(a_action.optionIndex);
            return control && script.CanSelectOption(a_action.optionIndex) && control->type == a_action.controlType && control->optionLabel == a_action.optionLabel && control->stateName == a_action.stateName && !IsProfileWriteCommand(control->optionLabel, control->stateName);
        }

        if (a_action.type == RestoreActionType::ApplyClicks) {
            if (a_action.refreshingCycle) {
                return true;
            }
            // Without a typed value, the label, state and displayed value are the only identity.
            auto label = script.ReadOptionLabel(a_action.optionIndex);
            auto text = script.ReadOptionText(a_action.optionIndex);
            if (!script.IsTextControl(a_action.optionIndex) || !label || *label != a_action.optionLabel || !text || text->empty()) {
                return false;
            }
            return a_action.stateName.empty() || script.ReadStateName(a_action.optionIndex).value_or("") == a_action.stateName;
        }

        if (a_action.type == RestoreActionType::ApplyCycle) {
            const auto& modID = restoreMCMs[a_action.mcmIndex].identity.modID;
            const auto* cycle = SkyUICycleSupport::Find(modID, a_action.stringValue);
            if (!cycle || a_action.optionLabel != cycle->profileLabel || !a_action.stateName.empty()) {
                return false;
            }
            if (a_action.refreshingCycle || SkyUICycleSupport::FindOption(script, *cycle, true)) {
                return true;
            }
            // A matching disabled value needs no click. A different one must still be skipped safely.
            auto value = SkyUICycleSupport::ReadValue(script, *cycle);
            return value && *value == a_action.integerValue && SkyUICycleSupport::FindOption(script, *cycle).has_value();
        }

        if (VioLensSupport::IsSupported(script)) {
            auto liveState = script.ReadStateName(a_action.optionIndex);
            auto liveLabel = script.ReadOptionLabel(a_action.optionIndex);
            const auto& modID = restoreMCMs[a_action.mcmIndex].identity.modID;
            // Older profiles may not contain the state name. Check the current row as well.
            const bool savedCommand = MCMCommandSupport::IsIgnored(modID, a_action.pageName, a_action.pageIndex, a_action.controlType, a_action.stateName, a_action.optionLabel);
            const bool liveCommand = MCMCommandSupport::IsIgnored(modID, page->name, page->index, a_action.controlType, liveState.value_or(""), liveLabel.value_or(""));
            if (savedCommand || liveCommand) {
                return false;
            }
        }

        if (!script.MatchesControl(a_action.controlType, a_action.optionIndex, a_action.stateName)) {
            return false;
        }
        if (!a_action.stateName.empty()) {
            return true;
        }

        // A rebuilt page can put another control of the same type at this index.
        auto label = script.ReadOptionLabel(a_action.optionIndex);
        return label && !a_action.optionLabel.empty() && *label == a_action.optionLabel;
    }

    bool Restore::IsActionPageReady(const RestoreAction& a_action) const
    {
        if (a_action.mcmIndex >= restoreMCMs.size()) {
            return true;
        }
        return MCMScript(restoreMCMs[a_action.mcmIndex].mcmScript).IsPageReady(a_action.pageIndex);
    }

    bool Restore::IsActionNeeded(const RestoreAction& a_action) const
    {
        if (a_action.command) {
            return true;
        }
        if (a_action.controlType == ControlType::Unknown || a_action.mcmIndex >= restoreMCMs.size()) {
            return true;
        }

        // Do not call a setting unchanged by reading the wrong page or control.
        if (!IsActionValid(a_action)) {
            return true;
        }

        // The displayed key can match while its stored setting or runtime binding still needs the callback.
        if (a_action.type == RestoreActionType::SetIntegerSetting || a_action.type == RestoreActionType::ChangeKeymap) {
            return true;
        }

        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);

        // SkyUI fills its value buffers one page at a time. Reading them while the page is still
        // building would risk matching an "already matches" state.
        if (!script.IsPageReady(a_action.pageIndex)) {
            logger::debug("Page {} of '{}' is unsettled; restoring '{}' without comparing its current value", a_action.pageIndex, restoreMCMs[a_action.mcmIndex].identity.modID, a_action.optionLabel);
            return true;
        }

        if (a_action.type == RestoreActionType::ApplyClicks) {
            auto text = script.ReadOptionText(a_action.optionIndex);
            return a_action.refreshingCycle || !text || *text != a_action.stringValue;
        }

        if (a_action.type == RestoreActionType::ApplyCycle) {
            const auto& modID = restoreMCMs[a_action.mcmIndex].identity.modID;
            const auto* cycle = SkyUICycleSupport::Find(modID, a_action.stringValue);
            auto value = cycle ? SkyUICycleSupport::ReadValue(script, *cycle) : std::nullopt;
            return a_action.refreshingCycle || !value || *value != a_action.integerValue;
        }

        if (a_action.type == RestoreActionType::SetMenuIndex) {
            // The shown text belongs to this row, while _menuParams only holds whatever control
            // asked for dialog data last, so the text is safer and needs no script call.
            if (!a_action.valueText.empty()) {
                auto text = script.ReadOptionText(a_action.optionIndex);
                return !text || *text != a_action.valueText;
            }
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

    void Restore::VerifyKeymapAction(const RestoreAction& a_action) const
    {
        if (a_action.mcmIndex >= restoreMCMs.size()) {
            return;
        }
        auto shownKeyCode = MCMScript(restoreMCMs[a_action.mcmIndex].mcmScript).ReadCurrentValue(ControlType::Keymap, a_action.optionIndex);
        if (shownKeyCode && shownKeyCode->is_number_integer() && shownKeyCode->get<int>() == a_action.integerValue) {
            return;
        }
        // An MCM that keeps the key only in its own variable never refreshes the row, 
        // so this is the place a lost remap can be checked in log.
        logger::warn("Keymap '{}' in '{}' does not show key {} after the remap", a_action.optionLabel, restoreMCMs[a_action.mcmIndex].identity.modID, a_action.integerValue);
    }

    void Restore::CompleteClicksAction(RestoreAction& a_action, bool a_continue)
    {
        if (a_continue && !a_action.refreshingCycle) {
            // The row shows its new value only after the page is rebuilt.
            a_action.refreshingCycle = true;
            currentActionIndex = pendingActionIndex;
            return;
        }

        a_action.refreshingCycle = false;
        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
        auto text = script.ReadOptionText(a_action.optionIndex);
        if (a_continue && text && *text == a_action.stringValue) {
            ++mcmStats.appliedSettingCount;
            logger::info("Restored text setting '{}' in {} clicks (value '{}')", a_action.optionLabel, a_action.cycleClicks, *text);
        }

        // Stop when clicking stops changing the value, when it comes back to where it started,
        // or when the recorded value simply is not among this control choices any more.
        else if (a_continue && text && *text != a_action.previousCycleText && *text != a_action.startText && a_action.cycleClicks < maximumRecordedClicks) {
            currentActionIndex = pendingActionIndex;
            return;
        }
        else {
            ++mcmStats.skippedSettingCount;
            logger::warn("Text setting '{}' stopped after {} clicks without reaching '{}'", a_action.optionLabel, a_action.cycleClicks, a_action.stringValue);
        }
        a_action.completed = true;
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
        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
        const auto& modID = restoreMCMs[a_action.mcmIndex].identity.modID;
        const auto* cycle = SkyUICycleSupport::Find(modID, a_action.stringValue);
        auto value = cycle ? SkyUICycleSupport::ReadValue(script, *cycle) : std::nullopt;
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

    bool Restore::RunAction(RestoreAction& a_action, std::function<void()> a_result)
    {
        if (a_action.mcmIndex >= restoreMCMs.size()) {
            return false;
        }

        if (a_action.type == RestoreActionType::ApplyCommand) {
            logger::info("Replaying command '{}' in '{}'", a_action.optionLabel, restoreMCMs[a_action.mcmIndex].identity.modID);
            return CallMCMFunction(a_action.mcmIndex, "SelectOption", RE::MakeFunctionArguments(int{ a_action.optionIndex }), std::move(a_result), a_action.confirmedCommand, true);
        }

        if (a_action.type == RestoreActionType::ApplyClicks) {
            if (a_action.refreshingCycle) {
                return CallMCMFunction(a_action.mcmIndex, "SetPage", RE::MakeFunctionArguments(std::string{ a_action.pageName }, int{ a_action.pageIndex }), std::move(a_result));
            }

            MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
            auto text = script.ReadOptionText(a_action.optionIndex);
            if (!text || a_action.cycleClicks >= maximumRecordedClicks) {
                return false;
            }
            if (a_action.cycleClicks == 0) {
                a_action.startText = *text;
            }
            a_action.previousCycleText = *text;
            ++a_action.cycleClicks;
            return CallMCMFunction(a_action.mcmIndex, "SelectOption", RE::MakeFunctionArguments(int{ a_action.optionIndex }), std::move(a_result));
        }

        if (a_action.type == RestoreActionType::ApplyCycle) {
            if (a_action.refreshingCycle) {
                return CallMCMFunction(a_action.mcmIndex, "SetPage", RE::MakeFunctionArguments(std::string{ a_action.pageName }, int{ a_action.pageIndex }), std::move(a_result));
            }
            MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
            const auto& modID = restoreMCMs[a_action.mcmIndex].identity.modID;
            const auto* cycle = SkyUICycleSupport::Find(modID, a_action.stringValue);
            auto index = cycle ? SkyUICycleSupport::FindOption(script, *cycle, true) : std::nullopt;
            auto value = cycle ? SkyUICycleSupport::ReadValue(script, *cycle) : std::nullopt;
            if (!cycle || !index || !value || a_action.cycleClicks >= cycle->valueCount - 1) {
                return false;
            }
            a_action.previousCycleValue = *value;
            ++a_action.cycleClicks;
            return CallMCMFunction(a_action.mcmIndex, "SelectOption", RE::MakeFunctionArguments(int{ *index }), std::move(a_result));
        }

        // The MCM activation control is not a setting, so it is handled separately.
        if (a_action.type == RestoreActionType::ActivateMCM) {
            MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
            const auto& activation = restoreMCMs[a_action.mcmIndex].activation;
            auto optionIndex = activation ? MCMActivationSupport::FindOption(script, *activation) : std::nullopt;
            if (!optionIndex) {
                logger::error("Could not find the recorded MCM activation control in '{}'", restoreMCMs[a_action.mcmIndex].identity.modID);
                return false;
            }
            a_action.optionIndex = *optionIndex;
            logger::info("Activating MCM '{}' before restoring its settings", restoreMCMs[a_action.mcmIndex].identity.modID);
            return CallMCMFunction(a_action.mcmIndex, "SelectOption", RE::MakeFunctionArguments(int{ *optionIndex }), std::move(a_result), true, true);
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
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.integerValue }), std::move(a_result), a_action.command && a_action.confirmedCommand, a_action.command);
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
