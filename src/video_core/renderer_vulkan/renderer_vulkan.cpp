// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "common/assert.h"
#include "common/emu_window.h"
#include "common/logging/log.h"
#include "common/profiler_reporting.h"

#include "core/memory.h"
#include "core/settings.h"
#include "core/hw/gpu.h"
#include "core/hw/hw.h"
#include "core/hw/lcd.h"

#include "video_core/video_core.h"
#include "video_core/debug_utils/debug_utils.h"

#include "video_core/renderer_opengl/gl_shader_util.h"

#include "renderer_vulkan.h"

#define INIT_INSTANCE 1
#define INIT_DEVICE 2
#define INIT_SURFACE 4
#define INIT_SWAPCHAIN 8
#define INIT_TOPSCREEN 16
#define INIT_BOTTOMSCREEN 32
#define INIT_MEMORYTOP 64
#define INIT_MEMORYBOTTOM 128
#define INIT_VIEWTOP 256
#define INIT_VIEWBOTTOM 512

static const char vertex_shader[] = R"(
#version 150 core

in vec2 vert_position;
in vec2 vert_tex_coord;
out vec2 frag_tex_coord;

// This is a truncated 3x3 matrix for 2D transformations:
// The upper-left 2x2 submatrix performs scaling/rotation/mirroring.
// The third column performs translation.
// The third row could be used for projection, which we don't need in 2D. It hence is assumed to
// implicitly be [0, 0, 1]
uniform mat3x2 modelview_matrix;

void main() {
    // Multiply input position by the rotscale part of the matrix and then manually translate by
    // the last column. This is equivalent to using a full 3x3 matrix and expanding the vector
    // to `vec3(vert_position.xy, 1.0)`
    gl_Position = vec4(mat2(modelview_matrix) * vert_position + modelview_matrix[2], 0.0, 1.0);
    frag_tex_coord = vert_tex_coord;
}
)";

static const char fragment_shader[] = R"(
#version 150 core

in vec2 frag_tex_coord;
out vec4 color;

uniform sampler2D color_texture;

void main() {
    color = texture(color_texture, frag_tex_coord);
}
)";

inline bool HasPicaFeatures(VkPhysicalDeviceFeatures * features){
    return features->geometryShader;//&& features->
}

/**
 * Defines a 1:1 pixel ortographic projection matrix with (0,0) on the top-left
 * corner and (width, height) on the lower-bottom.
 *
 * The projection part of the matrix is trivial, hence these operations are represented
 * by a 3x2 matrix.
 */
static std::array<GLfloat, 3 * 2> MakeOrthographicMatrix(const float width, const float height) {
    std::array<GLfloat, 3 * 2> matrix;

    matrix[0] = 2.f / width; matrix[2] = 0.f;           matrix[4] = -1.f;
    matrix[1] = 0.f;         matrix[3] = -2.f / height; matrix[5] = 1.f;
    // Last matrix row is implicitly assumed to be [0, 0, 1].

    return matrix;
}

RendererVulkan::RendererVulkan(){
    resolution_width  = std::max(VideoCore::kScreenTopWidth, VideoCore::kScreenBottomWidth);
    resolution_height = VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight;
}

void RendererVulkan::SetWindow(EmuWindow* window){
    this->render_window = window;
}

void RendererVulkan::Init(){
    VkApplicationInfo appinfo;
    appinfo.apiVersion = VK_API_VERSION;
    appinfo.applicationVersion = 0;
    appinfo.pApplicationName = "CTR";
    appinfo.pEngineName = "Citra Emulator";
    appinfo.pNext = VK_NULL_HANDLE;
    appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    uint32_t extensionCount;
    const char ** enabledExtensions = render_window->RequiredVulkanExtensions(&extensionCount);
    VkInstanceCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext = VK_NULL_HANDLE;
    info.ppEnabledLayerNames = VK_NULL_HANDLE;
    info.enabledLayerCount = 0;
    info.pApplicationInfo = &appinfo;
    info.enabledExtensionCount = extensionCount;
    info.ppEnabledExtensionNames = enabledExtensions;
    VkResult result = vkCreateInstance(&info, VK_NULL_HANDLE, &instance);

    if (result == VK_SUCCESS){
        this->Initialized |= INIT_INSTANCE;
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, VK_NULL_HANDLE);
        VkPhysicalDevice * devices = (VkPhysicalDevice*)malloc(deviceCount*sizeof(VkPhysicalDevice));
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
        VkPhysicalDeviceProperties deviceProps;
        VkPhysicalDeviceFeatures deviceFeatures;
        uint32_t lastAPIVersion = 0;
        VkPhysicalDevice device = VK_NULL_HANDLE;
        for (int i = 0; i < deviceCount; i++){
            vkGetPhysicalDeviceProperties(devices[i], &deviceProps);
            vkGetPhysicalDeviceFeatures(devices[i], &deviceFeatures);
            if (deviceProps.apiVersion > lastAPIVersion && HasPicaFeatures(&deviceFeatures)){
                device = devices[i];
            }
        }
        if (device != VK_NULL_HANDLE){
            VkDeviceCreateInfo deviceInfo;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceInfo.pEnabledFeatures = &deviceFeatures;
            deviceInfo.enabledExtensionCount = 0;
            deviceInfo.enabledLayerCount = 0;
            deviceInfo.queueCreateInfoCount = 0;
            deviceInfo.ppEnabledExtensionNames = VK_NULL_HANDLE;
            deviceInfo.ppEnabledLayerNames = VK_NULL_HANDLE;
            deviceInfo.pNext = VK_NULL_HANDLE;
            deviceInfo.pQueueCreateInfos = VK_NULL_HANDLE;
            if (vkCreateDevice(device, &deviceInfo, VK_NULL_HANDLE, &this->device) == VK_SUCCESS){
                this->Initialized |= INIT_DEVICE;
                surface = (VkSurfaceKHR)render_window->CreateVulkanSurface(instance);
                if (surface != NULL){
                    this->Initialized |= INIT_SURFACE;
                    VkSwapchainCreateInfoKHR swapchainInfo = {};
                    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                    swapchainInfo.imageFormat = VK_FORMAT_R8G8B8_UINT;
                    swapchainInfo.surface = surface;
                    swapchainInfo.minImageCount = 2;
                    swapchainInfo.clipped = true;
                    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
                    vkCreateSwapchainKHR(this->device, &swapchainInfo, VK_NULL_HANDLE, &swapchain);
                }
            }
        }
    }
}

void RendererVulkan::InitVulkanObjects(){
    if (this->Initialized & INIT_INSTANCE){
        VkImageCreateInfo imageTop = {};
        VkImageCreateInfo imageBottom = {};

        imageTop.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageTop.pNext = VK_NULL_HANDLE;
        imageTop.imageType = VK_IMAGE_TYPE_2D;
        imageTop.format = VK_FORMAT_R8G8B8_UINT;
        imageTop.extent = { VideoCore::kScreenTopHeight, VideoCore::kScreenTopWidth, 1 };
        imageTop.mipLevels = 1;
        imageTop.arrayLayers = 1;
        imageBottom.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageTop.samples = VK_SAMPLE_COUNT_1_BIT;
        imageTop.tiling = VK_IMAGE_TILING_OPTIMAL;

        imageBottom.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageBottom.pNext = VK_NULL_HANDLE;
        imageBottom.imageType = VK_IMAGE_TYPE_2D;
        imageBottom.format = VK_FORMAT_R8G8B8_UINT;
        imageBottom.extent = { VideoCore::kScreenBottomWidth, VideoCore::kScreenBottomHeight, 1 };
        imageBottom.mipLevels = 1;
        imageBottom.arrayLayers = 1;
        imageBottom.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageBottom.samples = VK_SAMPLE_COUNT_1_BIT;
        imageBottom.tiling = VK_IMAGE_TILING_OPTIMAL;

        VkResult result = vkCreateImage(device, &imageTop, VK_NULL_HANDLE, &textures[0].texture);
        bool success = true;
        if (result == VK_SUCCESS){
            this->Initialized |= INIT_TOPSCREEN;
        }else{
            success = false;
        }
        result = vkCreateImage(device, &imageBottom, VK_NULL_HANDLE, &textures[1].texture);
        if (result == VK_SUCCESS){
            this->Initialized |= INIT_BOTTOMSCREEN;
        }else{
            success = false;
        }
        if (success){
            VkMemoryAllocateInfo imageTopAllocation;
            VkMemoryRequirements imageTopMemoryReqs;
            VkMemoryAllocateInfo imageBottomAllocation;
            VkMemoryRequirements imageBottomMemoryReqs;
            vkGetImageMemoryRequirements(device, textures[0].texture, &imageTopMemoryReqs);
            vkGetImageMemoryRequirements(device, textures[1].texture, &imageBottomMemoryReqs);
            imageTopAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            imageTopAllocation.pNext = VK_NULL_HANDLE;
            imageTopAllocation.allocationSize = imageTopMemoryReqs.size;
            imageTopAllocation.memoryTypeIndex = 0; //TODO: figgure out what goes here.
            result = vkAllocateMemory(device,&imageTopAllocation,VK_NULL_HANDLE,&textures[0].memory);

            if (result == VK_SUCCESS){
                this->Initialized |= INIT_MEMORYTOP;
            }else{
                success = false;
            }

            imageBottomAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            imageBottomAllocation.pNext = VK_NULL_HANDLE;
            imageBottomAllocation.allocationSize = imageBottomMemoryReqs.size;
            imageBottomAllocation.memoryTypeIndex = 0; //TODO: figgure out what goes here.
            result = vkAllocateMemory(device,&imageBottomAllocation,VK_NULL_HANDLE,&textures[0].memory);

            if (result == VK_SUCCESS){
                this->Initialized |= INIT_MEMORYBOTTOM;
            }else{
                success = false;
            }

            if (success){
                VkImageViewCreateInfo viewTopInfo;
                VkImageViewCreateInfo viewBottomInfo;

                viewTopInfo.image = textures[0].texture;
                viewBottomInfo.image = textures[1].texture;

                viewTopInfo.format = VK_FORMAT_R8G8B8_UINT;
                viewBottomInfo.format = VK_FORMAT_R8G8B8_UINT;

                viewTopInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewBottomInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

                result = vkCreateImageView(device,&viewTopInfo,VK_NULL_HANDLE,&textures[0].view);
                if (result == VK_SUCCESS){
                    this->Initialized |= INIT_VIEWTOP;
                }else{
                    success = false;
                }
                result = vkCreateImageView(device,&viewBottomInfo,VK_NULL_HANDLE,&textures[1].view);
                if (result == VK_SUCCESS){
                    this->Initialized |= INIT_VIEWBOTTOM;
                }else{
                    success = false;
                }
            }
        }
    }
}

/// Swap buffers (render frame)
void RendererVulkan::SwapBuffers() {

    for (int i : {0, 1}) {
        const auto& framebuffer = GPU::g_regs.framebuffer_config[i];

        u32 lcd_color_addr = (i == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
        lcd_color_addr = HW::VADDR_LCD + 4 * lcd_color_addr;
        LCD::Regs::ColorFill color_fill = {0};
        LCD::Read(color_fill.raw, lcd_color_addr);
        if (color_fill.is_enabled) {
        }
        else{
        }
    }
}

/**
 * Draws the emulated screens to the emulator window.
 */
void RendererVulkan::DrawScreens() {
    auto layout = render_window->GetFramebufferLayout();
    DrawSingleScreenRotated(textures[0], (float)layout.top_screen.left, (float)layout.top_screen.top,
        (float)layout.top_screen.GetWidth(), (float)layout.top_screen.GetHeight());
    DrawSingleScreenRotated(textures[1], (float)layout.bottom_screen.left,(float)layout.bottom_screen.top,
        (float)layout.bottom_screen.GetWidth(), (float)layout.bottom_screen.GetHeight());

    m_current_frame++;
}

RendererVulkan::~RendererVulkan(){
    if (this->Initialized&INIT_VIEWBOTTOM)vkDestroyImageView(device, textures[1].view, VK_NULL_HANDLE);
    if (this->Initialized&INIT_VIEWTOP)vkDestroyImageView(device, textures[0].view, VK_NULL_HANDLE);
    if (this->Initialized&INIT_MEMORYBOTTOM)vkFreeMemory(this->device, this->textures[1].memory, VK_NULL_HANDLE);
    if (this->Initialized&INIT_MEMORYTOP)vkFreeMemory(this->device, this->textures[0].memory, VK_NULL_HANDLE);
    if (this->Initialized&INIT_BOTTOMSCREEN)vkDestroyImage(this->device,this->textures[1].texture,VK_NULL_HANDLE);
    if (this->Initialized&INIT_TOPSCREEN)vkDestroyImage(this->device,this->textures[0].texture,VK_NULL_HANDLE);
    //if (this->Initialized&INIT_SWAPCHAIN)vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
    if (this->Initialized&INIT_SURFACE)render_window->DestroyVulkanSurface(instance, surface);
    if (this->Initialized&INIT_DEVICE)vkDestroyDevice(this->device,VK_NULL_HANDLE);
    if (this->Initialized&INIT_INSTANCE)vkDestroyInstance(this->instance,VK_NULL_HANDLE);
}