
#include "logger.hpp"
#include "capture.hpp"
#include "restore.hpp"
#include "settings.hpp"
#include "mcm_registry.hpp"

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
            Capture::GetSingleton()->Install();
            Restore::GetSingleton()->Install();
            break;
        case SKSE::MessagingInterface::kNewGame:
        case SKSE::MessagingInterface::kPostLoadGame:
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

    logger::info("{} plugin is loaded", BEAUTIFUL_NAME);

    return true;
}
