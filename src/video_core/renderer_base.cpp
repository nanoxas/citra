// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/color.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"
#include "video_core/frame_dumper.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/swrasterizer/swrasterizer.h"
#include "video_core/video_core.h"

RendererBase::RendererBase(EmuWindow& window)
    : render_window{window}, frame_dumpers{std::make_unique<FrameDumper>(),
                                           std::make_unique<FrameDumper>()} {
    FrameDumper::Initialize();
}

RendererBase::~RendererBase() = default;
void RendererBase::UpdateCurrentFramebufferLayout() {
    const Layout::FramebufferLayout& layout = render_window.GetFramebufferLayout();
    render_window.UpdateCurrentFramebufferLayout(layout.width, layout.height);
}

void RendererBase::RefreshRasterizerSetting() {
    bool hw_renderer_enabled = VideoCore::g_hw_renderer_enabled;
    if (rasterizer == nullptr || opengl_rasterizer_active != hw_renderer_enabled) {
        opengl_rasterizer_active = hw_renderer_enabled;

        if (hw_renderer_enabled) {
            rasterizer = std::make_unique<RasterizerOpenGL>(render_window);
        } else {
            rasterizer = std::make_unique<VideoCore::SWRasterizer>();
        }
    }
}

bool RendererBase::StartFrameDumping(const std::string& path_top, const std::string& path_bottom,
                                     const std::string& format) {
    if (!frame_dumpers[0]->StartDumping(
            path_top, format, Core::kScreenTopWidth * VideoCore::GetResolutionScaleFactor(),
            Core::kScreenTopHeight * VideoCore::GetResolutionScaleFactor())) {
        return false;
    }
    if (!frame_dumpers[1]->StartDumping(
            path_bottom, format, Core::kScreenBottomWidth * VideoCore::GetResolutionScaleFactor(),
            Core::kScreenBottomHeight * VideoCore::GetResolutionScaleFactor())) {
        return false;
    }
    start_dumping = true;
    return true;
}

void RendererBase::StopFrameDumping() {
    stop_dumping = true;
}

u32 RendererBase::GetColorFillForFramebuffer(int framebuffer_index) {
    // Main LCD (0): 0x1ED02204, Sub LCD (1): 0x1ED02A04
    u32 lcd_color_addr =
        (framebuffer_index == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
    lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
    LCD::Regs::ColorFill color_fill = {0};
    LCD::Read(color_fill.raw, lcd_color_addr);
    return color_fill.raw;
}
