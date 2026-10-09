#include "menu/hud.hpp"
#include "menu/menu.hpp"
#include "menu/profile.hpp"
#include "menu/activity.hpp"
#include "menu/translate.hpp"
#include "menu/frameworks.hpp"

#include "menu/notifications.hpp"

namespace MCMMemory::Menu
{
    void Register()
    {
        Frameworks frameworks;
        frameworks.Detect();
        if (!frameworks.HasSKSEMenuFramework()) {
            if (frameworks.HasFLICK()) {
                logger::info("FLICK detected; its MCM Memory menu integration is pending, so the menu is disabled");
            }
            else {
                logger::info("No available menu framework detected; the MCM Memory menu is disabled");
            }
            return;
        }

        if (frameworks.HasFLICK()) {
            logger::info("Both menu frameworks detected; using SKSE Menu Framework for the MCM Memory menu");
        }

        Trans::GetTranslator().Load();
        SKSEMenuFramework::SetSection(BEAUTIFUL_NAME);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Profile").c_str(), RenderProfile);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Activity").c_str(), RenderActivity);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Notifications").c_str(), RenderNotifications);
        SKSEMenuFramework::AddHudElement(RenderHUD);
        logger::info("MCM Memory menu registered with SKSE Menu Framework {}", frameworks.skseVersion);
    }
}
