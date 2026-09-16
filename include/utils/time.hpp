#pragma once

#include <chrono>
#include <cstdint>

namespace MCMMemory
{
    // Converts frame counts to seconds assuming a 60 FPS menu rate.
    inline constexpr float secondsPerFrame = 1.0F / 60.0F;

    // Some values may become readable a few frames after the callback.
    inline constexpr int QueueDelayFrames = 2;

    // Check busy toggles and opening pages every 0.1 seconds, for about five seconds.
    inline constexpr uint32_t captureReadDelayFrames = 6;
    inline constexpr uint32_t maximumCaptureReads = 50;

    // Recorded commands can wait for the player to answer a confirmation dialog.
    inline constexpr uint32_t maximumCommandCaptureReads = 600;

    // Retry profile saves every 5 seconds, up to three times.
    // TODO: I can inject several failing saves to test this more aggressively.
    inline constexpr float profileSaveRetryDelaySeconds = 5.0F;
    inline constexpr uint32_t maximumProfileSaveRetries = 3;
    inline constexpr auto profileSaveIdleDelay = std::chrono::seconds(5);

    // Normal control calls use at most five seconds before recovery starts.
    inline constexpr float mcmControlTimeoutSeconds{ 5.0F };

    // A watchdog for MCMs that fail to respond.
    inline constexpr float mcmWatchIntervalSeconds{ 0.5F };
    inline constexpr float mcmRecoverySeconds{ 10.0F };

    // Wait for MCM registration to finish and the registry to stop changing.
    inline constexpr uint32_t maximumRegistryChecks{ 30 };
    inline constexpr uint32_t requiredStableRegistryChecks{ 2 };
    inline constexpr float registryCheckDelaySeconds{ 5.0F };
    inline constexpr auto registryRefreshInterval{ std::chrono::seconds(5) };

    // Buffer checks are separate from the timeout of a Papyrus call.
    inline constexpr uint32_t maximumScriptWaitChecks{ 5 };

    // Gives a newly enabled MCM time to finish starting before it is opened again.
    inline constexpr float mcmActivationDelaySeconds{ 2.0F };

    // A cycle normally ends when the value stops changing or returns to where it
    // started; this just fixpoints a control whose text never repeats.
    // Largest cycle I found so far when making patches is 6.
    inline constexpr int maximumRecordedClicks{ 16 };

    // A page can still be rebuilding when the next setting is checked, so give it a few tries
    // before deciding the control is gone.
    inline constexpr int maximumSettleChecks{ 5 };

    inline bool IsTimeSet(const std::chrono::steady_clock::time_point& a_time)
    {
        return a_time.time_since_epoch().count() != 0;
    }

    inline float SecondsSince(const std::chrono::steady_clock::time_point& a_startedAt, const std::chrono::steady_clock::time_point& a_now)
    {
        return std::chrono::duration<float>(a_now - a_startedAt).count();
    }

    inline float SecondsUntil(const std::chrono::steady_clock::time_point& a_target, const std::chrono::steady_clock::time_point& a_now)
    {
        return SecondsSince(a_now, a_target);
    }

    inline std::chrono::steady_clock::time_point TimeAfter(const std::chrono::steady_clock::time_point& a_time, std::chrono::steady_clock::duration a_delay)
    {
        return a_time + a_delay;
    }

    inline std::chrono::steady_clock::time_point TimeAfter(const std::chrono::steady_clock::time_point& a_time, float a_seconds)
    {
        return TimeAfter(a_time, std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(a_seconds)));
    }
}
