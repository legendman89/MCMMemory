#include "mcm/mcm_messages.hpp"
#include "mcm/mcm_calls.hpp"

namespace MCMMemory
{
    bool MCMMessages::HasExpectedSignature(const RE::BSScript::IFunction& a_function)
    {
        using Type = RE::BSScript::TypeInfo::RawType;
        if (!a_function.GetIsNative() || !a_function.GetIsStatic() || a_function.GetParamCount() != 3 || a_function.GetReturnType() != RE::BSScript::TypeInfo(Type::kNone)) {
            return false;
        }
        constexpr std::array expectedTypes{ Type::kString, Type::kString, Type::kStringArray };
        for (uint32_t index = 0; index < expectedTypes.size(); ++index) {
            RE::BSFixedString name;
            RE::BSScript::TypeInfo type;
            a_function.GetParam(index, name, type);
            if (type != RE::BSScript::TypeInfo(expectedTypes[index])) {
                return false;
            }
        }
        return true;
    }

    bool MCMMessages::Install()
    {
        if (function) {
            return true;
        }
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> uiType;
        if (!vm || !vm->GetScriptObjectType(RE::BSFixedString("UI"), uiType) || !uiType || !uiType->IsLinked()) {
            logger::error("SkyUI message handling could not find the UI script type; the watchdog remains active");
            return false;
        }

        const auto* functions = uiType->GetGlobalFuncIter();
        for (uint32_t index = 0; functions && index < uiType->GetNumGlobalFuncs(); ++index) {

            // Find UI.InvokeStringA and verify its signature before changing its dispatch function.
            auto candidate = functions[index].func;
            if (!candidate || candidate->GetName() != RE::BSFixedString("InvokeStringA")) {
                continue;
            }
            if (!HasExpectedSignature(*candidate)) {
                logger::error("SkyUI message handling found an unexpected UI.InvokeStringA signature; leaving it unchanged");
                return false;
            }

            // Here we swap the vtable of the UI.InvokeStringA function with a copy of its original table, replacing only the dispatch entry.
            const auto* originalTable = *reinterpret_cast<const uintptr_t* const*>(candidate.get());
            std::copy_n(originalTable - 1, replacementVTable.size(), replacementVTable.begin());

            originalDispatch = reinterpret_cast<DispatchFunction>(originalTable[dispatchIndex]);

            replacementVTable[dispatchIndex + 1] = reinterpret_cast<uintptr_t>(&Dispatch);

            const auto* replacementTable = replacementVTable.data() + 1;
            if (!REL::safe_write(reinterpret_cast<uintptr_t>(candidate.get()), &replacementTable, sizeof(replacementTable), &originalTable, sizeof(originalTable))) {
                logger::error("SkyUI message handling could not install its UI.InvokeStringA hook");
                return false;
            }
            function = std::move(candidate);
            
            logger::info("SkyUI message handling installed for watched backup/restore calls");

            return true;
        }

        logger::error("SkyUI message handling could not find UI.InvokeStringA; the watchdog remains active");

        return false;
    }

    bool MCMMessages::Dispatch(const RE::BSScript::NF_util::NativeFunctionBase* a_function, RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm, RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame)
    {
        if (HandleMessage(a_frame)) {
            a_result.SetNone();
            return true;
        }
        return originalDispatch(a_function, a_base, a_vm, a_stackID, a_result, a_frame);
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
        result->SetBool(!confirmation);
        waiting->SetBool(false);
        if (confirmation) {
            call->confirmationDeclined.store(true, std::memory_order_release);
            logger::warn("Declined MCM confirmation '{}' during '{}' on '{}'; this call is incomplete", (*parameters)[0].GetString(), call->functionName, call->modID);
        }
        else {
            logger::info("Acknowledged MCM message '{}' during '{}' on '{}'", (*parameters)[0].GetString(), call->functionName, call->modID);
        }
        
        return true;
    }
}
