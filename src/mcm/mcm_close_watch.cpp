#include "mcm/mcm_close_watch.hpp"
#include "mcm/mcm_script.hpp"
#include "settings.hpp"

namespace MCMMemory
{
    bool MCMCloseWatch::Install()
    {
        std::lock_guard lock(watchMutex);
        if (installed) {
            return true;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            logger::error("MCM close watch could not find the menu event source");
            return false;
        }

        ui->AddEventSink<RE::MenuOpenCloseEvent>(this);

        installed = true;

        logger::info("MCM close watch is successfully installed");

        return true;
    }

    bool MCMCloseWatch::IsClosing(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript)
    {
        std::lock_guard lock(watchMutex);
        if (!closingMCM || closingMCM->mcmScript.get() != a_mcmScript.get()) {
            return false;
        }

        const float elapsedSeconds = SecondsSince(journalClosedAt, std::chrono::steady_clock::now());
        if (!MCMScript(closingMCM->mcmScript).IsConfigOpen()) {
            logger::debug("'{}' finished closing in {:.1f} seconds after the Journal Menu closed", closingMCM->identity.modID, elapsedSeconds);
            closingMCM.reset();
            return false;
        }

        if (elapsedSeconds >= GetSettings().scriptCallTimeoutSeconds) {
            logger::warn("'{}' was still closing in {:.1f} seconds after the Journal Menu closed; backup and restore won't wait any longer", closingMCM->identity.modID, elapsedSeconds);
            closingMCM.reset();
            return false;
        }

        return true;
    }

    RE::BSEventNotifyControl MCMCloseWatch::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
    {
        if (!a_event || a_event->opening || std::string_view(a_event->menuName.c_str()) != RE::JournalMenu::MENU_NAME) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto activeMCM = MCMRegistry().ReadActiveMCM();
        if (!activeMCM || !MCMScript(activeMCM->mcmScript).IsConfigOpen()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        std::lock_guard lock(watchMutex);
        closingMCM = std::move(activeMCM);
        journalClosedAt = std::chrono::steady_clock::now();
        logger::debug("Waiting for '{}' to finish closing after the Journal Menu closed", closingMCM->identity.modID);
        
        return RE::BSEventNotifyControl::kContinue;
    }
}
