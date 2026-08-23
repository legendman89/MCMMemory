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
        SKSEMenuFramework::AddSectionItem("Profile", RenderProfile);
        SKSEMenuFramework::AddHudElement(RenderHUD);
        logger::info("MCM Memory menu registered with SKSE Menu Framework {}", version);
    }

    void __stdcall RenderProfile()
    {
        const bool operationRunning = Backup::GetSingleton()->IsRunning() || Restore::GetSingleton()->IsRunning();
        const bool operationAvailable = IsGameLoaded() && !operationRunning;

        if (IconCTAButton(Trans::Tr("Back Up Now").c_str(), operationAvailable, Icons::kSave, Color::kBackupButtonColors)) {
            Backup::GetSingleton()->Start();
        }

        GUI::SameLine(0.0F, 18.0F);

        if (IconCTAButton(Trans::Tr("Restore Now").c_str(), operationAvailable, Icons::kRestore, Color::kRestoreButtonColors)) {
            Restore::GetSingleton()->Start();
        }

        GUI::Spacing();

        auto& settings = GetSettings();
        bool settingsChanged{};

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

            GUI::Spacing();

            if (IconButton(Trans::Tr("Preview").c_str(), Icons::kPreview, Color::kPreviewButtonColors)) {
                HUD::GetSingleton()->Preview();
            }

            GUI::SeparatorText(Trans::Tr("Timing").c_str());
#define DRAW_HUD_TIMING_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
            if (GUI::SliderFloat(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                settingsChanged = true; \
            }
            FOREACH_HUD_TIMING_SETTING(DRAW_HUD_TIMING_SETTING)
#undef DRAW_HUD_TIMING_SETTING

            GUI::SeparatorText(Trans::Tr("Appearance").c_str());
#define DRAW_HUD_APPEARANCE_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
            if (GUI::SliderInt(Trans::Tr(label).c_str(), std::addressof(settings.settingName), minimum, maximum, format)) { \
                settingsChanged = true; \
            }
            FOREACH_HUD_APPEARANCE_SETTING(DRAW_HUD_APPEARANCE_SETTING)
#undef DRAW_HUD_APPEARANCE_SETTING
            GUI::EndDisabled();

            GUI::SeparatorText(Trans::Tr("Restore Warning").c_str());
#define DRAW_HUD_WARNING_SETTING(type, settingName, defaultValue, optionName, minimum, maximum, label, format) \
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
    }
}
