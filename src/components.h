#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

struct PlatformFramework 
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
};

struct Window
{
    GLFWwindow* object;
};