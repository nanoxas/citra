// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <thread>

#include "common/logging/log.h"
#include "common/logging/text_formatter.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/scope_exit.h"

#include "core/settings.h"
#include "core/system.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/arm/gdb_stub.h"

#include "citra/config.h"
#include "citra/emu_window/emu_window_glfw.h"

#include "video_core/video_core.h"

/// Application entry point
int main(int argc, char **argv) {
    Log::Filter log_filter(Log::Level::Debug);
    Log::SetFilter(&log_filter);

    if (argc < 2) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return -1;
    }

    Config config;
    log_filter.ParseFilterString(Settings::values.log_filter);

    std::string boot_filename = argv[1];
    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

    VideoCore::g_hw_renderer_enabled = Settings::values.use_hw_renderer;

    System::Init(emu_window);

    Loader::ResultStatus load_result = Loader::LoadFile(boot_filename);
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Frontend, "Failed to load ROM (Error %i)!", load_result);
        return -1;
    }

    if (Settings::values.gdb_port > 0) {
        while (emu_window->IsOpen()) {
            if (GDB::IsActive()){
                Core::Debug();
            } else {
                Core::RunLoop();
            }
        }
    } else {
        while (emu_window->IsOpen()) {
            Core::RunLoop();
        }
    }

    System::Shutdown();

    delete emu_window;

    return 0;
}
