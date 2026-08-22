
#include "menu/hud.hpp"
#include "menu/menu.hpp"
#include "mcm/mcm_registry.hpp"
#include "profile/backup.hpp"
#include "profile/capture.hpp"
#include "profile/restore.hpp"
#include "settings.hpp"
#include "utils/logger.hpp"


namespace MCMMemory
{
    void HandleSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        switch (a_message->type) {

            case SKSE::MessagingInterface::kDataLoaded:
                if (!MCMRegistry::IsSkyUIAvailable()) {
                    logger::critical("SkyUI is required, but its MCM manager quest is unavailable");
                    break;
                }
                if (!SettingsStorage::Load()) {
                    logger::critical("MCM Memory settings could not be loaded");
                    break;
                }
                HUD::GetSingleton()->Configure(GetSettings());
                Backup::GetSingleton()->Install();
                Capture::GetSingleton()->Install();
                Restore::GetSingleton()->Install();
                break;

            case SKSE::MessagingInterface::kPreLoadGame:
                HUD::GetSingleton()->Reset();
                break;

            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame:
                HUD::GetSingleton()->Reset();
                Backup::GetSingleton()->Reset();
                Capture::GetSingleton()->Reset();
                Restore::GetSingleton()->Reset();
                break;

            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    MCMMemory::Logger::SetupLog();

    SKSE::Init(a_skse);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MCMMemory::HandleSKSEMessage)) {
        logger::critical("Failed to register SKSE message listener");
        return false;
    }

    MCMMemory::Menu::Register();

    logger::info("{} plugin is loaded", BEAUTIFUL_NAME);

    return true;
}
