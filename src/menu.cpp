#include "menu.hpp"
#include "backup.hpp"
#include "hud.hpp"
#include "restore.hpp"
#include "settings.hpp"
#include "translate.hpp"

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
        GUI::BeginDisabled(operationRunning);
        if (GUI::Button(Trans::Tr("Back Up Now").c_str())) {
            Backup::GetSingleton()->Start();
        }
        GUI::SameLine();
        if (GUI::Button(Trans::Tr("Restore Now").c_str())) {
            Restore::GetSingleton()->Start();
        }
        GUI::EndDisabled();

        GUI::Spacing();

        auto& settings = GetSettings();
        bool settingsChanged{};

        if (GUI::CollapsingHeader(Trans::Tr("Automation").c_str(), 0)) {
            if (GUI::Checkbox(Trans::Tr("Automatic backup").c_str(), std::addressof(settings.autoBackup))) {
                settingsChanged = true;
            }
            if (GUI::Checkbox(Trans::Tr("Automatic restore").c_str(), std::addressof(settings.autoRestore))) {
                settingsChanged = true;
            }
        }

        GUI::Spacing();

        if (GUI::CollapsingHeader(Trans::Tr("HUD Notifications").c_str(), 0)) {
            if (GUI::Checkbox(Trans::Tr("Show backup and restore notifications").c_str(), std::addressof(settings.notifications))) {
                settingsChanged = true;
            }
            GUI::BeginDisabled(!settings.notifications);
            if (GUI::Checkbox(Trans::Tr("Show individual MCM results").c_str(), std::addressof(settings.individualMCMNotifications))) {
                settingsChanged = true;
            }
            GUI::SameLine();
            if (GUI::Button(Trans::Tr("Preview").c_str())) {
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
        }

        if (settingsChanged) {
            HUD::GetSingleton()->Configure(settings);
            if (!SettingsStorage::Save()) {
                logger::error("MCM Memory menu could not save its settings");
            }
        }
    }
}
