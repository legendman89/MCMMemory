#include "menu/hud.hpp"
#include "menu/backend.hpp"
#include "menu/profile.hpp"
#include "menu/activity.hpp"
#include "menu/translate.hpp"
#include "menu/frameworks.hpp"
#include "menu/notifications.hpp"

#include <cstring>

namespace MCMMemory::Menu
{
    void Frameworks::Register()
    {
        if (registered) {
            return;
        }

        if (!detected) {
            Detect();
        }

        if (HasSKSEMenuFramework()) {
            if (HasFLICK()) {
                logger::info("Both menu frameworks detected; using SKSE Menu Framework for the in-game menu");
            }
            RegisterSKSEMenuFramework();
        }
        else if (flickLoaded) {
            logger::info("FLICK detected; MCM Memory will connect after game data loads");
        }
        else {
            logger::info("No available menu framework detected; the in-game menu is disabled");
        }
    }

    void Frameworks::RegisterAfterDataLoaded()
    {
        if (registered) {
            return;
        }

        if (!detected) {
            Register();
        }

        if (!registered && flickLoaded) {
            registered = RegisterFLICK();
        }
    }

    void Frameworks::RegisterSKSEMenuFramework()
    {
#define GUI_REGISTER_SMF(name, result, args) GUI::RegisterFunction<GUI::Function::name>(GUI::SMF::name);
        FOREACH_GUI_FUNCTIONS(GUI_REGISTER_SMF)
#undef GUI_REGISTER_SMF

        Trans::GetTranslator().Load();
        SKSEMenuFramework::SetSection(BEAUTIFUL_NAME);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Profile").c_str(), RenderProfile);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Activity").c_str(), RenderActivity);
        SKSEMenuFramework::AddSectionItem(Trans::Tr("Menu.Tab.Notifications").c_str(), RenderNotifications);
        SKSEMenuFramework::AddHudElement(RenderHUD);
        registered = true;
        logger::info("MCM Memory menu registered with SKSE Menu Framework {}", skseVersion);
    }

    void Frameworks::Detect()
    {
        detected = true;

        skseVersion = SKSEMenuFramework::GetMenuFrameworkVersion();

        flickVersion = 0;

        if (HasSKSEMenuFramework()) {
            logger::info("Found SKSE Menu Framework version {}", skseVersion);
        }

        const auto module = GetModuleHandleW(L"FUCK.dll");
        flickLoaded = module != nullptr;

        if (!flickLoaded) {
            return;
        }

        using RequestInterface = void* (*)();
        const auto request = reinterpret_cast<RequestInterface>(GetProcAddress(module, "RequestFUCK"));
        if (!request) {
            logger::warn("FLICK is loaded but RequestFUCK is unavailable");
            return;
        }

        const auto* api = request();
        if (!api) {
            logger::warn("FLICK returned no API interface");
            return;
        }

        // The FLICK API version is a 32-bit unsigned at the start of the interface.
        // If that member moved elsewhere we are FUCKed.
        std::memcpy(&flickVersion, api, sizeof(flickVersion));
        if (HasFLICK()) {
            logger::info("Found FLICK version {}", flickVersion);
        }
        else {
            logger::warn("FLICK returned an invalid API version 0");
        }
    }
}
