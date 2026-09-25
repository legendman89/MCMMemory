#pragma once

#include "plugin.hpp"

#include "menu/hud.hpp"
#include "mcm/mcm_calls.hpp"
#include "mcm/mcm_support.hpp"
#include "profile/backup.hpp"
#include "profile/capture.hpp"
#include "profile/restore.hpp"

namespace MCMMemory
{
    // Singleton class that manages the state of the current game session.
    // The event is used as an alternative if SKSE VR doesn't fire load game signal.
    class GameSession : public RE::BSTEventSink<RE::TESLoadGameEvent>
    {
    public:

        bool Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* a_event, RE::BSTEventSource<RE::TESLoadGameEvent>* a_source) override;

        static GameSession* GetSingleton()
        {
            static GameSession singleton;
            return std::addressof(singleton);
        }

        inline void PrepareLoad()
        {
            loadMessagesReceived = true;
            waitingForLoadMessage = true;
            newGameStarted = false;
            SetGameLoaded(false);
            HUD::GetSingleton()->Reset();
        }

        inline void Start(bool a_autoRestoreAllowed, std::string_view a_reason)
        {
            waitingForLoadMessage = false;
            newGameStarted = a_autoRestoreAllowed;
            HUD::GetSingleton()->Reset();
            MCMRegistry::Reset();
            Backup::GetSingleton()->Reset();
            Capture::GetSingleton()->Reset();
            Restore::GetSingleton()->Reset(a_autoRestoreAllowed);
            MCMCallWatch::ResetSession();
            SetGameLoaded(true);
            logger::info("Game session started from {}; automatic restore allowed: {}", a_reason, a_autoRestoreAllowed);
        }

        inline void End()
        {
            const bool sessionActive = IsGameLoaded();
            waitingForLoadMessage = false;
            newGameStarted = false;
            SetGameLoaded(false);
            HUD::GetSingleton()->Reset();
            if (sessionActive) {
                logger::info("Game session ended at the main menu");
            }
        }

        inline bool HasNewGameStarted() const
        {
            return newGameStarted;
        }

        inline bool IsWaitingForLoadMessage() const
        {
            return waitingForLoadMessage;
        }

    private:

        bool installed{};

        bool waitingForLoadMessage{};

        // For VR workaround.
        bool newGameStarted{};

        // If SKSE sends kPreLoadGame before the game reads the save, 
        // if it arrives, we don't need the game event.
        bool loadMessagesReceived{};
    };
}
