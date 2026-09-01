#pragma once

#include "utils/helper.hpp"

#include <spdlog/sinks/ostream_sink.h>

namespace MCMMemory::Logger
{
    inline void SetupLog()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            SKSE::stl::report_and_fail("Failed to find SKSE log directory");
        }

        *path /= std::format("{}.log", PRODUCT_NAME);
        static auto stream = std::make_shared<std::ofstream>(*path, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!stream->is_open()) {
            SKSE::stl::report_and_fail(std::format("Failed to open SKSE log file {}", ToUTF8(*path)));
        }

        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(*stream);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::trace);
        log->flush_on(spdlog::level::trace);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }
}
