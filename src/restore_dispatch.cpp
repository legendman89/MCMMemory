#include "restore.hpp"

namespace MCMMemory
{
    bool Restore::CallMCMFunction(size_t a_mcmIndex, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments)
    {
        // Call the function directly on the live MCM script.
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || a_mcmIndex >= restoreMCMs.size() || !restoreMCMs[a_mcmIndex].mcmScript) {
            return false;
        }

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
        auto mcmScript = restoreMCMs[a_mcmIndex].mcmScript;
        return vm->DispatchMethodCall(mcmScript, RE::BSFixedString(a_functionName), a_arguments, result);
    }

    bool Restore::ReadToggleValue(size_t a_mcmIndex, int a_optionIndex, bool& a_value) const
    {
        if (a_mcmIndex >= restoreMCMs.size() || !restoreMCMs[a_mcmIndex].mcmScript || a_optionIndex < 0) {
            return false;
        }

        // SelectOption only flips a toggle, so read its current state first.
        const RE::BSScript::Variable* bufferValue = restoreMCMs[a_mcmIndex].mcmScript->GetVariable("_numValueBuf");
        if (!bufferValue || !bufferValue->IsArray()) {
            return false;
        }

        auto buffer = bufferValue->GetArray();
        uint32_t bufferIndex = static_cast<uint32_t>(a_optionIndex % 256);
        if (!buffer || bufferIndex >= buffer->size()) {
            return false;
        }

        const auto& value = (*buffer)[bufferIndex];
        if (value.IsBool()) {
            a_value = value.GetBool();
            return true;
        }
        if (value.IsFloat()) {
            a_value = value.GetFloat() != 0.0F;
            return true;
        }
        if (value.IsInt()) {
            a_value = value.GetSInt() != 0;
            return true;
        }
        return false;
    }

    bool Restore::RestoreToggle(const RestoreAction& a_action)
    {
        bool currentValue{};
        if (!ReadToggleValue(a_action.mcmIndex, a_action.optionIndex, currentValue)) {
            logger::error("Could not read live toggle state for option {}", a_action.optionIndex);
            return false;
        }

        logger::info("Toggle option {} is {} and profile requests {}", a_action.optionIndex, currentValue, a_action.boolValue);
        // Leave it alone when it already matches the profile.
        if (currentValue == a_action.boolValue) {
            return true;
        }
        return CallMCMFunction(a_action.mcmIndex, RestoreActionFunctionName(a_action.type), RE::MakeFunctionArguments(int{ a_action.optionIndex }));
    }

    bool Restore::RunAction(const RestoreAction& a_action)
    {
        if (a_action.mcmIndex >= restoreMCMs.size()) {
            return false;
        }

        auto functionName = RestoreActionFunctionName(a_action.type);
        logger::info("Profile restore calls '{}' on '{}'", functionName, restoreMCMs[a_action.mcmIndex].identity.modID);
        // Build the argument list expected by this script call.
        switch (GetRestoreArgumentType(a_action.type)) {
        case RestoreArgumentType::None:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments());
        case RestoreArgumentType::Page:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(std::string{ a_action.pageName }, int{ a_action.pageIndex }));
        case RestoreArgumentType::OptionIndex:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.optionIndex }));
        case RestoreArgumentType::IntegerValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.integerValue }));
        case RestoreArgumentType::FloatValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(float{ a_action.floatValue }));
        case RestoreArgumentType::StringValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(std::string{ a_action.stringValue }));
        case RestoreArgumentType::KeymapValue:
            return CallMCMFunction(a_action.mcmIndex, functionName, RE::MakeFunctionArguments(int{ a_action.optionIndex }, int{ a_action.integerValue }, std::string{}, std::string{}));
        case RestoreArgumentType::ToggleValue:
            return RestoreToggle(a_action);
        }
        return false;
    }
}
