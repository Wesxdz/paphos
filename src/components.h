#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "gpu/GrDirectContext.h"
#include "gpu/vk/GrVkBackendContext.h"
#include "gpu/GrBackendSurface.h"
#include "core/SkSurface.h"
#include "core/SkRefCnt.h"

struct PlatformFramework 
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    std::vector<const char*> extensions;
    std::vector<const char*> deviceExtensions;
};

struct RenderDevice
{
    VkPhysicalDevice physical;
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    VkDevice logical;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
};

struct Window
{
    GLFWwindow* object;
    VkSurfaceKHR surface;
    
    // SurfaceInfo
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    // SwapChain
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
    VkPipeline graphicsPipeline;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

};

struct SkiaGPU
{
    sk_sp<GrDirectContext> vkContext;
    // Use GrVkImageInfo to construct the following
    // sk_sp<GrRenderTarget> renderTarget;
    // GrBackendTexture* texture;
    GrBackendRenderTarget* rt;
    sk_sp<SkSurface> surface;
};