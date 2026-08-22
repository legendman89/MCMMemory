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
        GUI::TextUnformatted(Trans::Tr(BEAUTIFUL_NAME).c_str());

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
        GUI::SeparatorText(Trans::Tr("Automation").c_str());

        auto& settings = GetSettings();
        bool settingsChanged{};
        if (GUI::Checkbox(Trans::Tr("Automatic backup").c_str(), std::addressof(settings.autoBackup))) {
            settingsChanged = true;
        }
        if (GUI::Checkbox(Trans::Tr("Automatic restore").c_str(), std::addressof(settings.autoRestore))) {
            settingsChanged = true;
        }

        GUI::Spacing();
        GUI::SeparatorText(Trans::Tr("HUD Notifications").c_str());

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

        if (GUI::CollapsingHeader(Trans::Tr("Appearance").c_str(), 0)) {
            if (GUI::SliderInt(Trans::Tr("Font scale").c_str(), std::addressof(settings.notificationFontScale), 50, 200, "%d%%")) {
                settingsChanged = true;
            }
            if (GUI::SliderFloat(Trans::Tr("Display duration").c_str(), std::addressof(settings.notificationDurationSeconds), 0.5F, 15.0F, "%.1f s")) {
                settingsChanged = true;
            }
            if (GUI::SliderFloat(Trans::Tr("Fade duration").c_str(), std::addressof(settings.notificationFadeSeconds), 0.0F, 5.0F, "%.1f s")) {
                settingsChanged = true;
            }
        }
        GUI::EndDisabled();

        if (settingsChanged) {
            HUD::GetSingleton()->Configure(settings.notifications, settings.individualMCMNotifications);
            if (!SettingsStorage::Save()) {
                logger::error("MCM Memory menu could not save its settings");
            }
        }
    }
}
