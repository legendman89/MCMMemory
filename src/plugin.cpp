#include "menu/hud.hpp"
#include "menu/menu.hpp"
#include "utils/logger.hpp"
#include "profile/capture.hpp"
#include "profile/profiles.hpp"
#include "profile/restore.hpp"
#include "profile/activity.hpp"
#include "mcm/mcm_registry.hpp"

#include "debug/coc_test.hpp"

#include "settings.hpp"
#include "session.hpp"


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
                if (!Profiles::CheckSelection()) {
                    logger::error("MCM Memory could not save its selected profile");
                }
                HUD::GetSingleton()->Configure(GetSettings());
                if (!Activity::GetSingleton()->Load()) {
                    logger::error("MCM Memory activity history could not be loaded");
                }
                Capture::GetSingleton()->Install();
                Restore::GetSingleton()->Install();
                if (GetSettings().allowCOCForTesting) {
                    COCTest::GetSingleton()->Install();
                }
                break;

            case SKSE::MessagingInterface::kPreLoadGame:
                GameSession::GetSingleton()->PrepareLoad();
                break;

            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame: {
                const bool autoRestoreAllowed = a_message->type == SKSE::MessagingInterface::kNewGame;
                GameSession::GetSingleton()->Start(autoRestoreAllowed, autoRestoreAllowed ? "new game" : "loaded save");
                break;
            }

            default:
                break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    MCMMemory::Logger::SetupLog();

    SKSE::Init(a_skse, false);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MCMMemory::HandleSKSEMessage)) {
        logger::critical("Failed to register SKSE message listener");
        return false;
    }

    MCMMemory::Menu::Register();

    logger::info("{} plugin is loaded", BEAUTIFUL_NAME);

    return true;
}
