#pragma once

#include "mcm/mcm_script.hpp"

namespace MCMMemory
{
    inline constexpr float mcmWatchIntervalSeconds{ 0.5F };
    inline constexpr float mcmRecoverySeconds{ 10.0F };

    enum class MCMCallStatus
    {
        None,
        Waiting,
        Completed,
        Expired
    };

    template <class Operation>
    struct MCMWatchTask
    {
        Operation* operation{};

        uint64_t loadedGameSession{};

        void operator()() const { operation->CheckCalls(loadedGameSession); }
    };

    // The VM callback only publishes completion. Operation state stays on game tasks.
    struct MCMCallState
    {
        std::string modID;

        std::string functionName;

        std::chrono::steady_clock::time_point started;

        std::chrono::steady_clock::time_point finished;

        RE::BSScript::IStackCallbackFunctor* callback{};

        // Simulated delay for testing recovery.
        float simulatedCallbackDelaySeconds{};

        std::atomic<bool> completed{};

        std::atomic<bool> confirmationDeclined{};

        // Test mode keeps this call waiting even after its real callback arrives.
        bool simulateUnresponsive{};
    };

    struct MCMCallResult : public RE::BSScript::IStackCallbackFunctor
    {
        SKSE::TaskInterface::TaskFn task;

        std::shared_ptr<MCMCallState> state;

        MCMCallResult(SKSE::TaskInterface::TaskFn a_task, std::shared_ptr<MCMCallState> a_state) : task(std::move(a_task)), state(std::move(a_state)) {}

        void operator()(RE::BSScript::Variable) override;

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    // Backup and restore share ownership so their script calls can't overlap.
    class MCMCallWatch
    {
    public:

        // Returns true if this thread acquired ownership of the MCM call watch.
        // Only one operation can watch MCM calls at a time.
        bool Acquire();

        // Releases ownership of the MCM call watch.
        // If a_abandonPending is true, the pending call is abandoned and will not be recovered.
        void Release(bool a_abandonPending = false);

        // Calls an MCM function asynchronously through the Papyrus VM.
        bool Call(const MCMScript& a_script, std::string_view a_modID, std::string_view a_functionName, RE::BSScript::IFunctionArguments* a_arguments, SKSE::TaskInterface::TaskFn a_task);

        // Checks the status of the pending MCM call.
        MCMCallStatus Check();

        // Cancels the pending MCM call and starts recovery.
        void Cancel();

        // If the pending call is still active, logs its stack trace for debugging.
        void TracePending() const;

        // Abandons the pending call without recovery.
        void Abandon();

        // Stops tracking the pending call and releases its resources.
        void Consume();

        static bool IsUnavailable(std::string_view a_modID);

        static void ResetSession();

        static inline uint64_t UnavailableGeneration() { return unavailableGeneration.load(); }

        static inline bool IsBusy() { return owner.load() != nullptr; }
        
        inline bool HasCall() const { return pending != nullptr; }

        inline bool TimedOut() const { return timedOut; }

        inline bool IsClosing() const { return pending && pending->functionName == "CloseConfig"; }

        inline bool ConfirmationDeclined() const { return pending && pending->confirmationDeclined.load(std::memory_order_acquire); }

        inline void EndRecovery()
        {
            recovering = false;
            timedOut = false;
        }

    private:

        std::shared_ptr<MCMCallState> pending;

        std::chrono::steady_clock::time_point recoveryDeadline{};

        float timeoutSeconds{ 30.0F };

        bool recovering{};

        bool timedOut{};

        // Static as they are shared across MCM operations, and only one can be active at a time.
        static inline std::atomic<MCMCallWatch*> owner{};

        // Tracks MCMs unavailable to all scripted operations during this game session
        static inline std::mutex unavailableMutex;

        static inline MCMFilter unavailableMCMs;

        static inline std::atomic<uint64_t> unavailableGeneration{};

        // Test mode.
        static inline std::atomic<bool> simulatedUnresponsive{};
    };
}
