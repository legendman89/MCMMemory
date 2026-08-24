#pragma once

// HUD drawing is based on Log Watcher.

#include "menu/hud_defs.hpp"
#include "menu/ui.hpp"
#include "profile/stats.hpp"

#include <deque>

#define DECLARE_HUD_COLOR(name, red, green, blue, alpha) name,
#define DECLARE_HUD_COLOR_VALUE(name, red, green, blue, alpha) GUI::ImVec4{ red, green, blue, alpha },

namespace MCMMemory
{
    struct Settings;

    inline bool IsTimeSet(const std::chrono::steady_clock::time_point& a_time)
    {
        return a_time.time_since_epoch().count() != 0;
    }

    inline float SecondsSince(const std::chrono::steady_clock::time_point& a_startedAt, const std::chrono::steady_clock::time_point& a_now)
    {
        return std::chrono::duration<float>(a_now - a_startedAt).count();
    }

    inline std::chrono::steady_clock::time_point TimeAfter(const std::chrono::steady_clock::time_point& a_time, float a_seconds)
    {
        const auto delay = std::chrono::duration<float>(a_seconds);
        return a_time + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
    }

    inline float FadeAlpha(float a_ageSeconds, float a_fadeAtSeconds, float a_fadeSeconds)
    {
        if (a_fadeSeconds <= 0.0F || a_ageSeconds <= a_fadeAtSeconds) {
            return 1.0F;
        }

        return std::clamp(1.0F - (a_ageSeconds - a_fadeAtSeconds) / a_fadeSeconds, 0.0F, 1.0F);
    }

    enum class HUDColor
    {
        FOREACH_HUD_COLOR(DECLARE_HUD_COLOR)
        Count
    };

    inline constexpr std::array<GUI::ImVec4, static_cast<size_t>(HUDColor::Count)> HUDColors
    {
        FOREACH_HUD_COLOR(DECLARE_HUD_COLOR_VALUE)
    };

    enum class HUDMessageType
    {
        OperationStarted,
        MCMResult,
        BackupSummary,
        RestoreSummary,
        Cancellation,
        Failure,
        Preview,
        Count
    };

    struct HUDSegment
    {
        std::string text;

        HUDColor color{ HUDColor::Primary };
    };

    struct HUDMessage
    {
        std::vector<HUDSegment> segments;

        std::chrono::steady_clock::time_point createdAt{ std::chrono::steady_clock::now() };

        std::chrono::steady_clock::time_point showAt{};

        HUDMessageType type{ HUDMessageType::MCMResult };

        OperationMode operationMode{ OperationMode::Automatic };
    };

    struct HUDDisplay
    {
        HUDMessage message;

        std::chrono::steady_clock::time_point startedAt{};

        std::chrono::steady_clock::time_point pausedAt{};

        std::chrono::steady_clock::time_point nextAt{};

        bool active{};

        void Reset()
        {
            message = {};
            startedAt = {};
            pausedAt = {};
            nextAt = {};
            active = false;
        }
    };

    struct HUDWarning
    {
        std::chrono::steady_clock::time_point startedAt{};

        std::chrono::steady_clock::time_point pausedAt{};

        bool active{};

        void Reset()
        {
            startedAt = {};
            pausedAt = {};
            active = false;
        }
    };

    struct HUDOptions
    {
#define DECLARE_HUD_OPTION(type, settingName, defaultValue, optionName, minimum, maximum, label, format) type optionName{ defaultValue };
        FOREACH_HUD_SETTING(DECLARE_HUD_OPTION)
#undef DECLARE_HUD_OPTION
    };

    // Singleton class that manages the heads-up display for MCM backup and restore operations.
    // I hand-crafted this to hvae a customizable appearance over Debug Notifications.
    class HUD
    {
    public:

        static HUD* GetSingleton()
        {
            static HUD singleton;
            return std::addressof(singleton);
        }

        inline void ShowBackupStarted()
        {
            ShowOperationStarted("Backing up MCM settings");
        }

        inline void ShowRestoreStarted()
        {
            ShowOperationStarted("Restoring MCM settings");
        }

        void Configure(const Settings& a_settings);

        void Reset();

        void ShowBackupMCM(std::string_view a_modName, const BackupStats& a_stats, OperationMode a_operationMode);

        void ShowRestoreMCM(std::string_view a_modName, const RestoreStats& a_stats, OperationMode a_operationMode);

        void ShowBackupSummary(const BackupStats& a_stats);

        void ShowRestoreSummary(const RestoreStats& a_stats);

        void ShowBackupCancelled(const BackupStats& a_stats);

        void ShowRestoreCancelled(const RestoreStats& a_stats);

        void ShowFailure(std::string_view a_title, std::string_view a_detail);

        void ShowRestoreMenuWarning();

        void HideRestoreMenuWarning();

        void Preview();

        void KeepPreviewAlive();

        void Render();

    private:

        void StartPreview(const std::chrono::steady_clock::time_point& a_now);

        void StartMessage(HUDMessage a_message, const std::chrono::steady_clock::time_point& a_now)
        {
            AppendBackupAge(a_message, a_now);
            display.message = std::move(a_message);
            display.startedAt = a_now;
            display.pausedAt = {};
            display.active = true;
        }

        inline GUI::ImVec4 GetColor(HUDColor a_color, float a_alpha) const
        {
            auto color = HUDColors[static_cast<size_t>(a_color)];
            color.w *= a_alpha;
            return color;
        }

        inline float GetDelaySeconds(HUDMessageType a_type) const
        {
            const std::array<float, static_cast<size_t>(HUDMessageType::Count)> delays
            {
                options.startDelaySeconds,
                0.0F,
                options.summaryDelaySeconds,
                options.summaryDelaySeconds,
                0.0F,
                0.0F,
                0.0F
            };
            return delays[static_cast<size_t>(a_type)];
        }

        inline bool ShouldShowPerMod(OperationMode a_operationMode) const
        {
            const size_t index = static_cast<size_t>(a_operationMode);
            return index < perModNotifications.size() && perModNotifications[index].load(std::memory_order_relaxed);
        }

        void ShowOperationStarted(std::string_view a_text);

        void BeginOperation(HUDMessage a_message);

        void QueueMessage(HUDMessage a_message);

        void QueueFailure(HUDMessage a_message);

        bool UpdateMenuDelay(bool a_blocked, const std::chrono::steady_clock::time_point& a_now);

        bool UpdateActiveMessage(const std::chrono::steady_clock::time_point& a_now);

        bool StartNextMessage(const std::chrono::steady_clock::time_point& a_now);

        void AppendBackupAge(HUDMessage& a_message, const std::chrono::steady_clock::time_point& a_now) const;

        void DrawMessage(const HUDMessage& a_message, float a_alpha) const;

        void UpdateRestoreMenuWarning(bool a_blocked, const std::chrono::steady_clock::time_point& a_now);

        void DrawRestoreMenuWarning(float a_alpha) const;

        std::mutex hudMutex;

        // Keeps pending notifications in the order they should appear.
        std::deque<HUDMessage> notificationQueue;

        HUDDisplay display;

        HUDWarning warning;

        std::chrono::steady_clock::time_point menuResumeAt{};

        HUDOptions options;

        std::atomic<bool> enabled{ true };

        std::array<std::atomic<bool>, static_cast<size_t>(OperationMode::Count)> perModNotifications{};

        bool gameMenuBlocked{};
    };

    inline void __stdcall RenderHUD()
    {
        HUD::GetSingleton()->Render();
    }
}

#undef DECLARE_HUD_COLOR
#undef DECLARE_HUD_COLOR_VALUE
