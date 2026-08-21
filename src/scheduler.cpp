#include "scheduler.hpp"

namespace MCMMemory
{
    void DelayedTask::operator()()
    {
        std::this_thread::sleep_for(std::chrono::duration<float>(delaySeconds));
        auto tasks = SKSE::GetTaskInterface();
        if (!tasks || !task) {
            logger::error("A delayed game task could not reach the SKSE task queue");
            return;
        }
        tasks->AddTask(std::move(task));
    }

    bool Scheduler::ScheduleAfterSeconds(SKSE::TaskInterface::TaskFn a_task, float a_delaySeconds) const
    {
        if (!a_task) {
            return false;
        }

        if (a_delaySeconds <= 0.0F) {
            auto tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                return false;
            }
            tasks->AddTask(std::move(a_task));
            return true;
        }

        // Run the timer on another thread so waiting does not freeze Skyrim.
        try {
            std::thread(DelayedTask{ std::move(a_task), a_delaySeconds }).detach();
        } 
        catch (const std::exception& error) {
            logger::error("A delayed game task could not start its timer: {}", error.what());
            return false;
        }

        return true;
    }
}
