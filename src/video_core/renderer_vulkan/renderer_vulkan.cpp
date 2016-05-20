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

#include "video_core/renderer_vulkan/renderer_vulkan.h"

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
#define INIT_PRESENT_SEMAPHORE 1024
#define INIT_COMMAND_POOL 2048
#define INIT_SWAP_IMAGES 4096
#define INIT_RENDER_PASS 8192
#define INIT_VERTEX_BUFFERS 16384
#define INIT_INDEX_BUFFERS 32768
#define INIT_UNIFORM_BUFFERS 65536
#define INIT_FRAMEBUFFERS 131072
#define INIT_PIPELINE 262144
#define INIT_PIPELINE_CACHE 524288
#define INIT_SHADERS 1048576

#define INIT_ALL (INIT_INSTANCE|INIT_DEVICE|INIT_SURFACE|INIT_SWAPCHAIN|INIT_TOPSCREEN|\
                 INIT_BOTTOMSCREEN|INIT_MEMORYTOP|INIT_MEMORYBOTTOM|INIT_VIEWTOP|INIT_VIEWBOTTOM|\
                 INIT_PRESENT_SEMAPHORE|INIT_COMMAND_POOL|INIT_SWAP_IMAGES|INIT_RENDER_PASS|INIT_PIPELINE|\
                 INIT_VERTEX_BUFFERS|INIT_INDEX_BUFFERS|INIT_UNIFORM_BUFFERS|INIT_FRAMEBUFFERS|INIT_SHADERS)

#ifdef DEBUG_INTEGRATION
PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXTLocal;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXTLocal;
const char * vulkan_debug_msg_format = "Vulkan { %i } %s: %s";
static VkBool32 VKAPI_CALL logVulkanDebugMsgs(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
    switch (flags) {
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
        LOG_INFO(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
        break;
    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
        LOG_WARNING(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
        break;
    case VK_DEBUG_REPORT_WARNING_BIT_EXT:
        LOG_ERROR(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
        break;
    case VK_DEBUG_REPORT_ERROR_BIT_EXT:
        LOG_CRITICAL(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
        break;
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
        LOG_DEBUG(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
        break;
    default:
        if (flags > VK_DEBUG_REPORT_ERROR_BIT_EXT) {
            LOG_CRITICAL(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
            break;
        }else if (flags > VK_DEBUG_REPORT_WARNING_BIT_EXT) {
            LOG_ERROR(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
            break;
        }
        else if (flags > VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
            LOG_WARNING(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
            break;
        }
        else {
            LOG_INFO(Render_Vulkan, vulkan_debug_msg_format, messageCode, pLayerPrefix, pMessage);
            break;
        }
    }
    return VK_FALSE;
}
static const VkDebugReportCallbackCreateInfoEXT debugCallbackInfo = {
    VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
    VK_NULL_HANDLE,
    VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT,
    &logVulkanDebugMsgs,
    VK_NULL_HANDLE
};
#endif
#ifdef _DEBUG
#define WHITELIST_LENGTH 7 //8
const char * layerWhitelist[] = { "VK_LAYER_LUNARG_standard_validation", "VK_LAYER_LUNARG_param_checker", "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_draw_state", "VK_LAYER_LUNARG_mem_tracker", "VK_LAYER_LUNARG_image", "VK_LAYER_LUNARG_swapchain"/*, "VK_LAYER_RENDERDOC_Capture"*/ };
static bool CheckLayerWhitelist(char * layerName) {
    for (uint32_t i = 0; i < WHITELIST_LENGTH; i++) {
        if (strncmp(layerWhitelist[i], layerName, VK_MAX_EXTENSION_NAME_SIZE) == 0) {
            return true;
        }
    }
    return false;
}
#endif
static uint32_t clearColor[]{0, 0, 0, UINT32_MAX};
static const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,1 };
VkCommandBufferBeginInfo cmdBuffBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,VK_NULL_HANDLE,0,VK_NULL_HANDLE };
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
static std::array<float, 3 * 2> MakeOrthographicMatrix(const float width, const float height) {
    std::array<float, 3 * 2> matrix;

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

    std::vector<const char *> layers;

    VkInstanceCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext = VK_NULL_HANDLE;
#ifdef _DEBUG
    uint32_t layerCount = 0;
    info.enabledLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, VK_NULL_HANDLE);
    char * strings = (char *)malloc(VK_MAX_EXTENSION_NAME_SIZE*layerCount*sizeof(char));
    VkLayerProperties * layerProps = (VkLayerProperties *)malloc(layerCount*sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layerCount, layerProps);
    for (uint32_t i = 0; i < layerCount; i++) {
        if (CheckLayerWhitelist(layerProps[i].layerName)) {
            info.enabledLayerCount++;
            strcpy(&strings[VK_MAX_EXTENSION_NAME_SIZE*i], layerProps[i].layerName);
            layers.push_back(&strings[VK_MAX_EXTENSION_NAME_SIZE*i]);
        }
    }
    free(layerProps);
    info.ppEnabledLayerNames = layers.data();
#else
    info.ppEnabledLayerNames = VK_NULL_HANDLE;
    info.enabledLayerCount = 0;
#endif
    info.pApplicationInfo = &appinfo;
#ifdef DEBUG_INTEGRATION
    const char ** enabledExtensionsDebug = (const char **)malloc((extensionCount + 1)*sizeof(const char *));
    memcpy(enabledExtensionsDebug, enabledExtensions, extensionCount*sizeof(const char*));
    enabledExtensionsDebug[extensionCount] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
    info.enabledExtensionCount = extensionCount+1;
    info.ppEnabledExtensionNames = enabledExtensionsDebug;
#else
    info.enabledExtensionCount = extensionCount;
    info.ppEnabledExtensionNames = enabledExtensions;
#endif
    for (uint32_t i = 0; i < info.enabledLayerCount; i++) {
        LOG_INFO(Render_Vulkan, "Using Layer: %s", info.ppEnabledLayerNames[i]);
    }
    for (uint32_t i = 0; i < info.enabledExtensionCount; i++) {
        LOG_INFO(Render_Vulkan, "Using Extension: %s", info.ppEnabledExtensionNames[i]);
    }
    VkResult result = vkCreateInstance(&info, VK_NULL_HANDLE, &instance);
#ifdef DEBUG_INTEGRATION
    free(enabledExtensionsDebug);
#endif
#ifdef _DEBUG
    free(strings);
    layers.clear();
#endif
    if (result == VK_SUCCESS){
        LOG_INFO(Render_Vulkan, "Created Vulkan Instance.");
#ifdef DEBUG_INTEGRATION
        vkCreateDebugReportCallbackEXTLocal = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
        vkDestroyDebugReportCallbackEXTLocal = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
        hasDebugCallback = vkCreateDebugReportCallbackEXTLocal(instance, &debugCallbackInfo, VK_NULL_HANDLE, &debugCallback) == VK_SUCCESS;
#endif
        this->Initialized |= INIT_INSTANCE;
        screen = (VkSurfaceKHR)render_window->CreateVulkanSurface(instance);
        if (screen != NULL) {
            LOG_INFO(Render_Vulkan, "Created Vulkan Surface.");
            this->Initialized |= INIT_SURFACE;
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, VK_NULL_HANDLE);
            VkPhysicalDevice * devices = (VkPhysicalDevice*)malloc(deviceCount*sizeof(VkPhysicalDevice));
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
            VkPhysicalDeviceProperties deviceProps;
            VkPhysicalDeviceFeatures deviceFeatures;
            VkPhysicalDeviceMemoryProperties deviceMemoryProps;
            uint32_t lastAPIVersion = 0;
            VkPhysicalDevice device = VK_NULL_HANDLE;
            for (uint32_t i = 0; i < deviceCount; i++) {
                vkGetPhysicalDeviceProperties(devices[i], &deviceProps);
                vkGetPhysicalDeviceFeatures(devices[i], &deviceFeatures);
                if (deviceProps.apiVersion > lastAPIVersion && HasPicaFeatures(&deviceFeatures)) {
                    device = devices[i];
                }
            }
            free(devices);
            size_t privateHeapSize = 0;
            size_t publicHeapSize = 0;
            if (device != VK_NULL_HANDLE) {
                LOG_INFO(Render_Vulkan, "Found Compatable Vulkan Device.");
                uint32_t familyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, VK_NULL_HANDLE);
                VkQueueFamilyProperties * familyProps = (VkQueueFamilyProperties *)malloc(familyCount*sizeof(VkQueueFamilyProperties));
                vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, familyProps);
                VkBool32 supportsPresent = VK_FALSE;
                for (uint32_t i = 0; i < familyCount; i++) {
                    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, screen, &supportsPresent);
                    if (familyProps[i].queueFlags&VK_QUEUE_GRAPHICS_BIT&&familyProps[i].queueFlags&VK_QUEUE_TRANSFER_BIT&&supportsPresent) {
                        familyIndex = i;
                        break;
                    }
                }
                free(familyProps);
                VkDeviceCreateInfo deviceInfo;
                VkDeviceQueueCreateInfo queueInfo;
                vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
                vkGetPhysicalDeviceMemoryProperties(device, &deviceMemoryProps);
                for (uint32_t i = 0; i < deviceMemoryProps.memoryTypeCount; i++) {
                    VkMemoryType type = deviceMemoryProps.memoryTypes[i];
                    if (type.propertyFlags == 0 && deviceMemoryProps.memoryHeaps[type.heapIndex].size > privateHeapSize) {
                        privateHeapIndex = type.heapIndex;
                        privateHeapSize = deviceMemoryProps.memoryHeaps[type.heapIndex].size;
                    }
                    if (type.propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && type.propertyFlags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT &&  type.propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT && deviceMemoryProps.memoryHeaps[type.heapIndex].size > publicHeapSize) {
                        publicHeapIndex = type.heapIndex;
                        publicHeapSize = deviceMemoryProps.memoryHeaps[type.heapIndex].size;
                    }
                }
                const float priority = 1.f;
                const char * swapchain_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfo.pNext = VK_NULL_HANDLE;
                queueInfo.flags = 0;
                queueInfo.queueCount = 1;
                queueInfo.pQueuePriorities = &priority;
                queueInfo.queueFamilyIndex = familyIndex;
                deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                deviceInfo.pEnabledFeatures = &deviceFeatures;
                deviceInfo.enabledExtensionCount = 1;
#ifdef _DEBUG
                deviceInfo.enabledLayerCount = 0;
                vkEnumerateDeviceLayerProperties(device, &layerCount, VK_NULL_HANDLE);
                strings = (char *)malloc(VK_MAX_EXTENSION_NAME_SIZE*layerCount*sizeof(char));
                layerProps = (VkLayerProperties *)malloc(layerCount*sizeof(VkLayerProperties));
                vkEnumerateDeviceLayerProperties(device, &layerCount, layerProps);
                for (uint32_t i = 0; i < layerCount; i++) {
                    if (CheckLayerWhitelist(layerProps[i].layerName)) {
                        info.enabledLayerCount++;
                        strcpy(&strings[VK_MAX_EXTENSION_NAME_SIZE*i], layerProps[i].layerName);
                        layers.push_back(&strings[VK_MAX_EXTENSION_NAME_SIZE*i]);
                    }
                }
                free(layerProps);
                deviceInfo.ppEnabledLayerNames = layers.data();
#else
                deviceInfo.enabledLayerCount = 0;
                deviceInfo.ppEnabledLayerNames = VK_NULL_HANDLE;
#endif
                deviceInfo.queueCreateInfoCount = 1;
                deviceInfo.ppEnabledExtensionNames = &swapchain_ext;
                deviceInfo.pNext = VK_NULL_HANDLE;
                deviceInfo.pQueueCreateInfos = &queueInfo;
                deviceInfo.flags = 0;
                result = vkCreateDevice(device, &deviceInfo, VK_NULL_HANDLE, &this->device);
#ifdef _DEBUG
                free(strings);
                layers.clear();
#endif
                if (result == VK_SUCCESS) {
                    LOG_INFO(Render_Vulkan, "Initialized Vulkan Device.");
                    this->Initialized |= INIT_DEVICE;
                    VkSwapchainCreateInfoKHR swapchainInfo = {};
                    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
                    swapchainInfo.imageFormat = VK_FORMAT_R8G8B8A8_UINT;
                    swapchainInfo.surface = screen;
                    swapchainInfo.minImageCount = 2;
                    swapchainInfo.clipped = true;
                    swapchainInfo.queueFamilyIndexCount = 1;
                    swapchainInfo.pQueueFamilyIndices = &familyIndex;
                    swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
                    swapchainInfo.imageExtent = { static_cast<uint32_t>(resolution_width), static_cast<uint32_t>(resolution_height) };
                    swapchainInfo.imageArrayLayers = 1;
                    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
                    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
                    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
                    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
                    if (vkCreateSwapchainKHR(this->device, &swapchainInfo, VK_NULL_HANDLE, &swapchain) == VK_SUCCESS) {
                        this->Initialized |= INIT_SWAPCHAIN;
                        LOG_INFO(Render_Vulkan, "Initialized Swapchain for display surface.");
                        vkGetDeviceQueue(this->device, familyIndex, 0, &queue);
                        LOG_INFO(Render_Vulkan, "Initialized Device Queue.");
                    }
                }
            }
        }
    }
    InitVulkanObjects();
}

void RendererVulkan::InitVulkanObjects(){
    if (this->Initialized & INIT_INSTANCE){
        if (createVertexBuffers()) {
            this->Initialized |= INIT_VERTEX_BUFFERS;
        }
        if (createIndexBuffers()) {
            this->Initialized |= INIT_INDEX_BUFFERS;
        }
        if (createUniformBuffers()) {
            this->Initialized |= INIT_UNIFORM_BUFFERS;
        }
        VkImageCreateInfo imageTop = {};
        VkImageCreateInfo imageBottom = {};

        imageTop.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageTop.pNext = VK_NULL_HANDLE;
        imageTop.imageType = VK_IMAGE_TYPE_2D;
        imageTop.format = VK_FORMAT_R8G8B8A8_UINT;
        imageTop.extent = { VideoCore::kScreenTopHeight, VideoCore::kScreenTopWidth, 1 };
        imageTop.mipLevels = 1;
        imageTop.arrayLayers = 1;
        imageBottom.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        imageTop.samples = VK_SAMPLE_COUNT_1_BIT;
        imageTop.tiling = VK_IMAGE_TILING_OPTIMAL;

        imageBottom.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageBottom.pNext = VK_NULL_HANDLE;
        imageBottom.imageType = VK_IMAGE_TYPE_2D;
        imageBottom.format = VK_FORMAT_R8G8B8A8_UINT;
        imageBottom.extent = { VideoCore::kScreenBottomHeight, VideoCore::kScreenBottomWidth, 1 };
        imageBottom.mipLevels = 1;
        imageBottom.arrayLayers = 1;
        imageBottom.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        imageBottom.samples = VK_SAMPLE_COUNT_1_BIT;
        imageBottom.tiling = VK_IMAGE_TILING_OPTIMAL;

        VkResult result = vkCreateImage(device, &imageTop, VK_NULL_HANDLE, &(textures[0].texture));
        bool success = true;
        if (result == VK_SUCCESS){
            LOG_INFO(Render_Vulkan, "Created Vulkan Image for top screen.");
            this->Initialized |= INIT_TOPSCREEN;
        }else{
            success = false;
        }
        result = vkCreateImage(device, &imageBottom, VK_NULL_HANDLE, &(textures[1].texture));
        if (result == VK_SUCCESS){
            LOG_INFO(Render_Vulkan, "Created Vulkan Image for bottom screen.");
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
            imageTopAllocation.memoryTypeIndex = publicHeapIndex;
            result = vkAllocateMemory(device,&imageTopAllocation,VK_NULL_HANDLE,&textures[0].memory);

            if (result == VK_SUCCESS){
                this->Initialized |= INIT_MEMORYTOP;
                vkBindImageMemory(device, textures[0].texture, textures[0].memory,0);
                LOG_INFO(Render_Vulkan, "Allocated and bound top screen memory.");
            }else{
                success = false;
            }

            imageBottomAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            imageBottomAllocation.pNext = VK_NULL_HANDLE;
            imageBottomAllocation.allocationSize = imageBottomMemoryReqs.size;
            imageBottomAllocation.memoryTypeIndex = publicHeapIndex;
            result = vkAllocateMemory(device,&imageBottomAllocation,VK_NULL_HANDLE,&textures[1].memory);

            if (result == VK_SUCCESS){
                this->Initialized |= INIT_MEMORYBOTTOM;
                vkBindImageMemory(device, textures[1].texture, textures[1].memory, 0);
                LOG_INFO(Render_Vulkan, "Allocated and bound bottom screen memory.");
            }else{
                success = false;
            }

            if (success){
                VkImageViewCreateInfo viewTopInfo = {};
                VkComponentMapping mapping = {};
                VkImageViewCreateInfo viewBottomInfo = {};

                mapping.r = VK_COMPONENT_SWIZZLE_B;
                mapping.g = VK_COMPONENT_SWIZZLE_G;
                mapping.b = VK_COMPONENT_SWIZZLE_R;
                mapping.a = VK_COMPONENT_SWIZZLE_ONE;


                viewTopInfo.components = mapping;
                viewBottomInfo.components = mapping;

                viewTopInfo.image = textures[0].texture;
                viewBottomInfo.image = textures[1].texture;

                viewTopInfo.format = VK_FORMAT_R8G8B8A8_UINT;
                viewBottomInfo.format = VK_FORMAT_R8G8B8A8_UINT;

                viewTopInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewBottomInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

                viewTopInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewBottomInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

                viewTopInfo.subresourceRange = range;
                viewBottomInfo.subresourceRange = range;

                result = vkCreateImageView(device,&viewTopInfo,VK_NULL_HANDLE,&textures[0].view);
                if (result == VK_SUCCESS){
                    this->Initialized |= INIT_VIEWTOP;
                    LOG_INFO(Render_Vulkan, "Created Top screen image view.");
                }else{
                    success = false;
                }
                result = vkCreateImageView(device,&viewBottomInfo,VK_NULL_HANDLE,&textures[1].view);
                if (result == VK_SUCCESS){
                    this->Initialized |= INIT_VIEWBOTTOM;
                    LOG_INFO(Render_Vulkan, "Created Bottom screen image view.");
                }else{
                    success = false;
                }

                if (success) {
                    VkSemaphoreCreateInfo semaphoreInfo = {};
                    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                    if (vkCreateSemaphore(device, &semaphoreInfo, VK_NULL_HANDLE, &presentSemaphore) == VK_SUCCESS) {
                        this->Initialized |= INIT_PRESENT_SEMAPHORE;
                        LOG_INFO(Render_Vulkan, "Created Semaphore.");
                        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentSemaphore, (VkFence)nullptr, &currentBuffer);
                        VkCommandPoolCreateInfo commandPoolInfo = {};
                        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                        commandPoolInfo.queueFamilyIndex = familyIndex;
                        if (vkCreateCommandPool(device, &commandPoolInfo, VK_NULL_HANDLE, &commandPool) == VK_SUCCESS) {
                            this->Initialized |= INIT_COMMAND_POOL;
                            LOG_INFO(Render_Vulkan, "Created Command Pool.");
                            VkCommandBufferAllocateInfo commandBufferInfo = {};
                            commandBufferInfo.commandPool = commandPool;
                            commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                            commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                            commandBufferInfo.commandBufferCount = 3;
                            vkAllocateCommandBuffers(device, &commandBufferInfo, commandBuffers);
                            vkGetSwapchainImagesKHR(device, swapchain, &swapImageCount, VK_NULL_HANDLE);
                            swapImages = reinterpret_cast<VkImage *>(malloc(swapImageCount*sizeof(VkImage)));
                            swapImageViews = reinterpret_cast<VkImageView *>(malloc(swapImageCount*sizeof(VkImageView)));
                            if (vkGetSwapchainImagesKHR(device,swapchain,&swapImageCount,swapImages)== VK_SUCCESS) {
                                this->Initialized |= INIT_SWAP_IMAGES;
                                LOG_INFO(Render_Vulkan, "Fetched swapchain.");
                                VkImageViewCreateInfo createInfo = {};
                                createInfo.pNext = NULL;
                                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                                createInfo.components = mapping;
                                createInfo.format = VK_FORMAT_R8G8B8A8_UINT;
                                createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                                createInfo.subresourceRange = range;
                                for (uint32_t i = 0; i < swapImageCount; i++) {
                                    createInfo.image = swapImages[i];
                                    vkCreateImageView(device, &createInfo, VK_NULL_HANDLE, &swapImageViews[i]);
                                }
                                if (createRenderPass()) {
                                    this->Initialized |= INIT_RENDER_PASS;
                                    if (createFrameBuffers()) {
                                        this->Initialized |= INIT_FRAMEBUFFERS;
                                        if (createPipelineCache()) {
                                            this->Initialized |= INIT_PIPELINE_CACHE;
                                            if (createPipeline()) {
                                                this->Initialized |= INIT_PIPELINE;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    RefreshRasterizerSetting();
    RefreshDelegate();

}

void RendererVulkan::RefreshDelegate() {
    rendererDelegate.screens[0] = textures[0];
    rendererDelegate.screens[1] = textures[1];
    rendererDelegate.device = device;
    rendererDelegate.instance = instance;
    if (rasterizer.get() != lastRasterizer&&rasterizer->SupportsRendererDelegate()) {
        lastRasterizer = rasterizer.get();
        rasterizer->SetRendererDelegate(&rendererDelegate);
    }
}

bool RendererVulkan::createVertexBuffers()
{
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.pNext = VK_NULL_HANDLE;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    return vkCreateBuffer(device, &info, VK_NULL_HANDLE, &vertexBuffer) == VK_SUCCESS;
}

bool RendererVulkan::createIndexBuffers()
{
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.pNext = VK_NULL_HANDLE;
    info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    return vkCreateBuffer(device, &info, VK_NULL_HANDLE, &indexBuffer) == VK_SUCCESS;
}

bool RendererVulkan::createUniformBuffers()
{
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.pNext = VK_NULL_HANDLE;
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    return vkCreateBuffer(device, &info, VK_NULL_HANDLE, &uniformBuffer) == VK_SUCCESS;
}

bool RendererVulkan::createPipeline()
{
    VkDescriptorSetLayoutCreateInfo descriptorInfo = {};

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device, &descriptorInfo, VK_NULL_HANDLE, &layout) == VK_SUCCESS) {
        bool result = true;
        VkPipelineLayoutCreateInfo infoLayout = {};
        infoLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        infoLayout.flags = 0;
        infoLayout.pNext = VK_NULL_HANDLE;
        infoLayout.pSetLayouts = &layout;
        infoLayout.setLayoutCount = 1;
        infoLayout.pSetLayouts = &layout;
        //TODO: infoLayout.pPushConstantRanges
        //TODO: infoLayout.pushConstantRangeCount
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(device, &infoLayout, VK_NULL_HANDLE, &pipelineLayout) == VK_SUCCESS) {
            VkPipelineViewportStateCreateInfo viewportInfo = {};
            viewportInfo.viewportCount = 1;
            viewportInfo.pViewports = &viewport;
            viewportInfo.scissorCount = 0;
            viewportInfo.pScissors = VK_NULL_HANDLE;
            VkPipelineVertexInputStateCreateInfo vertexInfo = {};
            //TODO: vertex input state info
            VkPipelineTessellationStateCreateInfo tesselationInfo = {};
            tesselationInfo.flags = 0;
            VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
            rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
            rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizationInfo.depthBiasEnable = VK_FALSE;
            rasterizationInfo.depthClampEnable = VK_FALSE;
            rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
            VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
            multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampleInfo.pNext = VK_NULL_HANDLE;
            multisampleInfo.sampleShadingEnable = VK_FALSE;
            multisampleInfo.alphaToOneEnable = VK_FALSE;
            multisampleInfo.alphaToCoverageEnable = VK_FALSE;
            multisampleInfo.pSampleMask = VK_NULL_HANDLE;
            multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDynamicStateCreateInfo dynamicInfo = {};
            dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicInfo.pNext = VK_NULL_HANDLE;
            dynamicInfo.flags = 0;
            dynamicInfo.dynamicStateCount = 0;
            dynamicInfo.pDynamicStates = VK_NULL_HANDLE;
            VkPipelineDepthStencilStateCreateInfo depthInfo = {};
            depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthInfo.pNext = VK_NULL_HANDLE;
            depthInfo.depthTestEnable = VK_FALSE;
            depthInfo.depthWriteEnable = VK_FALSE;
            depthInfo.stencilTestEnable = VK_FALSE;
            depthInfo.maxDepthBounds = 1;
            depthInfo.minDepthBounds = 0;
            VkPipelineColorBlendAttachmentState attachmentState = {};
            attachmentState.alphaBlendOp = VK_BLEND_OP_MAX;
            attachmentState.colorBlendOp = VK_BLEND_OP_ADD;
            attachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
            attachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
            attachmentState.blendEnable = VK_FALSE;
            VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
            colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlendInfo.pNext = VK_NULL_HANDLE;
            colorBlendInfo.blendConstants[0] = 0.5;
            colorBlendInfo.blendConstants[1] = 0.5;
            colorBlendInfo.blendConstants[2] = 0.5;
            colorBlendInfo.blendConstants[3] = 1;
            colorBlendInfo.logicOpEnable = false;
            colorBlendInfo.attachmentCount = 1;
            colorBlendInfo.pAttachments = &attachmentState;
            VkPipelineInputAssemblyStateCreateInfo inputAsmInfo = {};
            //TODO: input assembly info
            VkPipelineShaderStageCreateInfo stageInfo = {};
            //TODO: shader stage info
            VkGraphicsPipelineCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            info.flags = 0;
            info.pNext = VK_NULL_HANDLE;
            info.layout = pipelineLayout;
            info.basePipelineIndex = 0;
            info.basePipelineHandle = VK_NULL_HANDLE;
            info.renderPass = renderPass;
            info.pColorBlendState = &colorBlendInfo;
            info.pDepthStencilState = &depthInfo;
            info.pDynamicState = &dynamicInfo;
            info.pMultisampleState = &multisampleInfo;
            info.pRasterizationState = &rasterizationInfo;
            info.pTessellationState = &tesselationInfo;
            info.pVertexInputState = &vertexInfo;
            info.pViewportState = &viewportInfo;
            info.pInputAssemblyState = &inputAsmInfo;
            info.pStages = &stageInfo;
            if (vkCreateGraphicsPipelines(device,pipelineCache,1,&info,VK_NULL_HANDLE,&pipeline)!=VK_SUCCESS) {
                result = false;
            }
            vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);
            result = false;
        }
        vkDestroyDescriptorSetLayout(device, layout, VK_NULL_HANDLE);
        return result;
    }else return false;
}

bool RendererVulkan::createPipelineCache()
{
    VkPipelineCacheCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.flags = 0;
    info.pNext = VK_NULL_HANDLE;
    info.initialDataSize = 0;
    info.pInitialData = VK_NULL_HANDLE;
    return vkCreatePipelineCache(device,&info,VK_NULL_HANDLE,&pipelineCache);
}

bool RendererVulkan::createRenderPass()
{
    VkAttachmentDescription description = {};
    description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    description.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.format = VK_FORMAT_R8G8B8A8_UINT;
    description.samples = VK_SAMPLE_COUNT_1_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext = VK_NULL_HANDLE;
    info.flags = 0;
    info.attachmentCount = 1;
    info.dependencyCount = 0;
    info.pAttachments = &description;
    info.pDependencies = VK_NULL_HANDLE;
    return vkCreateRenderPass(device,&info,VK_NULL_HANDLE,&renderPass) == VK_SUCCESS;
}

bool RendererVulkan::createFrameBuffers()
{
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext = VK_NULL_HANDLE;
    info.flags = 0;
    info.width = resolution_width;
    info.height = resolution_height;
    info.renderPass = renderPass;
    info.layers = 1;
    info.attachmentCount = 1;
    swapImageFramebuffers = reinterpret_cast<VkFramebuffer *>(malloc(swapImageCount*sizeof(VkFramebuffer)));
    bool result = true;
    for (uint32_t i = 0; i < swapImageCount; i++) {
        info.pAttachments = &swapImageViews[i];
        result = result && vkCreateFramebuffer(device, &info, VK_NULL_HANDLE, &swapImageFramebuffers[i]) == VK_SUCCESS;
    }
    return result;
}

/// Swap buffers (render frame)
void RendererVulkan::SwapBuffers() {
    if (this->Initialized == INIT_ALL) {
        for (int i : {0, 1}) {
            vkBeginCommandBuffer(commandBuffers[i], &cmdBuffBeginInfo);
            const auto& framebuffer = GPU::g_regs.framebuffer_config[i];
            u32 lcd_color_addr = (i == 0) ? LCD_REG_INDEX(color_fill_top) : LCD_REG_INDEX(color_fill_bottom);
            lcd_color_addr = static_cast<u32>(HW::VADDR_LCD + sizeof(u32) * lcd_color_addr);
            LCD::Regs::ColorFill color_fill = { 0 };
            LCD::Read(color_fill.raw, lcd_color_addr);
            if (color_fill.is_enabled) {
                VkClearColorValue clearValue = {};
                clearValue.uint32[0] = color_fill.color_r;
                clearValue.uint32[1] = color_fill.color_g;
                clearValue.uint32[2] = color_fill.color_b;
                clearValue.uint32[3] = UINT32_MAX;
                vkCmdClearColorImage(commandBuffers[i], textures[i].texture, textures[i].layout,&clearValue, 1, &range);
            }
            else {
            }
            vkEndCommandBuffer(commandBuffers[i]);
        }

        DrawScreens();
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = VK_NULL_HANDLE;
        submitInfo.commandBufferCount = 3;
        submitInfo.pCommandBuffers = commandBuffers;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.waitSemaphoreCount = 0;
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

        auto& profiler = Common::Profiling::GetProfilingManager();
        profiler.FinishFrame();
        {
            auto aggregator = Common::Profiling::GetTimingResultsAggregator();
            aggregator->AddFrame(profiler.GetPreviousFrameResults());
        }

        // Swap buffers
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = NULL;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &currentBuffer;
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentSemaphore, (VkFence)nullptr, &currentBuffer);
        vkQueuePresentKHR(queue, &presentInfo);
        render_window->PollEvents();

        profiler.BeginFrame();

        RefreshRasterizerSetting();
        RefreshDelegate();

        if (Pica::g_debug_context && Pica::g_debug_context->recorder) {
            Pica::g_debug_context->recorder->FrameFinished();
        }
    }
}

void RendererVulkan::DrawSingleScreenRotated(VkCommandBuffer buff,const VkTextureInfo& texture, float left,float top,float width,float height) {

}

/**
 * Draws the emulated screens to the emulator window.
 */
void RendererVulkan::DrawScreens() {
    vkBeginCommandBuffer(commandBuffers[2], &cmdBuffBeginInfo);
    clearColor[0] = static_cast<uint32_t>(Settings::values.bg_red*UINT32_MAX);
    clearColor[1] = static_cast<uint32_t>(Settings::values.bg_green*UINT32_MAX);
    clearColor[2] = static_cast<uint32_t>(Settings::values.bg_blue*UINT32_MAX);
    vkCmdClearColorImage(commandBuffers[2],swapImages[currentBuffer],VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,reinterpret_cast<const VkClearColorValue *>(clearColor),1,&range);
    //VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, VK_NULL_HANDLE};
    auto layout = render_window->GetFramebufferLayout();
    DrawSingleScreenRotated(commandBuffers[2],textures[0], (float)layout.top_screen.left, (float)layout.top_screen.top,
        (float)layout.top_screen.GetWidth(), (float)layout.top_screen.GetHeight());
    DrawSingleScreenRotated(commandBuffers[2],textures[1], (float)layout.bottom_screen.left,(float)layout.bottom_screen.top,
        (float)layout.bottom_screen.GetWidth(), (float)layout.bottom_screen.GetHeight());

    m_current_frame++;
    vkEndCommandBuffer(commandBuffers[2]);
}

void RendererVulkan::UpdateFramerate()
{

}

void RendererVulkan::ShutDown()
{
    if (this->Initialized&INIT_PIPELINE)vkDestroyPipeline(device, pipeline, VK_NULL_HANDLE);
    if (this->Initialized&INIT_PIPELINE_CACHE)vkDestroyPipelineCache(device, pipelineCache, VK_NULL_HANDLE);
    if (this->Initialized&INIT_FRAMEBUFFERS) {
        for (uint32_t i = swapImageCount; i > 0; i--) {
            vkDestroyFramebuffer(device, swapImageFramebuffers[i - 1], VK_NULL_HANDLE);
        }
    }
    if (this->Initialized&INIT_RENDER_PASS)vkDestroyRenderPass(device, renderPass, VK_NULL_HANDLE);
    if (this->Initialized&INIT_SWAP_IMAGES) {
        for (uint32_t i = swapImageCount; i > 0; i--) {
            vkDestroyImageView(device, swapImageViews[i - 1], VK_NULL_HANDLE);
        }
    }
    if (this->Initialized&INIT_COMMAND_POOL) {
        free(swapImageViews);
        free(swapImages);
        vkFreeCommandBuffers(device, commandPool, 3, commandBuffers);
        vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);
    }
    if (this->Initialized&INIT_PRESENT_SEMAPHORE)vkDestroySemaphore(device, presentSemaphore, VK_NULL_HANDLE);
    if (this->Initialized&INIT_VIEWBOTTOM)vkDestroyImageView(device, textures[1].view, VK_NULL_HANDLE);
    if (this->Initialized&INIT_VIEWTOP)vkDestroyImageView(device, textures[0].view, VK_NULL_HANDLE);
    if (this->Initialized&INIT_MEMORYBOTTOM)vkFreeMemory(this->device, this->textures[1].memory, VK_NULL_HANDLE);
    if (this->Initialized&INIT_MEMORYTOP)vkFreeMemory(this->device, this->textures[0].memory, VK_NULL_HANDLE);
    if (this->Initialized&INIT_BOTTOMSCREEN)vkDestroyImage(this->device, this->textures[1].texture, VK_NULL_HANDLE);
    if (this->Initialized&INIT_TOPSCREEN)vkDestroyImage(this->device, this->textures[0].texture, VK_NULL_HANDLE);
    if (this->Initialized&INIT_SWAPCHAIN)vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
    if (this->Initialized&INIT_SURFACE)render_window->DestroyVulkanSurface(instance, screen);
    if (this->Initialized&INIT_DEVICE)vkDestroyDevice(this->device, VK_NULL_HANDLE);
#ifdef DEBUG_INTEGRATION
    if (this->hasDebugCallback)vkDestroyDebugReportCallbackEXTLocal(this->instance, debugCallback, VK_NULL_HANDLE);
#endif
    if (this->Initialized&INIT_INSTANCE)vkDestroyInstance(this->instance, VK_NULL_HANDLE);
}

RendererVulkan::~RendererVulkan(){}