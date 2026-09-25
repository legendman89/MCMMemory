#include "session.hpp"

namespace MCMMemory
{
    bool GameSession::Install()
    {
        if (installed) {
            return true;
        }

        auto* events = RE::ScriptEventSourceHolder::GetSingleton();
        if (!events) {
            logger::error("Game load event could not find the game event source");
            return false;
        }

        events->AddEventSink<RE::TESLoadGameEvent>(this);
        installed = true;

        logger::info("Game load event is successfully installed");

        return true;
    }

    RE::BSEventNotifyControl GameSession::ProcessEvent(const RE::TESLoadGameEvent*, RE::BSTEventSource<RE::TESLoadGameEvent>*)
    {
        if (loadMessagesReceived) {
            return RE::BSEventNotifyControl::kContinue;
        }

        logger::warn("SKSE didn't send its load game messages; another plugin may be overriding SKSE hooks");

        Start(false, "game load event");
        
        return RE::BSEventNotifyControl::kContinue;
    }
}
