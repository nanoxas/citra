// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <memory>
#include <vector>
#include <spdlog/spdlog.h>

#include "common/assert.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/backend_spdlog.h"
#include "common/logging/filter.h"
#include "common/logging/formatter.h"
#include "common/string_util.h"

namespace Log {

class SpdLogBackend {
public:
    SpdLogBackend();
    ~SpdLogBackend();

    static std::shared_ptr<SpdLogBackend> Instance();

    SpdLogBackend(SpdLogBackend const&) = delete;
    const SpdLogBackend& operator=(SpdLogBackend const&) = delete;

    using LogArray =
        std::array<std::shared_ptr<spdlog::logger>, static_cast<u8>(Log::Class::Count)>;
    const LogArray& GetLoggers() {
        return loggers;
    }

private:
    LogArray loggers;
};

static spdlog::level::level_enum GetSpdLogLevel(Log::Level log_level) {
    switch (log_level) {
    case Log::Level::Trace:
        return spdlog::level::trace;
    case Log::Level::Debug:
        return spdlog::level::debug;
    case Log::Level::Info:
        return spdlog::level::info;
    case Log::Level::Warning:
        return spdlog::level::warn;
    case Log::Level::Error:
        return spdlog::level::err;
    case Log::Level::Critical:
        return spdlog::level::critical;
    default:
        UNREACHABLE();
    }
}

std::shared_ptr<SpdLogBackend> SpdLogBackend::Instance() {
    static auto instance = std::make_shared<SpdLogBackend>();
    return instance;
}

SpdLogBackend::SpdLogBackend() {
    // setup the custom citra formatter
    spdlog::set_formatter(std::make_shared<Formatter>());

    // Define the sinks to be passed to the loggers
    // true means truncate file
    auto file_sink = std::make_shared<spdlog::sinks::simple_file_sink_mt>(
        FileUtil::GetUserPath(D_USER_IDX) + "citra_log.txt", true);
#ifdef _WIN32
    auto color_sink = std::make_shared<spdlog::sinks::wincolor_stderr_sink_mt>();
#else
    auto color_sink = std::make_shared<spdlog::sinks::ansicolor_stderr_sink_mt>();
#endif
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::move(file_sink));
    sinks.push_back(std::move(color_sink));

    // register all of loggers with spdlog
    for (ClassType log_class = 0; log_class != static_cast<ClassType>(Log::Class::Count);
         ++log_class) {
        loggers[log_class] = spdlog::create(GetLogClassName(static_cast<Log::Class>(log_class)),
                                            begin(sinks), end(sinks));
    }
}

SpdLogBackend::~SpdLogBackend() {
    spdlog::drop_all();
}

void SpdLogImpl(Class log_class, Level log_level, const char* file, int line_num,
                const char* function, const char* format, fmt::ArgList args) {
    auto logger = SpdLogBackend::Instance()->GetLoggers()[static_cast<u8>(log_class)];
    fmt::MemoryWriter formatting_buffer;
    formatting_buffer << Common::TrimSourcePath(file) << ':' << function << ':' << line_num << ": "
                      << format;
    logger->log(GetSpdLogLevel(log_level), formatting_buffer.c_str(), args);
}

void SpdLogSetFilter(Filter* filter) {
    auto loggers = SpdLogBackend::Instance()->GetLoggers();
    auto class_level = filter->GetClassLevel();
    for (ClassType log_class = 0; log_class != static_cast<ClassType>(Log::Class::Count);
         ++log_class) {
        loggers[log_class]->set_level(GetSpdLogLevel(class_level[log_class]));
    }
}
}; // namespace Log
