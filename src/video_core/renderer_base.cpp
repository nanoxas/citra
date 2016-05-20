// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <memory>

#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "video_core/swrasterizer.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"

void RendererBase::RefreshRasterizerSetting() {
    VideoCore::Rasterizer r_backend = VideoCore::g_selected_rasterizer;
    if (rasterizer == nullptr || active_rasterizer != static_cast<int>(r_backend)) {
        active_rasterizer = static_cast<int>(r_backend);
        switch (r_backend) {
        case VideoCore::Rasterizer::VULKAN:
            rasterizer = std::make_unique<RasterizerVulkan>();
            break;
        case VideoCore::Rasterizer::OPENGL:
            rasterizer = std::make_unique<RasterizerOpenGL>();
            break;
        case VideoCore::Rasterizer::SOFTWARE:
        default:
            rasterizer = std::make_unique<VideoCore::SWRasterizer>();
            break;
        }
    }
}
