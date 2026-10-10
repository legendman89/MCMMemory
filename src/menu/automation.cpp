#include "menu/backend.hpp"
#include "menu/translate.hpp"
#include "menu/automation.hpp"

#include "settings.hpp"

namespace MCMMemory::Menu
{
    void RenderAutomation()
    {
        auto& settings = GetSettings();
        bool changed{};
        if (GUI::Checkbox(Trans::Tr("Profile.Automation.Backup").c_str(), std::addressof(settings.autoBackup))) {
            changed = true;
        }
        GUI::HelpMarker(Trans::Tr("Profile.Automation.Backup.Tooltip").c_str());

        GUI::SameLine(0.0F, 20.0F);

        const float restoreColumnX = GUI::GetCursorPosX();
        if (GUI::Checkbox(Trans::Tr("Profile.Automation.Restore").c_str(), std::addressof(settings.autoRestore))) {
            changed = true;
        }
        GUI::HelpMarker(Trans::Tr("Profile.Automation.Restore.Tooltip").c_str());

        GUI::Spacing();

        // Recording needs automatic backup running; there is no interaction order to monitor without it.
        GUI::BeginDisabled(!settings.autoBackup);
        if (GUI::Checkbox(Trans::Tr("Profile.Automation.Record").c_str(), std::addressof(settings.recordActions))) {
            changed = true;
        }
        GUI::EndDisabled();
        GUI::HelpMarker(Trans::Tr("Profile.Automation.Record.Tooltip").c_str());

        GUI::SameLine(restoreColumnX);

        if (GUI::Checkbox(Trans::Tr("Profile.Automation.Messages").c_str(), std::addressof(settings.dismissRestoreMessages))) {
            changed = true;
        }
        GUI::HelpMarker(Trans::Tr("Profile.Automation.Messages.Tooltip").c_str());

        if (changed && !SettingsStorage::Save()) {
            logger::error("MCM Memory menu could not save its automation settings");
        }
    }
}
