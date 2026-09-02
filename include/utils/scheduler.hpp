#pragma once

#include "plugin.hpp"

namespace MCMMemory
{

    inline constexpr float secondsPerFrame = 1.0F / 60.0F;

    struct DelayedTask
    {
        DelayedTask(SKSE::TaskInterface::TaskFn a_task, float a_delaySeconds, bool a_uiTask) : task(std::move(a_task)), delaySeconds(a_delaySeconds), uiTask(a_uiTask) {}

        // Work sent to one of SKSE task queues.
        SKSE::TaskInterface::TaskFn task;

        // Number of seconds to wait before queuing the work.
        float delaySeconds{};

        // Sends Scaleform work to SKSE dedicated UI queue.
        bool uiTask{};

        // Waits, then sends the stored work to its SKSE task queue.
        void operator()();
    };

    class Scheduler
    {
    public:
    
        static Scheduler* GetSingleton()
        {
            static Scheduler singleton;
            return std::addressof(singleton);
        }

        // Schedules the task after k frames.
        inline bool ScheduleAfterFrames(SKSE::TaskInterface::TaskFn a_task, const uint32_t a_delayFrames) const
        {
            return ScheduleAfterSeconds(std::move(a_task), static_cast<float>(a_delayFrames) * secondsPerFrame);
        }

        // Schedules Scaleform work on SKSE UI queue after k frames.
        inline bool ScheduleUIAfterFrames(SKSE::TaskInterface::TaskFn a_task, const uint32_t a_delayFrames) const
        {
            return Schedule(std::move(a_task), static_cast<float>(a_delayFrames) * secondsPerFrame, true);
        }

        // Runs a task now or after the requested delay.
        inline bool ScheduleAfterSeconds(SKSE::TaskInterface::TaskFn a_task, float a_delaySeconds) const
        {
            return Schedule(std::move(a_task), a_delaySeconds, false);
        }

    private:

        bool Schedule(SKSE::TaskInterface::TaskFn a_task, float a_delaySeconds, bool a_uiTask) const;
    };
}
