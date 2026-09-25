#include "menu/hud.hpp"
#include "menu/menu.hpp"
#include "utils/logger.hpp"
#include "profile/backup.hpp"
#include "profile/capture.hpp"
#include "profile/restore.hpp"
#include "profile/profiles.hpp"
#include "profile/activity.hpp"
#include "mcm/mcm_registry.hpp"
#include "mcm/mcm_messages.hpp"

#include "debug/coc_test.hpp"

#include "settings.hpp"
#include "session.hpp"


namespace MCMMemory
{

    inline void OnPostLoad()
    {
        Menu::Register();
    }

    inline void OnPreLoadGame()
    {
        GameSession::GetSingleton()->PrepareLoad();
    }

    inline void OnLoadGame(SKSE::MessagingInterface::Message* a_message)
    {
        const bool autoRestoreAllowed = a_message->type == SKSE::MessagingInterface::kNewGame;
        auto* session = GameSession::GetSingleton();
        if (!autoRestoreAllowed || !session->HasNewGameStarted()) {
            session->Start(autoRestoreAllowed, autoRestoreAllowed ? "new game" : "loaded save");
        }
    }

    void OnDataLoaded()
    {
        if (!MCMRegistry::IsSkyUIAvailable()) {
            logger::critical("SkyUI is required but its MCM manager quest is unavailable");
            return;
        }

        if (!SettingsStorage::Load()) {
            logger::critical("MCM Memory settings could not be loaded");
            return;
        }

        if (!Profiles::CheckSelection()) {
            logger::error("MCM Memory could not save its selected profile");
        }

        MCMRegistry::Install();

        MCMMessages::Install();

        HUD::GetSingleton()->Configure(GetSettings());
        if (!Activity::GetSingleton()->Load()) {
            logger::error("MCM Memory activity history could not be loaded");
        }

        Backup::GetSingleton()->Install();
        Capture::GetSingleton()->Install();
        Restore::GetSingleton()->Install();
        GameSession::GetSingleton()->Install();

        if (GetSettings().allowCOCForTesting) {
            COCTest::GetSingleton()->Install();
        }
    }

    void HandleSKSEMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (!a_message) {
            return;
        }

        switch (a_message->type) {

            case SKSE::MessagingInterface::kDataLoaded:
            {
                OnDataLoaded();
                break;
            }

            case SKSE::MessagingInterface::kPostLoad:
            {    
                OnPostLoad();
                break;
            }

            case SKSE::MessagingInterface::kPreLoadGame:
            {    
                OnPreLoadGame();
                break;
            }

            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame: 
            {
                OnLoadGame(a_message);
                break;
            }

            default: break;
        }
    }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    MCMMemory::Logger::SetupLog();

    SKSE::Init(a_skse, false);

    logger::info("{} v{} by {} (Game v{})", BEAUTIFUL_NAME, CURR_VERSION, AUTHOR_NAME, REL::Module::get().version().string("."));
    
    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MCMMemory::HandleSKSEMessage)) {
        logger::critical("Failed to register SKSE message listener");
        return false;
    }

    logger::info("SKSE message listener is registered successfully");

    return true;
}
