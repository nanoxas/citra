// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/logging/log.h"
#include "video_core/pica.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

EmuWindow* g_emu_window = nullptr; ///< Frontend emulator window
ShaderCompilationThread* g_shader_thread =
    nullptr;                              ///< Optional thread for compiling shaders asynchronously
std::unique_ptr<RendererBase> g_renderer; ///< Renderer plugin

std::atomic<bool> g_hw_renderer_enabled;
std::atomic<bool> g_shader_jit_enabled;
std::atomic<bool> g_hw_shader_enabled;
std::atomic<bool> g_hw_shader_accurate_gs;
std::atomic<bool> g_hw_shader_accurate_mul;

/// Initialize the video core
bool Init(EmuWindow* emu_window, ShaderCompilationThread* shader_thread) {
    Pica::Init();

    g_emu_window = emu_window;
    g_shader_thread = shader_thread;
    g_renderer = std::make_unique<RendererOpenGL>();
    g_renderer->SetWindow(g_emu_window);
    g_renderer->SetShaderCompilationThread(g_shader_thread);
    if (g_renderer->Init()) {
        LOG_DEBUG(Render, "initialized OK");
    } else {
        LOG_ERROR(Render, "initialization failed !");
        return false;
    }
    return true;
}

/// Shutdown the video core
void Shutdown() {
    Pica::Shutdown();

    g_renderer.reset();

    LOG_DEBUG(Render, "shutdown OK");
}

} // namespace VideoCore
