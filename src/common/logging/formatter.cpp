// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <string>
#include "common/assert.h"
#include "common/logging/formatter.h"

namespace Log {

static const char* GetLevelName(spdlog::level::level_enum log_level) {
    switch (log_level) {
    case spdlog::level::trace:
        return "Trace";
    case spdlog::level::debug:
        return "Debug";
    case spdlog::level::info:
        return "Info";
    case spdlog::level::warn:
        return "Warning";
    case spdlog::level::err:
        return "Error";
    case spdlog::level::critical:
        return "Critical";
    default:
        UNREACHABLE();
        return "UNREACHABLE";
    }
}

void Formatter::format(spdlog::details::log_msg& msg) {
    using std::chrono::steady_clock;
    using std::chrono::duration_cast;

    static const steady_clock::time_point time_origin = steady_clock::now();
    auto timestamp = duration_cast<std::chrono::microseconds>(steady_clock::now() - time_origin);

    const auto time_seconds = timestamp.count() / 1000000;
    const auto time_fractional = timestamp.count() % 1000000;

    msg.formatted << '[' << fmt::pad(time_seconds, 4, ' ') << '.'
                  << fmt::pad(time_fractional, 6, '0') << "] ";
    msg.formatted << *msg.logger_name << " <" << GetLevelName(msg.level) << "> ";

    msg.formatted << fmt::StringRef(msg.raw.data(), msg.raw.size());
    msg.formatted << '\n';
}

} // namespace Log