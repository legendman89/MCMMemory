#include "menu/hud.hpp"
#include "menu/translate.hpp"
#include "settings.hpp"
#include "utils/helper.hpp"

namespace MCMMemory
{
    inline constexpr float HUDMargin{ 30.0F };
    inline constexpr float HUDHorizontalPadding{ 12.0F };
    inline constexpr float HUDVerticalPadding{ 7.0F };
    inline constexpr float HUDWarningAccentWidth{ 6.0F };
    inline constexpr float HUDWarningHorizontalPadding{ 20.0F };
    inline constexpr float HUDWarningVerticalPadding{ 12.0F };
    inline constexpr float HUDWarningLineGap{ 7.0F };
    inline constexpr float HUDWarningScale{ 1.35F };

    void HUD::Configure(const Settings& a_settings)
    {
        enabled.store(a_settings.notifications, std::memory_order_relaxed);

        const std::array<bool, static_cast<size_t>(OperationMode::Count)> perModOptions
        {
            a_settings.perModNotificationsAuto,
            a_settings.perModNotificationsManual
        };

        for (size_t index = 0; index < perModOptions.size(); ++index) {
            perModNotifications[index].store(perModOptions[index], std::memory_order_relaxed);
        }

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
            message.showAt = TimeAfter(message.createdAt, GetDelaySeconds(message.type));
        }

        auto message = notificationQueue.begin();
        while (message != notificationQueue.end()) {
            if (message->type == HUDMessageType::MCMResult && !ShouldShowPerMod(message->operationMode)) {
                message = notificationQueue.erase(message);
            }
            else {
                ++message;
            }
        }
        if (display.active && display.message.type == HUDMessageType::MCMResult && !ShouldShowPerMod(display.message.operationMode)) {
            display.Reset();
        }
    }

    void HUD::Reset()
    {
        std::lock_guard lock(hudMutex);
        notificationQueue.clear();
        display.Reset();
        warning.Reset();
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

    void HUD::ShowBackupMCM(std::string_view a_modName, const BackupStats& a_stats, OperationMode a_operationMode)
    {
        if (!enabled.load(std::memory_order_relaxed) || !ShouldShowPerMod(a_operationMode)) {
            return;
        }

        HUDMessage message;
        message.type = HUDMessageType::MCMResult;
        message.operationMode = a_operationMode;
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

    void HUD::ShowRestoreMCM(std::string_view a_modName, const RestoreStats& a_stats, OperationMode a_operationMode)
    {
        if (!enabled.load(std::memory_order_relaxed) || !ShouldShowPerMod(a_operationMode)) {
            return;
        }

        HUDMessage message;
        message.type = HUDMessageType::MCMResult;
        message.operationMode = a_operationMode;
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

    void HUD::ShowRestoreMenuWarning()
    {
        std::lock_guard lock(hudMutex);
        warning.startedAt = {};
        warning.pausedAt = {};
        warning.active = true;
    }

    void HUD::StartPreview(const std::chrono::steady_clock::time_point& a_now)
    {
        HUDMessage message;
        message.type = HUDMessageType::Preview;
        message.segments.push_back({ Trans::Tr("42 MCMs backed up"), HUDColor::Success });
        message.segments.push_back({ Trans::Tr("    318 settings"), HUDColor::Accent });

        gameMenuBlocked = false;
        menuResumeAt = {};
        StartMessage(std::move(message), a_now);
    }

    void HUD::Preview()
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(hudMutex);
        StartPreview(now);
        logger::info("HUD notification preview started");
    }

    void HUD::KeepPreviewAlive()
    {
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(hudMutex);

        if (display.active && display.message.type != HUDMessageType::Preview) {
            return;
        }
        if (!notificationQueue.empty()) {
            return;
        }
        if (display.active) {
            gameMenuBlocked = false;
            menuResumeAt = {};
            display.startedAt = now;
            display.pausedAt = {};
            return;
        }

        StartPreview(now);
    }

    void HUD::BeginOperation(HUDMessage a_message)
    {
        std::lock_guard lock(hudMutex);
        a_message.showAt = TimeAfter(a_message.createdAt, GetDelaySeconds(a_message.type));
        notificationQueue.clear();
        display.Reset();
        notificationQueue.push_back(std::move(a_message));
    }

    void HUD::QueueMessage(HUDMessage a_message)
    {
        std::lock_guard lock(hudMutex);
        a_message.showAt = TimeAfter(a_message.createdAt, GetDelaySeconds(a_message.type));
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
            if (display.active && !IsTimeSet(display.pausedAt)) {
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
                menuResumeAt = TimeAfter(a_now, options.menuCloseDelaySeconds);
            }
        }
        if (IsTimeSet(menuResumeAt) && a_now < menuResumeAt) {
            return true;
        }
        if (IsTimeSet(menuResumeAt)) {
            menuResumeAt = {};
            if (IsTimeSet(display.pausedAt)) {
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

        const float age = SecondsSince(display.startedAt, a_now);
        if (age >= options.durationSeconds + options.fadeSeconds) {
            display.active = false;
            display.nextAt = TimeAfter(a_now, options.gapSeconds);
            return true;
        }

        DrawMessage(display.message, FadeAlpha(age, options.durationSeconds, options.fadeSeconds));
        return true;
    }

    bool HUD::StartNextMessage(const std::chrono::steady_clock::time_point& a_now)
    {
        if (IsTimeSet(display.nextAt) && a_now < display.nextAt) {
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
        const float availableWidth = io->DisplaySize.x - 2.0F * HUDHorizontalPadding;
        if (availableWidth <= 0.0F) {
            return;
        }
        const float fontScale = std::min(requestedScale, availableWidth / totalWidth);
        const float scaledWidth = totalWidth * fontScale;
        const float scaledHeight = font->FontSize * fontScale;
        const float backgroundWidth = scaledWidth + 2.0F * HUDHorizontalPadding;
        const float backgroundHeight = scaledHeight + 2.0F * HUDVerticalPadding;
        const float maximumX = std::max(0.0F, io->DisplaySize.x - backgroundWidth);
        const float maximumY = std::max(0.0F, io->DisplaySize.y - backgroundHeight);
        const float backgroundX = std::clamp(static_cast<float>(options.horizontalOffset), 0.0F, maximumX);
        const float backgroundY = std::clamp(static_cast<float>(options.verticalOffset), 0.0F, maximumY);
        const GUI::ImVec2 backgroundMin{ backgroundX, backgroundY };
        const GUI::ImVec2 backgroundMax{ backgroundX + backgroundWidth, backgroundY + backgroundHeight };
        const float textX = backgroundX + HUDHorizontalPadding;
        const float textY = backgroundY + HUDVerticalPadding;

        // The light translucent surface keeps the text readable without hiding the scene.
        const auto backgroundColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.96F, 0.96F, 0.96F, 0.75F * a_alpha });
        GUI::ImDrawListManager::AddRectFilled(drawList, backgroundMin, backgroundMax, backgroundColor, 7.0F, 0);

        float positionX = textX;
        for (const auto& segment : a_message.segments) {
            const GUI::ImVec2 size = GUI::CalcTextSize(segment.text.c_str(), nullptr, false, 0.0F);
            const GUI::ImVec2 shadowPosition{ positionX + 1.0F, textY + 1.0F };
            const GUI::ImVec2 textPosition{ positionX, textY };
            const auto shadowColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.0F, 0.0F, 0.0F, 0.18F * a_alpha });
            const auto textColor = GUI::ColorConvertFloat4ToU32(GetColor(segment.color, a_alpha));
            GUI::ImDrawListManager::AddText(drawList, font, scaledHeight, shadowPosition, shadowColor, segment.text.c_str());
            GUI::ImDrawListManager::AddText(drawList, font, scaledHeight, textPosition, textColor, segment.text.c_str());
            positionX += size.x * fontScale;
        }
    }

    void HUD::UpdateRestoreMenuWarning(bool a_blocked, const std::chrono::steady_clock::time_point& a_now)
    {
        if (!warning.active) {
            return;
        }
        if (a_blocked) {
            if (IsTimeSet(warning.startedAt) && !IsTimeSet(warning.pausedAt)) {
                warning.pausedAt = a_now;
            }
            return;
        }
        if (IsTimeSet(warning.pausedAt)) {
            warning.startedAt += a_now - warning.pausedAt;
            warning.pausedAt = {};
        }
        if (!IsTimeSet(warning.startedAt)) {
            warning.startedAt = a_now;
        }

        const float age = SecondsSince(warning.startedAt, a_now);
        if (age >= options.warningDurationSeconds) {
            warning.Reset();
            return;
        }

        const float fadeSeconds = std::min(options.fadeSeconds, options.warningDurationSeconds);
        const float fadeAt = options.warningDurationSeconds - fadeSeconds;
        DrawRestoreMenuWarning(FadeAlpha(age, fadeAt, fadeSeconds));
    }

    void HUD::DrawRestoreMenuWarning(float a_alpha) const
    {
        if (a_alpha <= 0.0F) {
            return;
        }

        auto* drawList = GUI::GetForegroundDrawList();
        auto* io = GUI::GetIO();
        auto* font = GUI::GetFont();
        if (!drawList || !io || !font || io->DisplaySize.x <= 0.0F || io->DisplaySize.y <= 0.0F) {
            return;
        }

        const auto title = Trans::Tr("Game menu closed");
        const auto detail = Trans::Tr("MCM restore is still running");
        const GUI::ImVec2 titleSize = GUI::CalcTextSize(title.c_str(), nullptr, false, 0.0F);
        const GUI::ImVec2 detailSize = GUI::CalcTextSize(detail.c_str(), nullptr, false, 0.0F);
        const float textWidth = std::max(titleSize.x, detailSize.x);
        const float availableWidth = io->DisplaySize.x - 2.0F * HUDMargin - 2.0F * HUDWarningHorizontalPadding;
        if (textWidth <= 0.0F || availableWidth <= 0.0F) {
            return;
        }

        const float requestedScale = static_cast<float>(options.fontScale) / 100.0F * HUDWarningScale;
        const float fontScale = std::min(requestedScale, availableWidth / textWidth);
        const float lineHeight = font->FontSize * fontScale;
        const float scaledWidth = textWidth * fontScale;
        const float cardWidth = scaledWidth + 2.0F * HUDWarningHorizontalPadding + HUDWarningAccentWidth;
        const float cardHeight = 2.0F * lineHeight + HUDWarningLineGap + 2.0F * HUDWarningVerticalPadding;
        const GUI::ImVec2 cardMin{ (io->DisplaySize.x - cardWidth) * 0.5F, (io->DisplaySize.y - cardHeight) * 0.5F };
        const GUI::ImVec2 cardMax{ cardMin.x + cardWidth, cardMin.y + cardHeight };
        const GUI::ImVec2 accentMax{ cardMin.x + HUDWarningAccentWidth, cardMax.y };
        const float textX = cardMin.x + HUDWarningAccentWidth + HUDWarningHorizontalPadding;
        const float titleY = cardMin.y + HUDWarningVerticalPadding;
        const float detailY = titleY + lineHeight + HUDWarningLineGap;
        const auto backgroundColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.96F, 0.96F, 0.96F, 0.86F * a_alpha });
        const auto accentColor = GUI::ColorConvertFloat4ToU32(GetColor(HUDColor::Warning, a_alpha));
        const auto titleColor = GUI::ColorConvertFloat4ToU32(GetColor(HUDColor::Warning, a_alpha));
        const auto detailColor = GUI::ColorConvertFloat4ToU32(GetColor(HUDColor::Primary, a_alpha));
        const auto shadowColor = GUI::ColorConvertFloat4ToU32(GUI::ImVec4{ 0.0F, 0.0F, 0.0F, 0.18F * a_alpha });

        GUI::ImDrawListManager::AddRectFilled(drawList, cardMin, cardMax, backgroundColor, 5.0F, 0);
        GUI::ImDrawListManager::AddRectFilled(drawList, cardMin, accentMax, accentColor, 5.0F, 0);
        GUI::ImDrawListManager::AddText(drawList, font, lineHeight, GUI::ImVec2{ textX + 1.0F, titleY + 1.0F }, shadowColor, title.c_str());
        GUI::ImDrawListManager::AddText(drawList, font, lineHeight, GUI::ImVec2{ textX, titleY }, titleColor, title.c_str());
        GUI::ImDrawListManager::AddText(drawList, font, lineHeight, GUI::ImVec2{ textX + 1.0F, detailY + 1.0F }, shadowColor, detail.c_str());
        GUI::ImDrawListManager::AddText(drawList, font, lineHeight, GUI::ImVec2{ textX, detailY }, detailColor, detail.c_str());
    }

    void HUD::Render()
    {
        const auto now = std::chrono::steady_clock::now();
        const bool blocked = SKSEMenuFramework::IsAnyBlockingWindowOpened();
        const auto* menuWindow = SKSEMenuFramework::GetMainWindow();
        const bool menuOpen = menuWindow && menuWindow->IsOpen.load(std::memory_order_relaxed);

        std::lock_guard lock(hudMutex);
        UpdateRestoreMenuWarning(blocked && !menuOpen, now);
        if (!enabled.load(std::memory_order_relaxed)) {
            return;
        }
        if (UpdateMenuDelay(blocked && !menuOpen, now)) {
            return;
        }
        if (UpdateActiveMessage(now)) {
            return;
        }
        StartNextMessage(now);
    }

}
