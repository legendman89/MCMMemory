#pragma once

#include "plugin.hpp"

#include "menu/hud.hpp"
#include "mcm/mcm_support.hpp"
#include "mcm/mcm_calls.hpp"
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
            SetGameLoaded(false);
            HUD::GetSingleton()->Reset();
        }

        inline void Start(bool a_autoRestoreAllowed, std::string_view a_reason)
        {
            waitingForLoadMessage = false;
            HUD::GetSingleton()->Reset();
            MCMRegistry::Reset();
            Backup::GetSingleton()->Reset();
            Capture::GetSingleton()->Reset();
            Restore::GetSingleton()->Reset(a_autoRestoreAllowed);
            MCMCallWatch::ResetSession();
            SetGameLoaded(true);
            logger::info("Game session started from {}; automatic restore allowed: {}", a_reason, a_autoRestoreAllowed);
        }

        inline bool IsWaitingForLoadMessage() const
        {
            return waitingForLoadMessage;
        }

    private:

        bool waitingForLoadMessage{};
    };
}
