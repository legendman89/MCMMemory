#include "mcm/mcm_messages.hpp"
#include "mcm/mcm_calls.hpp"

namespace MCMMemory
{
    bool MCMMessageFunction::Install(std::string_view a_scriptName, std::string_view a_functionName, std::span<const RE::BSScript::TypeInfo::RawType> a_parameters, MCMMessageDispatch a_dispatch)
    {
        if (function) {
            return true;
        }

        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();

        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> scriptType;
        if (!vm || !vm->GetScriptObjectType(RE::BSFixedString(a_scriptName), scriptType) || !scriptType || !scriptType->IsLinked()) {
            logger::error("MCM message handling could not find the '{}' script type; the watchdog remains active", a_scriptName);
            return false;
        }

        // Fetch all functions in the script and find the one that matches the name and signature.
        const auto* functions = scriptType->GetGlobalFuncIter();
        for (uint32_t index = 0; functions && index < scriptType->GetNumGlobalFuncs(); ++index) {
            auto candidate = functions[index].func;
            if (!candidate || candidate->GetName() != RE::BSFixedString(a_functionName)) {
                continue;
            }
            
            if (!candidate->GetIsNative() || !candidate->GetIsStatic() || candidate->GetParamCount() != a_parameters.size() || candidate->GetReturnType() != RE::BSScript::TypeInfo(RawType::kNone)) {
                logger::error("MCM message handling found an unexpected {}.{} signature; leaving it unchanged", a_scriptName, a_functionName);
                return false;
            }

            // Now try to match function parameters.
            for (uint32_t parameter = 0; parameter < a_parameters.size(); ++parameter) {
                RE::BSFixedString name;
                RE::BSScript::TypeInfo type;
                candidate->GetParam(parameter, name, type);
                if (type != RE::BSScript::TypeInfo(a_parameters[parameter])) {
                    logger::error("MCM message handling found an unexpected {}.{} parameter {}; leaving it unchanged", a_scriptName, a_functionName, parameter);
                    return false;
                }
            }

            // Latent functions suspend their calling script, therefore this hook shouldn't handle resuming them.
            const auto* nativeFunction = static_cast<const RE::BSScript::NF_util::NativeFunctionBase*>(candidate.get());
            if (nativeFunction->GetIsLatent()) {
                logger::error("MCM message handling cannot bypass latent {}.{}; leaving it unchanged", a_scriptName, a_functionName);
                return false;
            }

            // Keep the original dispatch while replacing only this function table.
            const auto* originalTable = *reinterpret_cast<const uintptr_t* const*>(candidate.get());
            std::copy_n(originalTable - 1, replacementVTable.size(), replacementVTable.begin());

            originalDispatch = reinterpret_cast<MCMMessageDispatch>(originalTable[dispatchIndex]);

            replacementVTable[dispatchIndex + 1] = reinterpret_cast<uintptr_t>(a_dispatch);

            const auto* replacementTable = replacementVTable.data() + 1;
            if (!REL::safe_write(reinterpret_cast<uintptr_t>(candidate.get()), &replacementTable, sizeof(replacementTable), &originalTable, sizeof(originalTable))) {
                logger::error("MCM message handling could not install its {}.{} hook", a_scriptName, a_functionName);
                return false;
            }
            function = std::move(candidate);
            
            logger::info("MCM message handling installed for {}.{} during watched calls", a_scriptName, a_functionName);

            return true;
        }

        logger::error("MCM message handling could not find {}.{}; the watchdog remains active", a_scriptName, a_functionName);

        return false;
    }

    bool MCMMessages::Install()
    {
        constexpr std::array skyUIParameters{ RawType::kString, RawType::kString, RawType::kStringArray };
        constexpr std::array messageBoxParameters{ RawType::kString };
        const bool skyUIInstalled = skyUIMessage.Install("UI", "InvokeStringA", skyUIParameters, &Dispatch);
        const bool messageBoxInstalled = debugMessageBox.Install("Debug", "MessageBox", messageBoxParameters, &Dispatch);
        return skyUIInstalled && messageBoxInstalled;
    }

    bool MCMMessages::Dispatch(const RE::BSScript::NF_util::NativeFunctionBase* a_function, RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm, RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame)
    {
        const bool nativeMessageBox = a_function == debugMessageBox.function.get();
        if (nativeMessageBox ? HandleMessageBox(a_frame) : HandleMessage(a_frame)) {
            a_result.SetNone();
            return true;
        }

        const auto originalDispatch = nativeMessageBox ? debugMessageBox.originalDispatch : skyUIMessage.originalDispatch;
        return originalDispatch(a_function, a_base, a_vm, a_stackID, a_result, a_frame);
    }

    bool MCMMessages::HandleMessageBox(const RE::BSScript::StackFrame& a_frame)
    {
        const auto call = activeCall.load();
        if (!call || !call->dismissMessageBoxes || call->completed.load(std::memory_order_acquire) || !a_frame.parent || a_frame.parent->callback.get() != call->callback) {
            return false;
        }

        const auto& message = a_frame.GetStackFrameVariable(0, a_frame.GetPageForFrame());
        if (!message.IsString()) {
            return false;
        }

        logger::info("Dismissed Debug.MessageBox '{}' during '{}' on '{}'", message.GetString(), call->functionName, call->modID);
       
        return true;
    }

    bool MCMMessages::HandleMessage(const RE::BSScript::StackFrame& a_frame)
    {
        const auto call = activeCall.load();
        if (!call || call->completed.load(std::memory_order_acquire) || !a_frame.parent || a_frame.parent->callback.get() != call->callback) {
            return false;
        }

        // Inspect the caller on its own VM thread, not from the watchdog game task.
        const auto* caller = a_frame.previousFrame;
        const auto* callerFunction = caller ? caller->owningFunction.get() : nullptr;
        if (!callerFunction || callerFunction->GetName() != RE::BSFixedString("ShowMessage") || callerFunction->GetObjectTypeName() != RE::BSFixedString("SKI_ConfigBase") || callerFunction->GetParamCount() != 4 || !caller->self.IsObject()) {
            return false;
        }

        const auto page = a_frame.GetPageForFrame();
        const auto& menu = a_frame.GetStackFrameVariable(0, page);
        const auto& target = a_frame.GetStackFrameVariable(1, page);
        const auto& arguments = a_frame.GetStackFrameVariable(2, page);
        if (!menu.IsString() || menu.GetString() != "Journal Menu" || !target.IsString() || !target.GetString().ends_with(".showMessageDialog") || !arguments.IsArray()) {
            return false;
        }

        const auto parameters = arguments.GetArray();
        if (!parameters || parameters->size() != 3 || !(*parameters)[0].IsString() || !(*parameters)[1].IsString() || !(*parameters)[2].IsString()) {
            return false;
        }

        const auto& withCancel = caller->GetStackFrameVariable(1, caller->GetPageForFrame());
        if (!withCancel.IsBool()) {
            return false;
        }

        auto script = caller->self.GetObject();
        auto* waiting = script ? script->GetVariable(RE::BSFixedString("_waitForMessage")) : nullptr;
        auto* result = script ? script->GetVariable(RE::BSFixedString("_messageResult")) : nullptr;
        if (!waiting || !waiting->IsBool() || !waiting->GetBool() || !result || !result->IsBool()) {
            return false;
        }

        // This is the same result SkyUI sets after a click. ShowMessage then unregisters
        // its listener and returns normally, without opening an invisible dialog.
        const bool confirmation = withCancel.GetBool();
        const bool accepted = !confirmation || call->acceptConfirmation;
        result->SetBool(accepted);
        waiting->SetBool(false);
        if (confirmation && !accepted) {
            call->confirmationDeclined.store(true, std::memory_order_release);
            logger::warn("Declined MCM confirmation '{}' during '{}' on '{}'; this call is incomplete", (*parameters)[0].GetString(), call->functionName, call->modID);
        }
        else if (confirmation) {
            logger::info("Accepted MCM confirmation '{}' during '{}' on '{}'", (*parameters)[0].GetString(), call->functionName, call->modID);
        }
        else {
            logger::info("Acknowledged MCM message '{}' during '{}' on '{}'", (*parameters)[0].GetString(), call->functionName, call->modID);
        }
        
        return true;
    }
}
