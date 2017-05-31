#include <spdlog/spdlog.h>

#include "common/assert.h"
#include "common/logging/backend.h"
#include "common/logging/backend_spdlog.h"
#include "common/logging/formatter.h"
#include "common/string_util.h"

namespace Log {

static spdlog::level::level_enum GetLevel(Log::Level log_level) {
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

SpdLogBackend& SpdLogBackend::Instance() {
    static SpdLogBackend instance;
    return instance;
}

SpdLogBackend::SpdLogBackend() {
    // setup the custom citra formatter
    spdlog::set_formatter(std::make_shared<Formatter>());

    std::vector<spdlog::sink_ptr> sinks;
    // Define the sinks to be passed to the loggers
    // true means truncate file
    auto file_sink = std::make_shared<spdlog::sinks::simple_file_sink_mt>("citra_log.txt", true);
#ifdef _WIN32
    auto color_sink = std::make_shared<spdlog::sinks::wincolor_stderr_sink_mt>();
#else
    auto stderr_sink = spdlog::sinks::stderr_sink_mt::instance();
    auto color_sink = std::make_shared<spdlog::sinks::ansicolor_sink>(stderr_sink);
#endif
    sinks.push_back(std::move(file_sink));
    sinks.push_back(std::move(color_sink));

    // register all of loggers with spdlog
    for (u8 log_class = 0; log_class != static_cast<u8>(Log::Class::Count); ++log_class) {
        spdlog::create(GetLogClassName(static_cast<Log::Class>(log_class)), begin(sinks),
                       end(sinks));
    }
}

SpdLogBackend::~SpdLogBackend() {
    spdlog::drop_all();
}

void SpdLogImpl(Class log_class, Level log_level, const char* file, int line_num,
                const char* function, const char* format, fmt::ArgList args) {
    SpdLogBackend::Instance();
    fmt::MemoryWriter formatting_buffer;
    formatting_buffer << Common::TrimSourcePath(file) << ':' << function << ':' << line_num << ": "
                      << format;
    auto& logger = spdlog::get(GetLogClassName(log_class));
    logger->log(GetLevel(log_level), formatting_buffer.c_str(), args);
}
}; // namespace Log
