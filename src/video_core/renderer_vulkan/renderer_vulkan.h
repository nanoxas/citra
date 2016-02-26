// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <glad/glad.h>

#include "core/hw/gpu.h"

#include "video_core/renderer_base.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XCB_KHR
#endif
#include <vulkan/vulkan.h>

class EmuWindow;

#ifdef _DEBUG
#define DEBUG_INTEGRATION
#endif

class RendererVulkan : public RendererBase {
public:

    RendererVulkan();
    ~RendererVulkan() override;

    /// Swap buffers (render frame)
    void SwapBuffers() override;

    /**
     * Set the emulator window to use for renderer
     * @param window EmuWindow handle to emulator window to use for rendering
     */
    void SetWindow(EmuWindow* window) override;

    /// Initialize the renderer
    void Init() override;

    /// Shutdown the renderer
    void ShutDown() override;

private:
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR screen;
    VkSwapchainKHR swapchain;
    VkQueue queue;
#ifdef DEBUG_INTEGRATION
    bool hasDebugCallback = false;
    VkDebugReportCallbackEXT debugCallback;
#endif
    /// Structure used for storing information about the textures for each 3DS screen
    struct TextureInfo {
        VkSampler sampler;
        VkImage texture;
        VkImageLayout layout;
        VkDeviceMemory memory;
        VkImageView view;
    };

    void InitVulkanObjects();
    void DrawScreens();
    //void DrawSingleScreenRotated(const TextureInfo& texture, float x, float y, float w, float h);
    void UpdateFramerate();

    // Loads framebuffer from emulated memory into the active Vulkan texture.
    //void LoadFBToActiveVKTexture(const GPU::Regs::FramebufferConfig& framebuffer,
    //                             const TextureInfo& texture);
    // Fills active OpenGL texture with the given RGB color.
    //void LoadColorToActiveVKTexture(u8 color_r, u8 color_g, u8 color_b,
    //                               const TextureInfo& texture);

    short Initialized = 0;

    uint32_t publicHeapIndex;
    uint32_t privateHeapIndex;

    uint32_t familyIndex = ~0;

    EmuWindow*  render_window;                    ///< Handle to render window

    int resolution_width;                         ///< Current resolution width
    int resolution_height;                        ///< Current resolution height

    uint32_t currentBuffer;

    std::array<TextureInfo, 2> textures;          ///< Textures for top and bottom screens respectively
};
