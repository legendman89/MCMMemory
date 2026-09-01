#pragma once

#include "plugin.hpp"

namespace MCMMemory
{
    struct MCMCallState;

    // Handles SkyUI dialogs only when they belong to our current backup/restore call.
    struct MCMMessages
    {
        static bool Install();

        static inline void Track(std::shared_ptr<MCMCallState> a_call) { activeCall.store(std::move(a_call)); }

        static inline void StopTracking(std::shared_ptr<MCMCallState> a_call)
        {
            activeCall.compare_exchange_strong(a_call, {});
        }

    private:

        using DispatchFunction = bool (*)(const RE::BSScript::NF_util::NativeFunctionBase*, RE::BSScript::Variable&, RE::BSScript::Internal::VirtualMachine&, RE::VMStackID, RE::BSScript::Variable&, const RE::BSScript::StackFrame&);

        static bool Dispatch(const RE::BSScript::NF_util::NativeFunctionBase* a_function, RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm, RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame);

        static bool HandleMessage(const RE::BSScript::StackFrame& a_frame);

        static bool HasExpectedSignature(const RE::BSScript::IFunction& a_function);

        // Keep the original UI function alive and forward every unrelated request to it.
        static inline RE::BSTSmartPointer<RE::BSScript::IFunction> function;

        static inline DispatchFunction originalDispatch{};

        // SkyUI Community's SKI_ConfigBase.ShowMessage uses UI.InvokeStringA to open MCM dialogs.
        // Native Papyrus functions share their dispatch code. Replacing the shared vtable
        // would affect unrelated functions, so give only UI.InvokeStringA a copied table
        // with its dispatch entry replaced.
        static inline std::array<uintptr_t, 24> replacementVTable{};

        static inline std::atomic<std::shared_ptr<MCMCallState>> activeCall;

        // UI.InvokeStringA dispatch vtable index.
        static constexpr size_t dispatchIndex{ 0x16 };
    };
}
