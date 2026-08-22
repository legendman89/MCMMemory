#pragma once

#include "plugin.hpp"

namespace MCMMemory
{
    struct DelayedTask
    {
        // SKSE game task queue.
        SKSE::TaskInterface::TaskFn task;

        // Number of seconds to wait before queuing the work.
        float delaySeconds{};

        // Waits, then sends the stored work to game task queue.
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

        // Converts a frame count into seconds and schedules the task.
        inline bool ScheduleAfterFrames(SKSE::TaskInterface::TaskFn a_task, const uint32_t a_delayFrames) const
        {
            constexpr float secondsPerFrame = 1.0F / 60.0F;
            return ScheduleAfterSeconds(std::move(a_task), static_cast<float>(a_delayFrames) * secondsPerFrame);
        }

        // Runs a task now or after the requested delay.
        bool ScheduleAfterSeconds(SKSE::TaskInterface::TaskFn a_task, float a_delaySeconds) const;
    };
}
