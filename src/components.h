#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

struct PlatformFramework 
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
};

struct RenderDevice
{
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t graphicsFamily;
    VkDevice logical;
    VkQueue graphicsQueue;
};

struct Window
{
    GLFWwindow* object;
};