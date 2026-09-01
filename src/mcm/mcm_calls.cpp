#include "mcm/mcm_calls.hpp"
#include "mcm/mcm_messages.hpp"
#include "settings.hpp"
#include "utils/scheduler.hpp"

namespace MCMMemory
{
    void MCMCallResult::operator()(RE::BSScript::Variable)
    {
        state->finished = std::chrono::steady_clock::now();
        state->completed.store(true, std::memory_order_release);
        if (state->simulateUnresponsive) {
            if (!Scheduler::GetSingleton()->ScheduleAfterSeconds(std::move(task), state->simulatedCallbackDelaySeconds)) {
                logger::error("The simulated late MCM callback could not be scheduled");
            }
            return;
        }
        auto* tasks = SKSE::GetTaskInterface();
        if (tasks && task) {
            tasks->AddTask(std::move(task));
        }
    }

    bool MCMCallWatch::Acquire()
    {
        MCMCallWatch* expected{};
        if (!owner.compare_exchange_strong(expected, this)) {
            return false;
        }
        Consume();
        EndRecovery();
        timeoutSeconds = GetSettings().scriptCallTimeoutSeconds;
        return true;
    }

    void MCMCallWatch::Release(bool a_abandonPending)
    {
        if (a_abandonPending) {
            Abandon();
        }
        if (owner.load() == this) {
            owner.store(nullptr);
        }
        Consume();
        EndRecovery();
    }

    void MCMCallWatch::Consume()
    {
        if (pending) {
            MCMMessages::StopTracking(pending);
            pending.reset();
        }
    }

    bool MCMCallWatch::Call(const MCMScript& a_script, std::string_view a_modID, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, SKSE::TaskInterface::TaskFn a_task)
    {
        if (owner.load() != this || pending || IsUnavailable(a_modID)) {
            delete a_arguments;
            return false;
        }
        pending = std::make_shared<MCMCallState>();
        pending->modID = a_modID;
        pending->functionName = a_functionName;
        pending->started = std::chrono::steady_clock::now();
        // Check for a test mode that simulates an unresponsive MCM call.
        const auto& testMCM = GetSettings().testUnresponsiveMCM;
        if (!testMCM.empty() && testMCM.size() == a_modID.size() && ContainsCaseInsensitive(testMCM, a_modID) && !simulatedUnresponsive.exchange(true)) {
            pending->simulateUnresponsive = true;
            pending->simulatedCallbackDelaySeconds = timeoutSeconds + mcmRecoverySeconds + 1.0F;
            logger::warn("Testing an unresponsive MCM call on '{}' during this game session", a_modID);
        }
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new MCMCallResult(std::move(a_task), pending));
        pending->callback = result.get();
        MCMMessages::Track(pending);
        logger::debug("Watching MCM call '{}' on '{}' (timeout {}s)", a_functionName, a_modID, timeoutSeconds);
        if (!a_script.Call(a_functionName, a_arguments, std::move(result))) {
            Consume();
            return false;
        }
        return true;
    }

    void MCMCallWatch::Cancel()
    {
        // Cancellation and timeout recovery share one deadline, including CloseConfig.
        if (!recovering) {
            recovering = true;
            recoveryDeadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(mcmRecoverySeconds));
        }
    }

    MCMCallStatus MCMCallWatch::Check()
    {
        const auto now = std::chrono::steady_clock::now();
        if (!pending) {
            return recovering && now >= recoveryDeadline ? MCMCallStatus::Expired : MCMCallStatus::None;
        }
        const bool completed = pending->completed.load(std::memory_order_acquire) && !pending->simulateUnresponsive;
        const auto checkedTime = completed ? pending->finished : now;
        const float elapsed = std::chrono::duration<float>(checkedTime - pending->started).count();
        if (!timedOut && elapsed >= timeoutSeconds) {
            timedOut = true;
            logger::warn("MCM call '{}' on '{}' exceeded {}s; waiting up to {}s for safe recovery", pending->functionName, pending->modID, timeoutSeconds, mcmRecoverySeconds);
            if (!recovering) {
                recovering = true;
                recoveryDeadline = pending->started + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(timeoutSeconds + mcmRecoverySeconds));
            }
            if (!completed) {
                TracePending();
            }
        }
        if (recovering && !completed && now >= recoveryDeadline) {
            return MCMCallStatus::Expired;
        }
        return completed ? MCMCallStatus::Completed : MCMCallStatus::Waiting;
    }

    void MCMCallWatch::TracePending() const
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !pending || pending->completed.load(std::memory_order_acquire)) {
            return;
        }
        std::optional<RE::VMStackID> stackID;
        {
            RE::BSSpinLockGuard lock(vm->runningStacksLock);
            for (const auto& entry : vm->allRunningStacks) {
                if (entry.second && entry.second->callback.get() == pending->callback) {
                    stackID = entry.first;
                    break;
                }
            }
        }
        if (stackID) {
            logger::warn("Pending MCM Papyrus stack {}: '{}' on '{}'; requesting its trace in the Papyrus log", *stackID, pending->functionName, pending->modID);
            vm->TraceStack("MCM Memory: script call exceeded its waiting limit", *stackID);
        }
        else {
            logger::warn("No running Papyrus stack was found for the pending '{}' call on '{}'", pending->functionName, pending->modID);
        }
    }

    void MCMCallWatch::Abandon()
    {
        if (!pending) {
            EndRecovery();
            return;
        }

        std::string modID = pending->modID;
        {
            std::lock_guard lock(unavailableMutex);
            if (!ContainsMCMID(unavailableMCMs, modID)) {
                unavailableMCMs.push_back(modID);
                unavailableGeneration.fetch_add(1);
            }
        }
        logger::error("MCM '{}' did not finish safely and will be skipped by scripted operations for this game session", modID);
        Consume();
        EndRecovery();
    }

    bool MCMCallWatch::IsUnavailable(std::string_view a_modID)
    {
        std::lock_guard lock(unavailableMutex);
        return ContainsMCMID(unavailableMCMs, a_modID);
    }

    void MCMCallWatch::ResetSession()
    {
        std::lock_guard lock(unavailableMutex);
        if (!unavailableMCMs.empty()) {
            unavailableMCMs.clear();
            unavailableGeneration.fetch_add(1);
        }
        simulatedUnresponsive.store(false);
    }
}
