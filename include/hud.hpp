#pragma once

// HUD system is improted from Log Watcher code.

#include "hud_defs.hpp"
#include "stats.hpp"
#include "ui.hpp"

#include <deque>

#define DECLARE_HUD_COLOR(name, red, green, blue, alpha) name,
#define DECLARE_HUD_COLOR_VALUE(name, red, green, blue, alpha) GUI::ImVec4{ red, green, blue, alpha },

namespace MCMMemory
{
    enum class HUDColor
    {
        FOREACH_HUD_COLOR(DECLARE_HUD_COLOR)
        Count
    };

    inline constexpr std::array<GUI::ImVec4, static_cast<size_t>(HUDColor::Count)> HUDColors
    {
        FOREACH_HUD_COLOR(DECLARE_HUD_COLOR_VALUE)
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

        bool showBackupAge{};

        bool allowWhileBlocked{};
    };

    struct HUDDisplay
    {
        HUDMessage message;

        std::chrono::steady_clock::time_point startedAt{};

        std::chrono::steady_clock::time_point pausedAt{};

        std::chrono::steady_clock::time_point nextAt{};

        bool active{};

        bool paused{};

        void Reset()
        {
            message = {};
            startedAt = {};
            pausedAt = {};
            nextAt = {};
            active = false;
            paused = false;
        }
    };

    class HUD
    {
    public:

        static HUD* GetSingleton()
        {
            static HUD singleton;
            return std::addressof(singleton);
        }

        void Configure(bool a_enabled, bool a_individualMCMs);

        void Reset();

        void ShowBackupStarted();

        void ShowRestoreStarted();

        void ShowBackupMCM(std::string_view a_modName, const BackupStats& a_stats);

        void ShowRestoreMCM(std::string_view a_modName, const RestoreStats& a_stats);

        void ShowBackupSummary(const BackupStats& a_stats);

        void ShowRestoreSummary(const RestoreStats& a_stats);

        void ShowFailure(std::string_view a_title, std::string_view a_detail);

        void Preview();

        void Render();

    private:

        void ShowNow(HUDMessage a_message);

        void QueueMessage(HUDMessage a_message);

        void QueueSummary(HUDMessage a_message);

        void StartMessage(HUDMessage a_message, const std::chrono::steady_clock::time_point& a_now);

        void AppendBackupAge(HUDMessage& a_message, const std::chrono::steady_clock::time_point& a_now) const;

        void DrawMessage(const HUDMessage& a_message, float a_alpha) const;

        std::string GetDisplayModName(std::string_view a_modName) const;

        GUI::ImVec4 GetColor(HUDColor a_color, float a_alpha) const;

        std::mutex hudMutex;

        // Keeps every MCM result in the order it completed.
        std::deque<HUDMessage> notificationQueue;

        HUDDisplay display;

        std::atomic<bool> enabled{ true };

        std::atomic<bool> individualMCMs{};

        bool rendererSeen{};
    };

    void __stdcall RenderHUD();
}

#undef DECLARE_HUD_COLOR
#undef DECLARE_HUD_COLOR_VALUE
