// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "settings.h"

#include "audio_core/audio_core.h"

#include "core/gdbstub/gdbstub.h"

#include "video_core/video_core.h"

#include "common/emu_window.h"

namespace Settings {

Values values = {};

void Apply() {

    GDBStub::SetServerPort(static_cast<u32>(values.gdbstub_port));
    GDBStub::ToggleServer(values.use_gdbstub);

    VideoCore::g_hw_renderer_enabled = values.use_hw_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
    VideoCore::g_scaled_resolution_enabled = values.use_scaled_resolution;

    if (VideoCore::g_emu_window) {
        int width = VideoCore::g_emu_window->GetFramebufferLayout().width;
        int height = VideoCore::g_emu_window->GetFramebufferLayout().height;
        EmuWindow::FramebufferLayout layout;
        switch (Settings::values.layout_option) {
        case Settings::Layout::TopOnly:
            layout = EmuWindow::FramebufferLayout::TopOnlyLayout(width, height);
            break;
        case Settings::Layout::BottomOnly:
            layout = EmuWindow::FramebufferLayout::BotOnlyLayout(width, height);
            break;
        case Settings::Layout::BottomFirst:
            layout = EmuWindow::FramebufferLayout::BotFirstLayout(width, height);
            break;
        case Settings::Layout::Default:
        default:
            layout = EmuWindow::FramebufferLayout::DefaultScreenLayout(width, height);
            break;
        }
        VideoCore::g_emu_window->NotifyFramebufferLayoutChanged(layout);
    }

    AudioCore::SelectSink(values.sink_id);

}

} // namespace
