// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/pica.h"
#include "video_core/rasterizer_interface.h"

class RasterizerVulkan : public VideoCore::RasterizerInterface {
public:

    RasterizerVulkan();
    ~RasterizerVulkan() override;

    /// Attempt to use a faster method to perform a display transfer
    virtual bool AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) { return false; }

    /// Attempt to use a faster method to fill a region
    virtual bool AccelerateFill(const GPU::Regs::MemoryFillConfig& config) { return false; }

    /// Attempt to use a faster method to display the framebuffer to screen
    virtual bool AccelerateDisplay(const GPU::Regs::FramebufferConfig& config, PAddr framebuffer_addr, u32 pixel_stride, ScreenInfo& screen_info) { return false; }

    void AddTriangle(const Pica::Shader::OutputVertex& v0,
        const Pica::Shader::OutputVertex& v1,
        const Pica::Shader::OutputVertex& v2) override;
    void DrawTriangles() override;
    void NotifyPicaRegisterChanged(u32 id) override;
    void FlushAll();
    void FlushRegion(PAddr addr, u32 size) override;
    void FlushAndInvalidateRegion(PAddr addr, u32 size) override;
private:
};