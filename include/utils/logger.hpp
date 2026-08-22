#pragma once

#include "utils/helper.hpp"

namespace MCMMemory::Logger
{
    inline void SetupLog()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            SKSE::stl::report_and_fail("Failed to find SKSE log directory");
        }

        *path /= std::format("{}.log", PRODUCT_NAME);
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(ToUTF8(*path), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::trace);
        log->flush_on(spdlog::level::trace);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }
}
