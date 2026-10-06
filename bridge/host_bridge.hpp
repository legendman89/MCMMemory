// Authored by Wuerfelhusten to support MCM bridge

#pragma once

#include "bridge/MCMBridgeHost.h"

namespace MCMMemory
{
    // Manages exclusive access to MCM Bridge and sends script calls through it.
    class HostBridge
    {
    public:

        static bool Present() { return GetModuleHandleW(L"MCMBridge.dll") != nullptr; }

        bool Acquire(bool a_restore);

        void Release();

        void Cancel();

        void Recover();

        bool Active() const { return context != nullptr; }

        bool Call(std::string_view a_modID, std::string_view a_function, RE::BSScript::IFunctionArguments* a_arguments, float a_timeoutSeconds, bool a_acceptConfirmation, std::function<void(MCMHostResult, bool)> a_completion);

    private:

        struct Context
        {
            const MCMBridgeHost* api{};
            MCMHostContext token{};
            bool released{};
            bool cancelled{};
        };

        struct PendingCall
        {
            std::shared_ptr<Context> context;
            std::string modID;
            std::string function;
            std::vector<std::string> strings;
            std::vector<MCMHostArgument> arguments;
            std::function<void(MCMHostResult, bool)> completion;
            std::chrono::steady_clock::time_point deadline;
            uint32_t timeout{};
            bool acceptConfirmation{};
        };

        struct RetryTask
        {
            std::shared_ptr<PendingCall> call;

            void operator()() const;
        };

        static void MCM_HOST_CALL Complete(void* a_user, MCMHostResult a_result, uint32_t a_declined);

        static bool Retry(const std::shared_ptr<PendingCall>& a_call);

        static void Attempt(const std::shared_ptr<PendingCall>& a_call);
        
        std::shared_ptr<Context> context;
    };
}
