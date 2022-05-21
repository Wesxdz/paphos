#pragma once

#include <flecs/flecs.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include "components.h"
#include "callback.h"

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}


void CreateWindow(flecs::entity e, Window& window)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.object = glfwCreateWindow(800, 600, EDITOR_NAME, nullptr, nullptr);
}

void PollEvents(flecs::iter& it)
{
    glfwPollEvents();
}

void CloseWindow(flecs::iter& it, Window* window)
{
    int closed = 0;
    for (int i = 0; i < it.count(); i++)
    {
        if (glfwWindowShouldClose(window[i].object))
        {
            it.entity(i).destruct();
            closed++;
        }
    }
    if (it.count() - closed == 0)
    {
        it.world().lookup("core").destruct();
    }
}

void DestroyWindow(flecs::entity e, Window& window)
{
    glfwDestroyWindow(window.object);
}

void SetupFramework(flecs::entity e, PlatformFramework& pf)
{
    glfwInit();

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = (std::string(EDITOR_NAME) + " editor").c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = (std::string(EDITOR_NAME) + " engine").c_str();
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;


    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (ENABLE_VALIDATION_LAYERS)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    
    createInfo.enabledLayerCount = 0;
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    if (ENABLE_VALIDATION_LAYERS)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    VkResult result;
    if ((result = vkCreateInstance(&createInfo, nullptr, &pf.instance)) != VK_SUCCESS)
    {
        spdlog::error("Failed to create Vulkan instance. Error code {}", result);
    }

    VkDebugUtilsMessengerCreateInfoEXT dmCreateInfo{};
    dmCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dmCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dmCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dmCreateInfo.pfnUserCallback = debugCallback;
    
    CreateDebugUtilsMessengerEXT(pf.instance, &dmCreateInfo, nullptr, &pf.debugMessenger);
    
}

void SelectPrimaryRenderDevice(flecs::entity e, PlatformFramework& pf, RenderDevice& rd)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(pf.instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        spdlog::error("Failed to find GPU with Vulkan support");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(pf.instance, &deviceCount, devices.data());
    for (const auto& device : devices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        bool hasGraphics = false;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                rd.graphicsFamily = i;
                hasGraphics = true;
                break;
            }
            i++;
        }

        if (hasGraphics && deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        deviceFeatures.geometryShader)
        {
            spdlog::info("Selected primary render device {}", deviceProperties.deviceName);
            rd.physical = device;
            break;
        }
    }
}

void SpecifyLogicalDevice(flecs::entity e, PlatformFramework& pf, RenderDevice& rd)
{
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = rd.graphicsFamily;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;
    createInfo.enabledLayerCount = 0; // device only validation layers depreciated
    VkResult result;
    if ((result = vkCreateDevice(rd.physical, &createInfo, nullptr, &rd.logical)) != VK_SUCCESS)
    {
        spdlog::error("Failed to create logical device {}", result);
    }
    vkGetDeviceQueue(rd.logical, rd.graphicsFamily, 0, &rd.graphicsQueue);
}

void ShutdownFramework(flecs::entity e, PlatformFramework& pf, RenderDevice& rd)
{
    vkDestroyDevice(rd.logical, nullptr);
    DestroyDebugUtilsMessengerEXT(pf.instance, pf.debugMessenger, nullptr);
    vkDestroyInstance(pf.instance, nullptr);
    glfwTerminate();
    e.world().quit();
}