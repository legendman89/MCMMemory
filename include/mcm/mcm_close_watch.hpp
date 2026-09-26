#pragma once

#include "mcm/mcm_registry.hpp"
#include "utils/time.hpp"

namespace MCMMemory
{
    // SkyUI closes the MCM that was open in the journal from a Papyrus event, after the journal is already gone.
    // Its OnConfigClose may take seconds (tested on Smart Harvest7), and SkyUI frees buffers by the end of that
    // time, so backup and restore wait for that before they open that MCM.
    class MCMCloseWatch : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:

        static MCMCloseWatch* GetSingleton()
        {
            static MCMCloseWatch singleton;
            return std::addressof(singleton);
        }

        inline void Reset()
        {
            std::lock_guard lock(watchMutex);
            closingMCM.reset();
        }

        bool Install();

        // True while this MCM that left open by the journal is still closing (async call).
        bool IsClosing(const RE::BSTSmartPointer<RE::BSScript::Object>& a_mcmScript);

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_source) override;

    private:

        std::mutex watchMutex;

        std::optional<MCMRegistryEntry> closingMCM;

        TimePoint journalClosedAt{};

        bool installed{};
    };
}
