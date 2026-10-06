// Authored by Wuerfelhusten to support MCM bridge

#include "bridge/host_bridge.hpp"
#include "utils/scheduler.hpp"

#include <cmath>

namespace MCMMemory
{
    bool HostBridge::Acquire(bool)
    {
        const auto module = GetModuleHandleW(L"MCMBridge.dll");
        if (!module) {
            return true;
        }

        // Bridge replaces the Classic interface, so fallback is unsafe while it is loaded.
        const auto getHost = reinterpret_cast<MCMBridgeGetHost>(GetProcAddress(module, "MCMBridge_GetHost"));
        const auto* api = getHost ? getHost() : nullptr;
        if (!api || !api->is_ready || !api->begin_context || !api->invoke || !api->cancel_context || !api->end_context || !api->is_ready()) {
            logger::error("MCMBridge is loaded but its native host interface is unavailable; Classic fallback is disabled");
            return false;
        }

        auto next = std::make_shared<Context>();
        next->api = api;

        // Avoid pausing the whole restore so mods can finish starting up between calls.
        const auto result = api->begin_context("MCMMemory", 0, &next->token);
        if (result != MCM_HOST_OK || !next->token) {
            logger::error("MCMBridge could not acquire an exclusive host context (result {})", result);
            return false;
        }

        context = std::move(next);

        return true;
    }

    void HostBridge::Release()
    {
        if (auto current = std::exchange(context, {})) {
            current->released = true;
            if (current->token) {
                current->api->end_context(current->token);
            }
        }
    }

    void HostBridge::Cancel()
    {
        if (context) {
            context->cancelled = true;
            if (context->token) {
                context->api->cancel_context(context->token);
            }
        }
    }

    void HostBridge::Recover()
    {
        if (!context) {
            return;
        }

        auto next = std::make_shared<Context>();
        next->api = context->api;
        // End the failed MCM context before opening another one.
        Release();
        context = std::move(next);
    }

    bool HostBridge::Call(std::string_view a_modID, std::string_view a_function, RE::BSScript::IFunctionArguments* a_arguments,
        float a_timeoutSeconds, bool a_acceptConfirmation, std::function<void(MCMHostResult, bool)> a_completion)
    {
        const std::unique_ptr<RE::BSScript::IFunctionArguments> ownedArguments(a_arguments);

        // Copy arguments and strings so they survive retries while Bridge is busy.
        if (!context || !ownedArguments || !a_completion || !std::isfinite(a_timeoutSeconds) || a_timeoutSeconds <= 0 || a_timeoutSeconds > 3600) {
            return false;
        }
        RE::BSScrapArray<RE::BSScript::Variable> values;
        if (!(*ownedArguments)(values) || values.size() > 4) {
            return false;
        }

        auto call = std::make_shared<PendingCall>();
        call->context = context;
        call->modID = a_modID;
        call->function = a_function;
        call->timeout = static_cast<uint32_t>(a_timeoutSeconds * 1000.0F);
        call->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(call->timeout);
        call->acceptConfirmation = a_acceptConfirmation;
        call->completion = std::move(a_completion);
        call->strings.reserve(values.size());
        call->arguments.reserve(values.size());
        for (const auto& value : values) {
            MCMHostArgument argument{};
            if (value.IsInt()) {
                argument.type = MCM_HOST_INTEGER;
                argument.integer = value.GetSInt();
            }
            else if (value.IsFloat()) {
                argument.type = MCM_HOST_FLOAT;
                argument.number = value.GetFloat();
            }
            else if (value.IsString()) {
                argument.type = MCM_HOST_STRING;
                call->strings.emplace_back(value.GetString());
                argument.text = call->strings.back().c_str();
            }
            else {
                return false;
            }
            call->arguments.push_back(argument);
        }

        Attempt(call);

        return true;
    }

    void HostBridge::RetryTask::operator()() const
    {
        Attempt(call);
    }

    void MCM_HOST_CALL HostBridge::Complete(void* a_user, MCMHostResult a_result, uint32_t a_declined)
    {
        std::unique_ptr<std::shared_ptr<PendingCall>> pending(static_cast<std::shared_ptr<PendingCall>*>(a_user));
        (*pending)->completion(a_result, a_declined != 0);
    }

    bool HostBridge::Retry(const std::shared_ptr<PendingCall>& a_call)
    {
        // Let Bridge finish closing or applying its pause before retrying.
        return Scheduler::GetSingleton()->ScheduleAfterFrames(RetryTask{ a_call }, 1);
    }

    void HostBridge::Attempt(const std::shared_ptr<PendingCall>& a_call)
    {
        if (a_call->context->released || a_call->context->cancelled) {
            a_call->completion(MCM_HOST_CANCELLED, false);
            return;
        }

        if (std::chrono::steady_clock::now() >= a_call->deadline) {
            a_call->completion(MCM_HOST_TIMED_OUT, false);
            return;
        }

        if (!a_call->context->token) {
            // Closing may be asynchronous. Wait for a fresh context before sending the next call.
            MCMHostContext token{};
            const auto result = a_call->context->api->begin_context("MCMMemory", 0, &token);
            if (result == MCM_HOST_BUSY && Retry(a_call)) {
                return;
            }
            if (result != MCM_HOST_OK || !token) {
                a_call->completion(result == MCM_HOST_OK ? MCM_HOST_UNAVAILABLE : result, false);
                return;
            }
            a_call->context->token = token;
        }

        const MCMHostCall request{ a_call->modID.c_str(), a_call->function.c_str(), a_call->arguments.data(), static_cast<uint32_t>(a_call->arguments.size()), a_call->timeout, a_call->acceptConfirmation ? 1U : 0U };

        // Keep the call alive until completion, even if its context is released.
        // Bridge may call back immediately; rejected calls need cleanup below.
        auto* receiver = new std::shared_ptr<PendingCall>(a_call);
        const auto result = a_call->context->api->invoke(a_call->context->token, &request, Complete, receiver);

        if (result == MCM_HOST_OK) {
            return;
        }

        delete receiver;

        // Busy means no script call started, so it is safe to retry.
        if (result == MCM_HOST_BUSY && Retry(a_call)) {
            return;
        }

        a_call->completion(result, false);
    }
}
