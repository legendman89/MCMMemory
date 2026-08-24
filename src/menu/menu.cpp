#include "menu/activity.hpp"
#include "menu/hud.hpp"
#include "menu/menu.hpp"
#include "menu/icons.hpp"
#include "menu/translate.hpp"
#include "profile/backup.hpp"
#include "profile/restore.hpp"
#include "settings.hpp"

namespace MCMMemory::Menu
{
    void Register()
    {
        const float version = SKSEMenuFramework::GetMenuFrameworkVersion();
        if (version <= 0.0F) {
            logger::info("SKSE Menu Framework is not available; the MCM Memory menu is disabled");
            return;
        }

        Trans::GetTranslator().Load();
        SKSEMenuFramework::SetSection(BEAUTIFUL_NAME);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Profile").c_str(), RenderProfile);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Activity").c_str(), RenderActivity);
        SKSEMenuFramework::AddHudElement(RenderHUD);
        logger::info("MCM Memory menu registered with SKSE Menu Framework {}", version);
    }

    void __stdcall RenderProfile()
    {
        auto* backup = Backup::GetSingleton();
        auto* restore = Restore::GetSingleton();
        const auto backupStatus = backup->GetStatus();
        const auto restoreStatus = restore->GetStatus();
        const bool operationRunning = backupStatus != OperationStatus::Idle || restoreStatus != OperationStatus::Idle;
        const bool operationAvailable = IsGameLoaded() && !operationRunning;
        const float rowStart = GUI::GetCursorPosX();
        const float rowWidth = GUI::GetContentRegionAvail().x;

        if (IconCTAButton(Trans::Tr("Back Up Now").c_str(), operationAvailable, Icons::kSave, Color::kBackupButtonColors)) {
            backup->Start();
        }

        GUI::SameLine(0.0F, 14.0F);

        if (IconCTAButton(Trans::Tr("Restore Now").c_str(), operationAvailable, Icons::kRestore, Color::kRestoreButtonColors)) {
            restore->Start();
        }

        const bool stopping = backupStatus == OperationStatus::Stopping || restoreStatus == OperationStatus::Stopping;
        const bool cancellationAvailable = backupStatus == OperationStatus::Running || restoreStatus == OperationStatus::Running;
        auto cancelText = Trans::Tr("Cancel");
        if (stopping) {
            cancelText = Trans::Tr("Stopping...");
        }
        else if (backupStatus == OperationStatus::Running) {
            cancelText = Trans::Tr("Cancel Backup");
        }
        else if (restoreStatus == OperationStatus::Running) {
            cancelText = Trans::Tr("Cancel Restore");
        }
        const std::array<std::string, 3> cancellationLabels{ Trans::Tr("Cancel Backup"), Trans::Tr("Cancel Restore"), Trans::Tr("Stopping...") };
        float cancelWidth{};
        for (const auto& label : cancellationLabels) {
            cancelWidth = std::max(cancelWidth, GUI::CalcTextSize(label.c_str()).x);
        }
        cancelWidth += 28.0F;

        GUI::SameLine();
        GUI::SetCursorPosX(std::max(GUI::GetCursorPosX(), rowStart + rowWidth - cancelWidth));
        if (CTAButton(cancelText.c_str(), cancellationAvailable, Color::kCancelButtonColors, GUI::ImVec2{ cancelWidth, 0.0F })) {
            if (backupStatus == OperationStatus::Running) {
                backup->Cancel();
            }
            else if (restoreStatus == OperationStatus::Running) {
                restore->Cancel();
            }
        }
        if (restoreStatus != OperationStatus::Idle) {
            WrappedTooltip(Trans::Tr("Settings already restored will not be reverted.").c_str());
        }

        GUI::Spacing();

        auto& settings = GetSettings();
        constexpr float hudSliderWidth = 360.0F;
        bool settingsChanged{};
        bool appearanceSettingActive{};

        if (GUI::CollapsingHeader(Trans::Tr("Automation").c_str(), GUI::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (GUI::Checkbox(Trans::Tr("Automatic backup").c_str(), std::addressof(settings.autoBackup))) {
                settingsChanged = true;
            }

            GUI::SameLine(0.0F, 20.0F);

            if (GUI::Checkbox(Trans::Tr("Automatic restore").c_str(), std::addressof(settings.autoRestore))) {
                settingsChanged = true;
            }
        }

        GUI::Spacing();

        if (GUI::CollapsingHeader(Trans::Tr("HUD Notifications").c_str(), GUI::ImGuiTreeNodeFlags_DefaultOpen)) {

            if (GUI::Checkbox(Trans::Tr("Show backup and restore notifications").c_str(), std::addressof(settings.notifications))) {
                settingsChanged = true;
            }

            GUI::BeginDisabled(!settings.notifications);

            if (GUI::Checkbox(Trans::Tr("Show per-mod notifications in auto mode").c_str(), std::addressof(settings.perModNotificationsAuto))) {
                settingsChanged = true;
            }

            GUI::SameLine(0.0F, 18.0F);

            if (GUI::Checkbox(Trans::Tr("Show per-mod notifications in manual mode").c_str(), std::addressof(settings.perModNotificationsManual))) {
                settingsChanged = true;
            }

            GUI::SeparatorText(Trans::Tr("Appearance").c_str());

            if (IconButton(Trans::Tr("Preview").c_str(), Icons::kPreview, Color::kPreviewButtonColors)) {
                HUD::GetSingleton()->Preview();
            }

            GUI::Spacing();

#define DRAW_HUD_FONT_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
            GUI::SetNextItemWidth(hudSliderWidth); \
            if (GUI::SliderInt(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                settingsChanged = true; \
            } \
            appearanceSettingActive = appearanceSettingActive || GUI::IsItemActive();
            FOREACH_HUD_FONT_SETTING(DRAW_HUD_FONT_SETTING)
#undef DRAW_HUD_FONT_SETTING

            const auto compactTableFlags = GUI::ImGuiTableFlags_SizingStretchSame;
            if (GUI::BeginTable("HUD notification offsets", 2, compactTableFlags)) {
#define DRAW_HUD_OFFSET_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
                GUI::TableNextColumn(); \
                GUI::SetNextItemWidth(hudSliderWidth); \
                if (GUI::SliderInt(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                    settingsChanged = true; \
                } \
                appearanceSettingActive = appearanceSettingActive || GUI::IsItemActive();
                FOREACH_HUD_OFFSET_SETTING(DRAW_HUD_OFFSET_SETTING)
#undef DRAW_HUD_OFFSET_SETTING
                GUI::EndTable();
            }

            GUI::SeparatorText(Trans::Tr("Timing").c_str());
            if (GUI::BeginTable("HUD notification timing", 2, compactTableFlags)) {
#define DRAW_HUD_TIMING_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
                GUI::TableNextColumn(); \
                GUI::SetNextItemWidth(hudSliderWidth); \
                if (GUI::SliderFloat(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                    settingsChanged = true; \
                }
                FOREACH_HUD_TIMING_SETTING(DRAW_HUD_TIMING_SETTING)
#undef DRAW_HUD_TIMING_SETTING
                GUI::EndTable();
            }
            GUI::EndDisabled();

            // Warning settings are always enabled, even if notifications are disabled, because they are critical to the user experience.
#define DRAW_HUD_WARNING_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
            GUI::SetNextItemWidth(hudSliderWidth); \
            if (GUI::SliderFloat(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                settingsChanged = true; \
            }
            FOREACH_HUD_WARNING_SETTING(DRAW_HUD_WARNING_SETTING)
#undef DRAW_HUD_WARNING_SETTING
        }

        if (settingsChanged) {
            HUD::GetSingleton()->Configure(settings);
            if (!SettingsStorage::Save()) {
                logger::error("MCM Memory menu could not save its settings");
            }
        }

        if (appearanceSettingActive) {
            HUD::GetSingleton()->KeepPreviewAlive();
        }
    }
}
