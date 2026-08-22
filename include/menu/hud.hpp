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

    struct HUDOptions
    {
#define DECLARE_HUD_OPTION(type, settingName, defaultValue, optionName, minimum, maximum, label, format) type optionName{ defaultValue };
        FOREACH_HUD_SETTING(DECLARE_HUD_OPTION)
#undef DECLARE_HUD_OPTION
    };

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

        void ShowBackupMCM(std::string_view a_modName, const BackupStats& a_stats);

        void ShowRestoreMCM(std::string_view a_modName, const RestoreStats& a_stats);

        void ShowBackupSummary(const BackupStats& a_stats);

        void ShowRestoreSummary(const RestoreStats& a_stats);

        void ShowFailure(std::string_view a_title, std::string_view a_detail);

        void Preview();

        void Render();

    private:

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
                0.0F
            };
            return delays[static_cast<size_t>(a_type)];
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

        std::string GetDisplayModName(std::string_view a_modName) const;

        std::mutex hudMutex;

        // Keeps pending notifications in the order they should appear.
        std::deque<HUDMessage> notificationQueue;

        HUDDisplay display;

        std::chrono::steady_clock::time_point menuResumeAt{};

        HUDOptions options;

        std::atomic<bool> enabled{ true };

        std::atomic<bool> individualMCMs{};

        bool gameMenuBlocked{};
    };

    inline void __stdcall RenderHUD()
    {
        HUD::GetSingleton()->Render();
    }
}

#undef DECLARE_HUD_COLOR
#undef DECLARE_HUD_COLOR_VALUE
