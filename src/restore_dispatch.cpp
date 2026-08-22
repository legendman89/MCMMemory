#include "restore.hpp"

#include <cmath>

namespace MCMMemory
{
    bool Restore::CallMCMFunction(size_t a_mcmIndex, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result)
    {
        if (a_mcmIndex >= restoreMCMs.size()) {
            return false;
        }
        return MCMScript(restoreMCMs[a_mcmIndex].mcmScript).Call(a_functionName, a_arguments, std::move(a_result));
    }

    bool Restore::RestoreToggle(const RestoreAction& a_action, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result)
    {
        return CallMCMFunction(a_action.mcmIndex, RestoreActionFunctionName(a_action.type), RE::MakeFunctionArguments(int{ a_action.optionIndex }), std::move(a_result));
    }

    bool Restore::IsActionValid(const RestoreAction& a_action) const
    {
        if (a_action.controlType == ControlType::Unknown || a_action.mcmIndex >= restoreMCMs.size()) {
            return true;
        }
        return MCMScript(restoreMCMs[a_action.mcmIndex].mcmScript).MatchesControl(a_action.controlType, a_action.optionIndex, a_action.stateName);
    }

    bool Restore::IsActionNeeded(const RestoreAction& a_action) const
    {
        if (a_action.controlType == ControlType::Unknown || a_action.mcmIndex >= restoreMCMs.size()) {
            return true;
        }

        MCMScript script(restoreMCMs[a_action.mcmIndex].mcmScript);
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

    bool Restore::RunAction(const RestoreAction& a_action, RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> a_result)
    {
        if (a_action.mcmIndex >= restoreMCMs.size()) {
            return false;
        }

        auto functionName = RestoreActionFunctionName(a_action.type);
        logger::debug("Profile restore calls '{}' on '{}'", functionName, restoreMCMs[a_action.mcmIndex].identity.modID);
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
        case RestoreArgumentType::KeymapValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.optionIndex }, int{ a_action.integerValue }, std::string{}, std::string{}), std::move(a_result));
        case RestoreArgumentType::ToggleValue:
            return RestoreToggle(a_action, std::move(a_result));
        }
        return false;
    }
}
