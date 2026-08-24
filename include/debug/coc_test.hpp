#pragma once

#include "plugin.hpp"

namespace MCMMemory
{
    // A simple singleton class that let's me test the plugin without having to load a save/new game.
    class COCTest : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>
    {
    public:

        static COCTest* GetSingleton()
        {
            static COCTest singleton;
            return std::addressof(singleton);
        }

        bool Install();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* a_event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>* a_source) override;

    private:

        bool installed{};
    };
}
