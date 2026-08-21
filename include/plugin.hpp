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
#include <spdlog/sinks/basic_file_sink.h>

using namespace std::literals;
namespace logger = SKSE::log;

namespace MCMMemory
{
    inline std::filesystem::path GetPluginDataPath()
    {
        auto executable = std::filesystem::path(REL::Module::get().filename());
        return executable.parent_path() / "Data" / "SKSE" / "Plugins" / PRODUCT_NAME;
    }
}
