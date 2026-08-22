#include "hud.hpp"
#include "settings.hpp"
#include "translate.hpp"

namespace MCMMemory
{

    // HUD drawing based on Log Watcher code.

    inline constexpr float HUDGapSeconds{ 0.5F };
    inline constexpr float HUDNotificationDelaySeconds{ 2.0F };
    inline constexpr float HUDMargin{ 30.0F };
    inline constexpr float HUDHorizontalPadding{ 12.0F };
    inline constexpr float HUDVerticalPadding{ 7.0F };

    void HUD::Configure(bool a_enabled, bool a_individualMCMs)
    {
        enabled.store(a_enabled, std::memory_order_relaxed);
        individualMCMs.store(a_individualMCMs, std::memory_order_relaxed);
        if (!a_enabled) {
            Reset();
        }
    }

    void HUD::Reset()
    {
        std::lock_guard lock(hudMutex);
        notificationQueue.clear();
        display.Reset();
    }

    void HUD::ShowBackupStarted()
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.segments.push_back({ Trans::Tr("Backing up MCM settings"), HUDColor::Accent });
        ShowNow(std::move(message));
    }

    void HUD::ShowRestoreStarted()
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.segments.push_back({ Trans::Tr("Restoring MCM settings"), HUDColor::Accent });
        ShowNow(std::move(message));
    }

    void HUD::ShowBackupMCM(std::string_view a_modName, const BackupStats& a_stats)
    {
        if (!enabled.load(std::memory_order_relaxed) || !individualMCMs.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        const auto modName = GetDisplayModName(a_modName);
        if (a_stats.failedMCMCount > 0) {
            message.segments.push_back({ Trans::Format("{} backup failed", modName), HUDColor::Error });
            message.segments.push_back({ Trans::Tr("    Previous settings kept"), HUDColor::Muted });
        }
        else {
            message.segments.push_back({ Trans::Format("{} backed up", modName), HUDColor::Success });
            message.segments.push_back({ Trans::Format("    {} settings", a_stats.settingCount), HUDColor::Accent });
            if (a_stats.skippedSettingCount > 0) {
                message.segments.push_back({ Trans::Format("    {} skipped", a_stats.skippedSettingCount), HUDColor::Warning });
            }
        }
        QueueMessage(std::move(message));
    }

    void HUD::ShowRestoreMCM(std::string_view a_modName, const RestoreStats& a_stats)
    {
        if (!enabled.load(std::memory_order_relaxed) || !individualMCMs.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        const auto modName = GetDisplayModName(a_modName);
        if (a_stats.appliedSettingCount == 0 && a_stats.unchangedSettingCount == 0 && a_stats.skippedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("{} restore skipped", modName), HUDColor::Warning });
        }
        else {
            message.segments.push_back({ Trans::Format("{} restored", modName), HUDColor::Success });
        }
        if (a_stats.appliedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("    {} changed", a_stats.appliedSettingCount), HUDColor::Accent });
        }
        if (a_stats.unchangedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("    {} already set", a_stats.unchangedSettingCount), HUDColor::Muted });
        }
        if (a_stats.skippedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("    {} skipped", a_stats.skippedSettingCount), HUDColor::Warning });
        }
        QueueMessage(std::move(message));
    }

    void HUD::ShowBackupSummary(const BackupStats& a_stats)
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.showBackupAge = true;
        message.segments.push_back({ Trans::Format("{} MCMs backed up", a_stats.MCMCount), HUDColor::Success });
        message.segments.push_back({ Trans::Format("    {} settings", a_stats.settingCount), HUDColor::Accent });
        if (a_stats.skippedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("    {} skipped", a_stats.skippedSettingCount), HUDColor::Warning });
        }
        if (a_stats.failedMCMCount > 0) {
            message.segments.push_back({ Trans::Format("    {} MCMs failed", a_stats.failedMCMCount), HUDColor::Error });
        }
        QueueSummary(std::move(message));
    }

    void HUD::ShowRestoreSummary(const RestoreStats& a_stats)
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.segments.push_back({ Trans::Format("{} MCMs restored", a_stats.MCMCount), HUDColor::Success });
        message.segments.push_back({ Trans::Format("    {} changed", a_stats.appliedSettingCount), HUDColor::Accent });
        message.segments.push_back({ Trans::Format("    {} already set", a_stats.unchangedSettingCount), HUDColor::Muted });
        if (a_stats.skippedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("    {} skipped", a_stats.skippedSettingCount), HUDColor::Warning });
        }
        QueueSummary(std::move(message));
    }

    void HUD::ShowFailure(std::string_view a_title, std::string_view a_detail)
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.segments.push_back({ Trans::Tr(a_title), HUDColor::Error });
        if (!a_detail.empty()) {
            message.segments.push_back({ std::format("    {}", Trans::Tr(a_detail)), HUDColor::Muted });
        }
        QueueSummary(std::move(message));
    }

    void HUD::Preview()
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.showBackupAge = true;
        message.allowWhileBlocked = true;
        message.segments.push_back({ Trans::Tr("42 MCMs backed up"), HUDColor::Success });
        message.segments.push_back({ Trans::Tr("    318 settings"), HUDColor::Accent });

        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(hudMutex);
        StartMessage(std::move(message), now);
        logger::info("HUD notification preview started");
    }

    void HUD::ShowNow(HUDMessage a_message)
    {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(hudMutex);
        notificationQueue.clear();
        StartMessage(std::move(a_message), now);
    }

    void HUD::QueueMessage(HUDMessage a_message)
    {
        std::lock_guard lock(hudMutex);
        notificationQueue.push_back(std::move(a_message));
    }

    void HUD::QueueSummary(HUDMessage a_message)
    {
        const auto now = std::chrono::steady_clock::now();
        a_message.showAt = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(HUDNotificationDelaySeconds));

        QueueMessage(std::move(a_message));
        logger::info("HUD notification queued with a {:.1f} second delay", HUDNotificationDelaySeconds);
    }

    void HUD::StartMessage(HUDMessage a_message, const std::chrono::steady_clock::time_point& a_now)
    {
        AppendBackupAge(a_message, a_now);
        display.message = std::move(a_message);
        display.startedAt = a_now;
        display.pausedAt = {};
        display.active = true;
        display.paused = false;
    }

    void HUD::AppendBackupAge(HUDMessage& a_message, const std::chrono::steady_clock::time_point& a_now) const
    {
        if (!a_message.showBackupAge) {
            return;
        }

        const auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(a_now - a_message.createdAt).count();
        if (elapsedMinutes <= 0) {
            a_message.segments.push_back({ Trans::Tr("    Last backup: just now"), HUDColor::Muted });
        }
        else if (elapsedMinutes < 60) {
            a_message.segments.push_back({ Trans::Format("    Last backup: {}m ago", elapsedMinutes), HUDColor::Muted });
        }
        else {
            const auto elapsedHours = elapsedMinutes / 60;
            a_message.segments.push_back({ Trans::Format("    Last backup: {}h ago", elapsedHours), HUDColor::Muted });
        }
        a_message.showBackupAge = false;
    }

    GUI::ImVec4 HUD::GetColor(HUDColor a_color, float a_alpha) const
    {
        auto color = HUDColors[static_cast<size_t>(a_color)];
        color.w *= a_alpha;
        return color;
    }

    std::string HUD::GetDisplayModName(std::string_view a_modName) const
    {
        std::string modName{ a_modName };
        if (!modName.starts_with('$')) {
            return modName;
        }

        std::string translatedName;
        if (SKSE::Translation::Translate(modName, translatedName) && !translatedName.empty()) {
            modName = std::move(translatedName);
        }
        if (modName.starts_with('$')) {
            modName.erase(0, 1);
        }
        return modName;
    }

    void HUD::DrawMessage(const HUDMessage& a_message, float a_alpha) const
    {
        if (a_message.segments.empty() || a_alpha <= 0.0F) {
            return;
        }

        auto* drawList = GUI::GetForegroundDrawList();
        auto* io = GUI::GetIO();
        auto* font = GUI::GetFont();
        if (!drawList || !io || !font) {
            return;
        }

        float totalWidth{};
        for (const auto& segment : a_message.segments) {
            GUI::ImVec2 size = GUI::CalcTextSize(segment.text.c_str(), nullptr, false, 0.0F);
            totalWidth += size.x;
        }
        if (totalWidth <= 0.0F || io->DisplaySize.x <= 0.0F) {
            return;
        }

        const float requestedScale = static_cast<float>(GetSettings().notificationFontScale) / 100.0F;
        const float availableWidth = io->DisplaySize.x - 2.0F * HUDMargin - 2.0F * HUDHorizontalPadding;
        if (availableWidth <= 0.0F) {
            return;
        }
        const float fontScale = std::min(requestedScale, availableWidth / totalWidth);
        const float scaledWidth = totalWidth * fontScale;
        const float scaledHeight = font->FontSize * fontScale;
        const float textX = io->DisplaySize.x - HUDMargin - HUDHorizontalPadding - scaledWidth;
        const float textY = HUDMargin + HUDVerticalPadding;

        // Draw background box with round edges.
        const GUI::ImVec2 backgroundMin{ textX - HUDHorizontalPadding, textY - HUDVerticalPadding };
        const GUI::ImVec2 backgroundMax{ textX + scaledWidth + HUDHorizontalPadding, textY + scaledHeight + HUDVerticalPadding };
        const auto backgroundColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.04F, 0.04F, 0.05F, 0.78F * a_alpha });
        GUI::ImDrawListManager::AddRectFilled(drawList, backgroundMin, backgroundMax, backgroundColor, 5.0F, 0);

        // Draw each segment of text with a shadow.
        float positionX = textX;
        for (const auto& segment : a_message.segments) {
            GUI::ImVec2 size = GUI::CalcTextSize(segment.text.c_str(), nullptr, false, 0.0F);
            const GUI::ImVec2 shadowPosition{ positionX + 1.0F, textY + 1.0F };
            const GUI::ImVec2 textPosition{ positionX, textY };
            const auto shadowColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.0F, 0.0F, 0.0F, 0.85F * a_alpha });
            const auto textColor = GUI::ColorConvertFloat4ToU32(GetColor(segment.color, a_alpha));
            GUI::ImDrawListManager::AddText(drawList, font, scaledHeight, shadowPosition, shadowColor, segment.text.c_str());
            GUI::ImDrawListManager::AddText(drawList, font, scaledHeight, textPosition, textColor, segment.text.c_str());
            positionX += size.x * fontScale;
        }
    }

    void HUD::Render()
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const bool blocked = SKSEMenuFramework::IsAnyBlockingWindowOpened();
        const auto* menuWindow = SKSEMenuFramework::GetMainWindow();
        const bool menuOpen = menuWindow && menuWindow->IsOpen.load(std::memory_order_relaxed);

        std::lock_guard lock(hudMutex);

        if (!rendererSeen) {
            rendererSeen = true;
            logger::info("HUD renderer is active");
        }

        if (blocked && !menuOpen && (!display.active || !display.message.allowWhileBlocked)) {
            if (display.active && !display.paused) {
                display.pausedAt = now;
                display.paused = true;
            }
            return;
        }
        if (display.active && display.paused) {
            display.startedAt += now - display.pausedAt;
            display.pausedAt = {};
            display.paused = false;
        }

        if (display.active) {
            const float age = std::chrono::duration<float>(now - display.startedAt).count();
            const float duration = GetSettings().notificationDurationSeconds;
            const float fade = GetSettings().notificationFadeSeconds;
            if (age >= duration + fade) {
                display.active = false;
                display.nextAt = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(HUDGapSeconds));
                return;
            }

            float alpha = 1.0F;
            if (fade > 0.0F && age > duration) {
                alpha = 1.0F - (age - duration) / fade;
            }
            DrawMessage(display.message, alpha);
            return;
        }

        if (display.nextAt.time_since_epoch().count() != 0 && now < display.nextAt) {
            return;
        }
        if (!notificationQueue.empty()) {
            if (now < notificationQueue.front().showAt) {
                return;
            }
            auto message = std::move(notificationQueue.front());
            notificationQueue.pop_front();
            StartMessage(std::move(message), now);
        }
        else {
            return;
        }

        DrawMessage(display.message, 1.0F);
    }

    void __stdcall RenderHUD()
    {
        HUD::GetSingleton()->Render();
    }
}
