// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/make_unique.h"

#include "core/settings.h"

#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "video_core/swrasterizer.h"
#ifdef VKENABLED
#include "common/emu_window.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#endif
#include "video_core/renderer_opengl/gl_rasterizer.h"

void RendererBase::RefreshRasterizerSetting() {
    bool hw_renderer_enabled = VideoCore::g_hw_renderer_enabled;
    if (rasterizer == nullptr || accelerated_rasterizer_active != hw_renderer_enabled) {
        accelerated_rasterizer_active = hw_renderer_enabled;

        if (hw_renderer_enabled) {
#ifdef VKENABLED
            if (VideoCore::g_emu_window->VulkanSupported()) {
                rasterizer = Common::make_unique<RasterizerVulkan>();
            }
            else {
                rasterizer = Common::make_unique<RasterizerOpenGL>();
            }
#else
            rasterizer = Common::make_unique<RasterizerOpenGL>();
#endif
        } else {
            rasterizer = Common::make_unique<VideoCore::SWRasterizer>();
        }
        rasterizer->InitObjects();
        rasterizer->Reset();
    }
}
