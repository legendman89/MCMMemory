#include "debug/coc_test.hpp"
#include "session.hpp"

namespace MCMMemory
{
    bool COCTest::Install()
    {
        if (installed) {
            return true;
        }

        auto* events = RE::ScriptEventSourceHolder::GetSingleton();
        if (!events) {
            logger::error("COC testing could not find the game event source");
            return false;
        }

        events->AddEventSink<RE::TESCellFullyLoadedEvent>(this);
        installed = true;
        logger::info("COC session detection is successfully installed for testing");
        return true;
    }

    RE::BSEventNotifyControl COCTest::ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
    {
        auto* session = GameSession::GetSingleton();
        if (!a_event || !a_event->cell || IsGameLoaded() || session->IsWaitingForLoadMessage()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || player->GetParentCell() != a_event->cell) {
            return RE::BSEventNotifyControl::kContinue;
        }

        session->Start(false, "COC cell entry");
        return RE::BSEventNotifyControl::kContinue;
    }
}
