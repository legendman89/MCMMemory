#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#ifdef GetObject
#undef GetObject
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
using namespace std::literals;
namespace logger = SKSE::log;

namespace MCMMemory
{
    inline std::atomic<bool> gameLoaded{};

    inline bool IsGameLoaded()
    {
        return gameLoaded.load(std::memory_order_relaxed);
    }

    inline void SetGameLoaded(bool a_loaded)
    {
        gameLoaded.store(a_loaded, std::memory_order_relaxed);
    }

    inline std::filesystem::path GetGameDataPath()
    {
        auto executable = std::filesystem::path(REL::Module::get().filename());
        return executable.parent_path() / "Data";
    }

    inline std::filesystem::path GetPluginDataPath()
    {
        return GetGameDataPath() / "SKSE" / "Plugins" / PRODUCT_NAME;
    }
}
