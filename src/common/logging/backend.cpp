// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>
#include <spdlog/spdlog.h>
#include "common/assert.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/string_util.h"

namespace Log {

/// Macro listing all log classes. Code should define CLS and SUB as desired before invoking this.
#define ALL_LOG_CLASSES()                                                                          \
    CLS(Log)                                                                                       \
    CLS(Common)                                                                                    \
    SUB(Common, Filesystem)                                                                        \
    SUB(Common, Memory)                                                                            \
    CLS(Core)                                                                                      \
    SUB(Core, ARM11)                                                                               \
    SUB(Core, Timing)                                                                              \
    CLS(Config)                                                                                    \
    CLS(Debug)                                                                                     \
    SUB(Debug, Emulated)                                                                           \
    SUB(Debug, GPU)                                                                                \
    SUB(Debug, Breakpoint)                                                                         \
    SUB(Debug, GDBStub)                                                                            \
    CLS(Kernel)                                                                                    \
    SUB(Kernel, SVC)                                                                               \
    CLS(Service)                                                                                   \
    SUB(Service, SRV)                                                                              \
    SUB(Service, FRD)                                                                              \
    SUB(Service, FS)                                                                               \
    SUB(Service, ERR)                                                                              \
    SUB(Service, APT)                                                                              \
    SUB(Service, BOSS)                                                                             \
    SUB(Service, GSP)                                                                              \
    SUB(Service, AC)                                                                               \
    SUB(Service, AM)                                                                               \
    SUB(Service, PTM)                                                                              \
    SUB(Service, LDR)                                                                              \
    SUB(Service, MIC)                                                                              \
    SUB(Service, NDM)                                                                              \
    SUB(Service, NFC)                                                                              \
    SUB(Service, NIM)                                                                              \
    SUB(Service, NWM)                                                                              \
    SUB(Service, CAM)                                                                              \
    SUB(Service, CECD)                                                                             \
    SUB(Service, CFG)                                                                              \
    SUB(Service, CSND)                                                                             \
    SUB(Service, DSP)                                                                              \
    SUB(Service, DLP)                                                                              \
    SUB(Service, HID)                                                                              \
    SUB(Service, HTTP)                                                                             \
    SUB(Service, SOC)                                                                              \
    SUB(Service, IR)                                                                               \
    SUB(Service, Y2R)                                                                              \
    CLS(HW)                                                                                        \
    SUB(HW, Memory)                                                                                \
    SUB(HW, LCD)                                                                                   \
    SUB(HW, GPU)                                                                                   \
    SUB(HW, AES)                                                                                   \
    CLS(Frontend)                                                                                  \
    CLS(Render)                                                                                    \
    SUB(Render, Software)                                                                          \
    SUB(Render, OpenGL)                                                                            \
    CLS(Audio)                                                                                     \
    SUB(Audio, DSP)                                                                                \
    SUB(Audio, Sink)                                                                               \
    CLS(Input)                                                                                     \
    CLS(Loader)

// GetClassName is a macro defined by Windows.h, grrr...
const char* GetLogClassName(Class log_class) {
    switch (log_class) {
#define CLS(x)                                                                                     \
    case Class::x:                                                                                 \
        return #x;
#define SUB(x, y)                                                                                  \
    case Class::x##_##y:                                                                           \
        return #x "." #y;
        ALL_LOG_CLASSES()
#undef CLS
#undef SUB
    case Class::Count:
        UNREACHABLE();
    }
}

const char* GetLevelName(Level log_level) {
#define LVL(x)                                                                                     \
    case Level::x:                                                                                 \
        return #x
    switch (log_level) {
        LVL(Trace);
        LVL(Debug);
        LVL(Info);
        LVL(Warning);
        LVL(Error);
        LVL(Critical);
    case Level::Count:
        UNREACHABLE();
    }
#undef LVL
}

class Filter;

class SpdLogBackend {
public:
    SpdLogBackend() {
        // set the log pattern to [HH:MM:SS.nano]
        spdlog::set_pattern("[%T.%F] %n <%I> %v");
        // Define the sinks to be passed to the loggers
        std::vector<spdlog::sink_ptr> sinks;

        // true means truncate file
        auto file_sink =
            std::make_shared<spdlog::sinks::simple_file_sink_mt>("citra_log.txt", true);
#ifdef _WIN32
        auto color_sink = std::make_shared<spdlog::sinks::wincolor_stderr_sink_mt>();
#else
        auto stderr_sink = spdlog::sinks::stderr_sink_mt::instance();
        auto color_sink = std::make_shared<spdlog::sinks::ansicolor_sink>(stderr_sink);
#endif
        sinks.push_back(file_sink);
        sinks.push_back(color_sink);

        // setup the loggers
        for (u8 i = 0; i < static_cast<u8>(Class::Count); ++i) {
            loggers[i] = std::make_shared<spdlog::logger>(GetLogClassName(static_cast<Class>(i)),
                                                          begin(sinks), end(sinks));
        }
    }
    ~SpdLogBackend() {
        spdlog::drop_all();
    }

    const std::shared_ptr<spdlog::logger> GetLogger(Class log_class) const {
        return loggers[static_cast<u8>(log_class)];
    }

private:
    std::array<std::shared_ptr<spdlog::logger>, (size_t)Class::Count> loggers;
};

static SpdLogBackend backend;

void SetFilter(const Filter& new_filter) {
    int i = 0;
    for (const auto level : new_filter) {
        auto logger = backend.GetLogger(static_cast<Class>(i++));
        switch (level) {
        case Level::Trace:
            logger->set_level(spdlog::level::trace);
            break;
        case Level::Debug:
            logger->set_level(spdlog::level::debug);
            break;
        case Level::Info:
            logger->set_level(spdlog::level::info);
            break;
        case Level::Warning:
            logger->set_level(spdlog::level::warn);
            break;
        case Level::Error:
            logger->set_level(spdlog::level::err);
            break;
        case Level::Critical:
            logger->set_level(spdlog::level::critical);
            break;
        default:
            UNREACHABLE();
            break;
        }
    }
}

void LogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_nr,
                const char* function, const char* format, ...) {
    auto logger = backend.GetLogger(log_class);
    va_list args;
    va_start(args, format);
    switch (log_level) {
    case Level::Trace:
        logger->trace(format, Common::TrimSourcePath(filename), function, line_nr, args);
        break;
    case Level::Debug:
        logger->debug(format, Common::TrimSourcePath(filename), function, line_nr, args);
        break;
    case Level::Info:
        logger->info(format, Common::TrimSourcePath(filename), function, line_nr, args);
        break;
    case Level::Warning:
        logger->warn(format, Common::TrimSourcePath(filename), function, line_nr, args);
        break;
    case Level::Error:
        logger->error(format, Common::TrimSourcePath(filename), function, line_nr, args);
        break;
    case Level::Critical:
        logger->critical(format, Common::TrimSourcePath(filename), function, line_nr, args);
        break;
    default:
        UNREACHABLE();
        break;
    }
    va_end(args);
}
}
