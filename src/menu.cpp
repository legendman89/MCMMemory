#include "menu.hpp"
#include "backup.hpp"
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
        if (settingsChanged && !SettingsStorage::Save()) {
            logger::error("MCM Memory menu could not save its settings");
        }
    }
}
