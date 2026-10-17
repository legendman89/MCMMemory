#pragma once

// HUD drawing is based on Log Watcher.

#include "menu/hud_defs.hpp"
#include "menu/backend.hpp"
#include "profile/stats.hpp"
#include "utils/helper.hpp"
#include "utils/time.hpp"

#include <deque>

#define DECLARE_HUD_COLOR(name, red, green, blue, alpha) name,

namespace MCMMemory
{
    inline constexpr float HUDHorizontalPadding{ 12.0F };
    inline constexpr float HUDVerticalPadding{ 7.0F };
    inline constexpr float HUDMargin{ 30.0F };
    inline constexpr float HUDWarningAccentWidth{ 6.0F };
    inline constexpr float HUDWarningHorizontalPadding{ 20.0F };
    inline constexpr float HUDWarningVerticalPadding{ 12.0F };
    inline constexpr float HUDWarningLineGap{ 7.0F };
    inline constexpr float HUDWarningScale{ 1.35F };

    inline constexpr GUI::Color4 kHUDBackground{ 0.96F, 0.96F, 0.96F, 0.75F };
    inline constexpr GUI::Color4 kHUDShadow{ 0.0F, 0.0F, 0.0F, 0.1F };

    struct Settings;

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

#define DECLARE_HUD_COLOR_VALUE(name, red, green, blue, alpha) GUI::Color4{ red, green, blue, alpha },
    inline constexpr std::array<GUI::Color4, ToIndex(HUDColor::Count)> HUDColors{ FOREACH_HUD_COLOR(DECLARE_HUD_COLOR_VALUE) };
#undef DECLARE_HUD_COLOR_VALUE

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

        TimePoint createdAt{ std::chrono::steady_clock::now() };

        TimePoint showAt{};

        HUDMessageType type{ HUDMessageType::MCMResult };

        OperationMode operationMode{ OperationMode::Automatic };
    };

    struct HUDDisplay
    {
        HUDMessage message;

        TimePoint startedAt{};

        TimePoint pausedAt{};

        TimePoint nextAt{};

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
        std::string detailKey;

        TimePoint startedAt{};

        TimePoint pausedAt{};

        bool active{};

        void Reset()
        {
            detailKey.clear();
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

    struct HUDMessageTiming
    {
        float delaySeconds{};

        float lifetimeSeconds{};

        float fadeAtSeconds{};

        float fadeSeconds{};
    };

    using HUDMessageRenderer = void (*)(const HUDMessage&, const HUDOptions&, float);
    using HUDWarningRenderer = void (*)(const char*, const HUDOptions&, float);

    // Singleton class that manages the heads-up display for MCM backup and restore operations.
    // I hand-crafted this to have a customizable appearance over Debug Notifications.
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
            ShowOperationStarted("HUD.Operation.BackupStarted");
        }

        inline void ShowRestoreStarted()
        {
            ShowOperationStarted("HUD.Operation.RestoreStarted");
        }

        inline bool HasNotifications()
        {
            std::lock_guard lock(hudMutex);
            return enabled.load(std::memory_order_relaxed) && (display.active || !notificationQueue.empty());
        }

        inline bool HasWarning()
        {
            std::lock_guard lock(hudMutex);
            return warning.active;
        }

        void Configure(const Settings& a_settings);

        void ShowBackupMCM(std::string_view a_modName, const BackupStats& a_stats, OperationMode a_operationMode);

        void ShowRestoreMCM(std::string_view a_modName, const RestoreStats& a_stats, OperationMode a_operationMode);

        void ShowBackupSummary(const BackupStats& a_stats);

        void ShowRestoreSummary(const RestoreStats& a_stats);

        void ShowBackupCancelled(const BackupStats& a_stats);

        void ShowRestoreCancelled(const RestoreStats& a_stats);

        void ShowMenuWarning(std::string_view a_detailKey);

        void ShowFailure(std::string_view a_titleKey, std::string_view a_detailKey);

        void HideMenuWarning();

        void KeepPreviewAlive();

        void Preview();

        void Render();

        void RenderNotifications(bool a_blocked, HUDMessageRenderer a_renderer = nullptr);

        void RenderWarning(bool a_blocked, HUDWarningRenderer a_renderer = nullptr);

        void Reset();

    private:

        inline void StartMessage(HUDMessage a_message, const TimePoint& a_now)
        {
            AppendBackupAge(a_message, a_now);
            display.message = std::move(a_message);
            display.startedAt = a_now;
            display.pausedAt = {};
            display.active = true;
        }

        inline HUDMessageTiming GetTiming(HUDMessageType a_type) const
        {
            const float lifetime = options.durationSeconds + options.fadeSeconds;
            const float operationFade = std::min(options.fadeSeconds, options.operationDelaySeconds);
            const std::array<HUDMessageTiming, ToIndex(HUDMessageType::Count)> timings
            {
                HUDMessageTiming{ options.startDelaySeconds, options.operationDelaySeconds, options.operationDelaySeconds - operationFade, operationFade },
                HUDMessageTiming{ 0.0F, lifetime, options.durationSeconds, options.fadeSeconds },
                HUDMessageTiming{ options.summaryDelaySeconds, lifetime, options.durationSeconds, options.fadeSeconds },
                HUDMessageTiming{ options.summaryDelaySeconds, lifetime, options.durationSeconds, options.fadeSeconds },
                HUDMessageTiming{ 0.0F, lifetime, options.durationSeconds, options.fadeSeconds },
                HUDMessageTiming{ 0.0F, lifetime, options.durationSeconds, options.fadeSeconds },
                HUDMessageTiming{ 0.0F, lifetime, options.durationSeconds, options.fadeSeconds }
            };
            return timings[ToIndex(a_type)];
        }

        inline bool ShouldShowPerMod(OperationMode a_operationMode) const
        {
            const size_t index = ToIndex(a_operationMode);
            return index < perModNotifications.size() && perModNotifications[index].load(std::memory_order_relaxed);
        }

        void StartPreview(const TimePoint& a_now);

        uint32_t GetColor(HUDColor a_color, float a_alpha) const;

        void ShowOperationStarted(std::string_view a_key);

        void BeginOperation(HUDMessage a_message);

        void QueueMessage(HUDMessage a_message);

        void QueueFailure(HUDMessage a_message);

        bool UpdateMenuDelay(bool a_blocked, const TimePoint& a_now);

        bool UpdateActiveMessage(const TimePoint& a_now, HUDMessageRenderer a_renderer = nullptr);

        bool StartNextMessage(const TimePoint& a_now, HUDMessageRenderer a_renderer = nullptr);

        void AppendBackupAge(HUDMessage& a_message, const TimePoint& a_now) const;

        void DrawMessage(const HUDMessage& a_message, float a_alpha, HUDMessageRenderer a_renderer = nullptr) const;

        void UpdateMenuWarning(bool a_blocked, const TimePoint& a_now, HUDWarningRenderer a_renderer = nullptr);

        void DrawMenuWarning(float a_alpha, HUDWarningRenderer a_renderer = nullptr) const;

        std::mutex hudMutex;

        // Keeps pending notifications in the order they should appear.
        std::deque<HUDMessage> notificationQueue;

        HUDDisplay display;

        HUDWarning warning;

        TimePoint menuResumeAt{};

        HUDOptions options;

        std::atomic<bool> enabled{ true };

        std::array<std::atomic<bool>, ToIndex(OperationMode::Count)> perModNotifications{};

        bool gameMenuBlocked{};
    };

    inline void __stdcall RenderHUD()
    {
        HUD::GetSingleton()->Render();
    }
}

#undef DECLARE_HUD_COLOR
