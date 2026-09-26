#pragma once

#include "plugin.hpp"

#include "menu/hud.hpp"
#include "mcm/mcm_calls.hpp"
#include "mcm/mcm_support.hpp"
#include "mcm/mcm_close_watch.hpp"
#include "profile/backup.hpp"
#include "profile/capture.hpp"
#include "profile/restore.hpp"

namespace MCMMemory
{
    // Singleton class that manages the state of the current game session.
    class GameSession
    {
    public:

        static GameSession* GetSingleton()
        {
            static GameSession singleton;
            return std::addressof(singleton);
        }

        inline void PrepareLoad()
        {
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
            MCMCloseWatch::GetSingleton()->Reset();
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
            MCMCloseWatch::GetSingleton()->Reset();
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

        bool waitingForLoadMessage{};

        // For VR workaround.
        bool newGameStarted{};
    };
}
