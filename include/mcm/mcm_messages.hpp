#pragma once

#include "plugin.hpp"
#include <span>

namespace MCMMemory
{
    struct MCMCallState;

    using RawType = RE::BSScript::TypeInfo::RawType;
    using MCMMessageDispatch = bool (*)(const RE::BSScript::NF_util::NativeFunctionBase*, RE::BSScript::Variable&, RE::BSScript::Internal::VirtualMachine&, RE::VMStackID, RE::BSScript::Variable&, const RE::BSScript::StackFrame&);

    // Each native function gets its own vtable so irrelevant Papyrus functions are untouched.
    // Grouped what I need in a struct to reuse with Debug and SkyUI dialogs.
    // SkyUI Community via SKI_ConfigBase.ShowMessage uses UI.InvokeStringA to open SkyUI dialogs.
    struct MCMMessageFunction
    {
        bool Install(std::string_view a_scriptName, std::string_view a_functionName, std::span<const RE::BSScript::TypeInfo::RawType> a_parameters, MCMMessageDispatch a_dispatch);

        RE::BSTSmartPointer<RE::BSScript::IFunction> function;

        MCMMessageDispatch originalDispatch{};

        std::array<uintptr_t, 24> replacementVTable{};

        static constexpr size_t dispatchIndex{ 0x16 };
    };

    // Handles dialogs only when they belong to our current backup/restore call.
    struct MCMMessages
    {
        static bool Install();

        static inline void Track(std::shared_ptr<MCMCallState> a_call) { activeCall.store(std::move(a_call)); }

        static inline void StopTracking(std::shared_ptr<MCMCallState> a_call)
        {
            activeCall.compare_exchange_strong(a_call, {});
        }

    private:

        static bool Dispatch(const RE::BSScript::NF_util::NativeFunctionBase* a_function, RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm, RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame);

        static bool HandleMessage(const RE::BSScript::StackFrame& a_frame);

        static bool HandleMessageBox(const RE::BSScript::StackFrame& a_frame);

        static inline MCMMessageFunction skyUIMessage, debugMessageBox;

        static inline std::atomic<std::shared_ptr<MCMCallState>> activeCall;
    };
}
