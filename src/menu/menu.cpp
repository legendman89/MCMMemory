#include "menu/activity.hpp"
#include "menu/hud.hpp"
#include "menu/menu.hpp"
#include "menu/notifications.hpp"
#include "menu/profile.hpp"
#include "menu/translate.hpp"

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
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Profile").c_str(), RenderProfile);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Activity").c_str(), RenderActivity);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Notifications").c_str(), RenderNotifications);
        SKSEMenuFramework::AddHudElement(RenderHUD);
        logger::info("MCM Memory menu registered with SKSE Menu Framework {}", version);
    }
}
