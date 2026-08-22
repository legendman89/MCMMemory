#include "menu/hud.hpp"
#include "menu/translate.hpp"
#include "settings.hpp"

namespace MCMMemory
{
    inline constexpr float HUDMargin{ 30.0F };
    inline constexpr float HUDHorizontalPadding{ 12.0F };
    inline constexpr float HUDVerticalPadding{ 7.0F };

    void HUD::Configure(const Settings& a_settings)
    {
        enabled.store(a_settings.notifications, std::memory_order_relaxed);
        individualMCMs.store(a_settings.individualMCMNotifications, std::memory_order_relaxed);

        std::lock_guard lock(hudMutex);
#define COPY_HUD_OPTION(type, settingName, defaultValue, optionName, minimum, maximum, label, format) options.optionName = a_settings.settingName;
        FOREACH_HUD_SETTING(COPY_HUD_OPTION)
#undef COPY_HUD_OPTION

        if (!a_settings.notifications) {
            notificationQueue.clear();
            display.Reset();
            menuResumeAt = {};
            gameMenuBlocked = false;
            return;
        }

        for (auto& message : notificationQueue) {
            const auto delay = std::chrono::duration<float>(GetDelaySeconds(message.type));
            message.showAt = message.createdAt + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
        }

        if (!a_settings.individualMCMNotifications) {
            auto message = notificationQueue.begin();
            while (message != notificationQueue.end()) {
                if (message->type == HUDMessageType::MCMResult) {
                    message = notificationQueue.erase(message);
                }
                else {
                    ++message;
                }
            }
            if (display.active && display.message.type == HUDMessageType::MCMResult) {
                display.Reset();
            }
        }
    }

    void HUD::Reset()
    {
        std::lock_guard lock(hudMutex);
        notificationQueue.clear();
        display.Reset();
        menuResumeAt = {};
        gameMenuBlocked = false;
    }

    void HUD::ShowOperationStarted(std::string_view a_text)
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.type = HUDMessageType::OperationStarted;
        message.segments.push_back({ Trans::Tr(a_text), HUDColor::Accent });
        BeginOperation(std::move(message));
    }

    void HUD::ShowBackupMCM(std::string_view a_modName, const BackupStats& a_stats)
    {
        if (!enabled.load(std::memory_order_relaxed) || !individualMCMs.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.type = HUDMessageType::MCMResult;
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
        message.type = HUDMessageType::MCMResult;
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
        message.type = HUDMessageType::BackupSummary;
        message.segments.push_back({ Trans::Format("{} MCMs backed up", a_stats.MCMCount), HUDColor::Success });
        message.segments.push_back({ Trans::Format("    {} settings", a_stats.settingCount), HUDColor::Accent });
        if (a_stats.skippedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("    {} skipped", a_stats.skippedSettingCount), HUDColor::Warning });
        }
        if (a_stats.failedMCMCount > 0) {
            message.segments.push_back({ Trans::Format("    {} MCMs failed", a_stats.failedMCMCount), HUDColor::Error });
        }
        QueueMessage(std::move(message));
    }

    void HUD::ShowRestoreSummary(const RestoreStats& a_stats)
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.type = HUDMessageType::RestoreSummary;
        message.segments.push_back({ Trans::Format("{} MCMs restored", a_stats.MCMCount), HUDColor::Success });
        message.segments.push_back({ Trans::Format("    {} changed", a_stats.appliedSettingCount), HUDColor::Accent });
        message.segments.push_back({ Trans::Format("    {} already set", a_stats.unchangedSettingCount), HUDColor::Muted });
        if (a_stats.skippedSettingCount > 0) {
            message.segments.push_back({ Trans::Format("    {} skipped", a_stats.skippedSettingCount), HUDColor::Warning });
        }
        QueueMessage(std::move(message));
    }

    void HUD::ShowFailure(std::string_view a_title, std::string_view a_detail)
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.type = HUDMessageType::Failure;
        message.segments.push_back({ Trans::Tr(a_title), HUDColor::Error });
        if (!a_detail.empty()) {
            message.segments.push_back({ std::format("    {}", Trans::Tr(a_detail)), HUDColor::Muted });
        }
        QueueFailure(std::move(message));
    }

    void HUD::Preview()
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        HUDMessage message;
        message.type = HUDMessageType::Preview;
        message.segments.push_back({ Trans::Tr("42 MCMs backed up"), HUDColor::Success });
        message.segments.push_back({ Trans::Tr("    318 settings"), HUDColor::Accent });

        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(hudMutex);
        gameMenuBlocked = false;
        menuResumeAt = {};
        StartMessage(std::move(message), now);
        logger::info("HUD notification preview started");
    }

    void HUD::BeginOperation(HUDMessage a_message)
    {
        std::lock_guard lock(hudMutex);
        const auto delay = std::chrono::duration<float>(GetDelaySeconds(a_message.type));
        a_message.showAt = a_message.createdAt + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
        notificationQueue.clear();
        display.Reset();
        notificationQueue.push_back(std::move(a_message));
    }

    void HUD::QueueMessage(HUDMessage a_message)
    {
        std::lock_guard lock(hudMutex);
        const auto delay = std::chrono::duration<float>(GetDelaySeconds(a_message.type));
        a_message.showAt = a_message.createdAt + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
        notificationQueue.push_back(std::move(a_message));
    }

    void HUD::QueueFailure(HUDMessage a_message)
    {
        std::lock_guard lock(hudMutex);
        a_message.showAt = a_message.createdAt;
        notificationQueue.clear();
        notificationQueue.push_back(std::move(a_message));
    }

    bool HUD::UpdateMenuDelay(bool a_blocked, const std::chrono::steady_clock::time_point& a_now)
    {
        const bool previewActive = display.active && display.message.type == HUDMessageType::Preview;
        if (a_blocked && !previewActive) {
            gameMenuBlocked = true;
            if (display.active && display.pausedAt.time_since_epoch().count() == 0) {
                display.pausedAt = a_now;
            }
            return true;
        }
        if (previewActive) {
            gameMenuBlocked = false;
            menuResumeAt = {};
            return false;
        }

        if (gameMenuBlocked) {
            gameMenuBlocked = false;
            if (display.active || !notificationQueue.empty()) {
                const auto delay = std::chrono::duration<float>(options.menuCloseDelaySeconds);
                menuResumeAt = a_now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
            }
        }
        if (menuResumeAt.time_since_epoch().count() != 0 && a_now < menuResumeAt) {
            return true;
        }
        if (menuResumeAt.time_since_epoch().count() != 0) {
            menuResumeAt = {};
            if (display.pausedAt.time_since_epoch().count() != 0) {
                display.startedAt += a_now - display.pausedAt;
                display.pausedAt = {};
            }
        }
        return false;
    }

    bool HUD::UpdateActiveMessage(const std::chrono::steady_clock::time_point& a_now)
    {
        if (!display.active) {
            return false;
        }

        const float age = std::chrono::duration<float>(a_now - display.startedAt).count();
        if (age >= options.durationSeconds + options.fadeSeconds) {
            display.active = false;
            const auto gap = std::chrono::duration<float>(options.gapSeconds);
            display.nextAt = a_now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(gap);
            return true;
        }

        float alpha = 1.0F;
        if (options.fadeSeconds > 0.0F && age > options.durationSeconds) {
            alpha = 1.0F - (age - options.durationSeconds) / options.fadeSeconds;
        }
        DrawMessage(display.message, alpha);
        return true;
    }

    bool HUD::StartNextMessage(const std::chrono::steady_clock::time_point& a_now)
    {
        if (display.nextAt.time_since_epoch().count() != 0 && a_now < display.nextAt) {
            return false;
        }
        if (notificationQueue.empty() || a_now < notificationQueue.front().showAt) {
            return false;
        }

        auto message = std::move(notificationQueue.front());
        notificationQueue.pop_front();
        StartMessage(std::move(message), a_now);
        DrawMessage(display.message, 1.0F);
        return true;
    }

    void HUD::AppendBackupAge(HUDMessage& a_message, const std::chrono::steady_clock::time_point& a_now) const
    {
        if (a_message.type != HUDMessageType::BackupSummary && a_message.type != HUDMessageType::Preview) {
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
            const GUI::ImVec2 size = GUI::CalcTextSize(segment.text.c_str(), nullptr, false, 0.0F);
            totalWidth += size.x;
        }
        if (totalWidth <= 0.0F || io->DisplaySize.x <= 0.0F) {
            return;
        }

        const float requestedScale = static_cast<float>(options.fontScale) / 100.0F;
        const float availableWidth = io->DisplaySize.x - 2.0F * HUDMargin - 2.0F * HUDHorizontalPadding;
        if (availableWidth <= 0.0F) {
            return;
        }
        const float fontScale = std::min(requestedScale, availableWidth / totalWidth);
        const float scaledWidth = totalWidth * fontScale;
        const float scaledHeight = font->FontSize * fontScale;
        const float textX = io->DisplaySize.x - HUDMargin - HUDHorizontalPadding - scaledWidth;
        const float textY = HUDMargin + HUDVerticalPadding;

        // Draw a dark background behind the notification.
        const GUI::ImVec2 backgroundMin{ textX - HUDHorizontalPadding, textY - HUDVerticalPadding };
        const GUI::ImVec2 backgroundMax{ textX + scaledWidth + HUDHorizontalPadding, textY + scaledHeight + HUDVerticalPadding };
        const auto backgroundColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.04F, 0.04F, 0.05F, 0.78F * a_alpha });
        GUI::ImDrawListManager::AddRectFilled(drawList, backgroundMin, backgroundMax, backgroundColor, 5.0F, 0);

        float positionX = textX;
        for (const auto& segment : a_message.segments) {
            const GUI::ImVec2 size = GUI::CalcTextSize(segment.text.c_str(), nullptr, false, 0.0F);
            const GUI::ImVec2 shadowPosition{ positionX + 1.0F, textY + 1.0F };
            const GUI::ImVec2 textPosition{ positionX, textY };
            const auto shadowColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.0F, 0.0F, 0.0F, 0.85F * a_alpha });
            const auto textColor = GUI::ColorConvertFloat4ToU32(GetColor(segment.color, a_alpha));
            GUI::ImDrawListManager::AddText(drawList, font, scaledHeight, shadowPosition, shadowColor, segment.text.c_str());
            GUI::ImDrawListManager::AddText(drawList, font, scaledHeight, textPosition, textColor, segment.text.c_str());
            positionX += size.x * fontScale;
        }
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
        if (UpdateMenuDelay(blocked && !menuOpen, now)) {
            return;
        }
        if (UpdateActiveMessage(now)) {
            return;
        }
        StartNextMessage(now);
    }

}
