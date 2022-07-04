#pragma once

#include <flecs/flecs.h>
#include <spdlog/spdlog.h>
#include <string.h>
#include <set>
#include "components.h"
#include "callback.h"
#include "vkutil.h"

#include "gpu/vk/GrVkBackendContext.h"
#include "gpu/vk/GrVkExtensions.h"
#include "gpu/GrDirectContext.h"
#include "core/SkColorSpace.h"
#include "core/SkCanvas.h"

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
    for (size_t i = 0; i < glfwExtensionCount; i++)
    {
        pf.extensions.push_back(glfwExtensions[i]);
    }
    if (ENABLE_VALIDATION_LAYERS)
    {
        pf.extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    pf.deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    createInfo.enabledExtensionCount = static_cast<uint32_t>(pf.extensions.size());
    createInfo.ppEnabledExtensionNames = pf.extensions.data();
    
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

void SelectPrimaryRenderDevice(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);
    spdlog::info("Select primary render device");
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(pf->instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        spdlog::error("Failed to find GPU with Vulkan support");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(pf->instance, &deviceCount, devices.data());
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

        VkBool32 extensionsSupported = checkDeviceExtensionSupport(pf, device);
        VkBool32 swapChainAdequate = false;
        if (extensionsSupported)
        {   
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, window->surface, &window->capabilities);
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, window->surface, &formatCount, nullptr);
            if (formatCount != 0)
            {
                window->formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, window->surface, &formatCount, window->formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, window->surface, &presentModeCount, nullptr);

            if (presentModeCount != 0) {
                window->presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, window->surface, &presentModeCount, window->presentModes.data());
            }
            swapChainAdequate = !window->formats.empty() && !window->presentModes.empty();

        }

        VkBool32 canPresent = false;
        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                rd->graphicsFamily = i;
                hasGraphics = true;
            }
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, window->surface, &canPresent);
            if (canPresent)
            {
                rd->presentFamily = i;
            }
            i++;
        }

        if (hasGraphics && deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        deviceFeatures.geometryShader && extensionsSupported && swapChainAdequate)
        {
            spdlog::info("Selected primary render device {}", deviceProperties.deviceName);
            rd->physical = device;
            break;
        }

    }
}

void SpecifyLogicalDevice(flecs::entity e, PlatformFramework& pf, RenderDevice& rd)
{
    spdlog::info("Specify logical device");
    std::set<uint32_t> distinctQueueFamilies = {rd.graphicsFamily, rd.presentFamily};

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(distinctQueueFamilies.size());
    int i = 0;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    for (const auto family : distinctQueueFamilies)
    {
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfos[i] = queueCreateInfo;
        i++;
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfos.data()[0];
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = pf.deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = pf.deviceExtensions.data();
    createInfo.enabledLayerCount = 0; // device only validation layers depreciated
    VkResult result;
    if ((result = vkCreateDevice(rd.physical, &createInfo, nullptr, &rd.logical)) != VK_SUCCESS)
    {
        spdlog::error("Failed to create logical device {}", result);
    }
    vkGetDeviceQueue(rd.logical, rd.graphicsFamily, 0, &rd.graphicsQueue);
    vkGetDeviceQueue(rd.logical, rd.presentFamily, 0, &rd.presentQueue);
}

void CreateSwapChain(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);
    spdlog::info("Create swapchain!");
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(window->formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(window->presentModes);
    VkExtent2D extent = chooseSwapExtent(window->object, window->capabilities);
    uint32_t imageCount = window->capabilities.minImageCount + 1;
    if (window->capabilities.maxImageCount > 0 && imageCount > window->capabilities.maxImageCount) {
        imageCount = window->capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = window->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {rd->graphicsFamily, rd->presentFamily};

    if (rd->graphicsFamily != rd->presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0; // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }
    createInfo.preTransform = window->capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    VkResult result;
    if ((result = vkCreateSwapchainKHR(rd->logical, &createInfo, nullptr, &window->swapChain)) != VK_SUCCESS)
    {
        spdlog::error("Failed to create swapchain");
    }
    vkGetSwapchainImagesKHR(rd->logical, window->swapChain, &imageCount, nullptr);
    window->swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(rd->logical, window->swapChain, &imageCount, window->swapChainImages.data());
    window->swapChainImageFormat = surfaceFormat.format;
    window->swapChainExtent = extent;

    window->swapChainImageViews.resize(window->swapChainImages.size());
    for (size_t i = 0; i < window->swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = window->swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = window->swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(rd->logical, &createInfo, nullptr, &window->swapChainImageViews[i]) != VK_SUCCESS)
        {
            spdlog::error("Failed to create swapchain image views");
        }

    }
}

void CreateRenderPass(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = window->swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(rd->logical, &renderPassInfo, nullptr, &window->renderPass) != VK_SUCCESS)
    {
        spdlog::error("Failed to create render pass");
    }

}


void CreateGraphicsPipeline(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);

    auto vertShaderCode = readFile("../res/shaders/vert.spv");
    auto fragShaderCode = readFile("../res/shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(rd->logical, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(rd->logical, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) window->swapChainExtent.width;
    viewport.height = (float) window->swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = window->swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;

    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(rd->logical, &pipelineLayoutInfo, nullptr, &window->pipelineLayout) != VK_SUCCESS) {
        spdlog::error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional
    pipelineInfo.layout = window->pipelineLayout;
    pipelineInfo.renderPass = window->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(rd->logical, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &window->graphicsPipeline) != VK_SUCCESS)
    {
        spdlog::error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(rd->logical, fragShaderModule, nullptr);
    vkDestroyShaderModule(rd->logical, vertShaderModule, nullptr);

}

void CreateSkiaSurface(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd, SkiaGPU* skgpu)
{
    auto window = it.term<const Window>(4);
    spdlog::info("Create Skia surface");
    
    GrVkBackendContext backend;
    backend.fInstance = pf->instance;
    backend.fPhysicalDevice = rd->physical;
    backend.fDevice = rd->logical;
    backend.fQueue = rd->graphicsQueue;
    backend.fGraphicsQueueIndex = rd->graphicsFamily;
    backend.fMaxAPIVersion = VK_API_VERSION_1_2;
    std::unique_ptr<GrVkExtensions> extensions(new GrVkExtensions());
    extensions->init(getProc, pf->instance, rd->physical, pf->extensions.size(), pf->extensions.data(), pf->deviceExtensions.size(), pf->deviceExtensions.data());
    backend.fVkExtensions = extensions.get();
    backend.fGetProc = getProc;
    VkPhysicalDeviceFeatures features;
    features.geometryShader = true;
    backend.fDeviceFeatures = &features;
    backend.fProtectedContext = GrProtected::kNo;
    skgpu->vkContext = GrDirectContext::MakeVulkan(backend);
    if (!skgpu->vkContext)
    {
        spdlog::error("Failed to create Skia Vulkan context");
    }


    // High DPI???
    int width, height;
    glfwGetWindowSize(window->object, &width, &height);
    GrVkImageInfo renderTargetInfo;
    renderTargetInfo.fImage = window->swapChainImages[0];
    renderTargetInfo.fFormat = window->swapChainImageFormat;
    renderTargetInfo.fLevelCount = 1;

    GrVkYcbcrConversionInfo conversionInfo;
    conversionInfo.fFormat = window->swapChainImageFormat;
    renderTargetInfo.fYcbcrConversionInfo = conversionInfo;

    skgpu->rt = new GrBackendRenderTarget(width, height, renderTargetInfo);
    if (!skgpu->rt->isValid())
    {
        spdlog::error("Failed to create Skia render target");
    }
    GrSurfaceOrigin origin;
    SkColorType colorType = kBGRA_8888_SkColorType;
    sk_sp<SkColorSpace> colorSpace = SkColorSpace::MakeSRGB();
    SkSurfaceProps surfaceProps;
    skgpu->surface = SkSurface::MakeFromBackendRenderTarget(skgpu->vkContext.get(), *skgpu->rt, origin, colorType, colorSpace, &surfaceProps);
    if (!skgpu->surface)
    {
        spdlog::error("Failed to create Skia surface");
    }
    skgpu->surface->getCanvas()->clear(SK_ColorRED);
}


void CreateFramebuffers(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);
    window->swapChainFramebuffers.resize(window->swapChainImageViews.size());

    for (size_t i = 0; i < window->swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            window->swapChainImageViews[i]
        };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = window->renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = window->swapChainExtent.width;
        framebufferInfo.height = window->swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(rd->logical, &framebufferInfo, nullptr, &window->swapChainFramebuffers[i]) != VK_SUCCESS) 
        {
            spdlog::error("Failed to create framebuffer");
        }
    }

}

void CreateCommandPool(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = rd->graphicsFamily;

    if (vkCreateCommandPool(rd->logical, &poolInfo, nullptr, &window->commandPool) != VK_SUCCESS) {
        spdlog::error("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = window->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(rd->logical, &allocInfo, &window->commandBuffer) != VK_SUCCESS) {
        spdlog::error("Failed to allocate command buffers");
    }

}

void CreateSyncObjects(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);
    spdlog::info("Create sync objects");

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(rd->logical, &semaphoreInfo, nullptr, &window->imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(rd->logical, &semaphoreInfo, nullptr, &window->renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(rd->logical, &fenceInfo, nullptr, &window->inFlightFence) != VK_SUCCESS) {
        spdlog::error("Failed to create semaphores");
    }
}

void ShutdownFramework(flecs::entity e, PlatformFramework& pf, RenderDevice& rd)
{
    vkDestroyDevice(rd.logical, nullptr);
    DestroyDebugUtilsMessengerEXT(pf.instance, pf.debugMessenger, nullptr);
    vkDestroyInstance(pf.instance, nullptr);
    glfwTerminate();
    e.world().quit();
}

void RenderSkiaTest(flecs::iter& it, SkiaGPU* skgpu)
{
    // spdlog::info("Render skia");
    // skgpu->vkContext->submit();
}

void RenderFrame(flecs::iter& it, PlatformFramework* pf, RenderDevice* rd)
{
    auto window = it.term<Window>(3);
    vkWaitForFences(rd->logical, 1, &window->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(rd->logical, 1, &window->inFlightFence);
    uint32_t imageIndex;
    vkAcquireNextImageKHR(rd->logical, window->swapChain, UINT64_MAX, window->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    vkResetCommandBuffer(window->commandBuffer, 0);
    recordCommandBuffer(window->commandBuffer, imageIndex, &*window);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {window->imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &window->commandBuffer;

    VkSemaphore signalSemaphores[] = {window->renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(rd->graphicsQueue, 1, &submitInfo, window->inFlightFence) != VK_SUCCESS) {
        spdlog::error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {window->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(rd->presentQueue, &presentInfo);

}

void CreateWindowSurface(flecs::iter& it, Window* window)
{
    auto pf = it.term<const PlatformFramework>(2);
    auto rd = it.term<const RenderDevice>(3);
    spdlog::info("Create window surface");
    VkResult result;
    if ((result = glfwCreateWindowSurface(pf->instance, window->object, nullptr, &window->surface)) != VK_SUCCESS)
    {
        spdlog::error("Failed to create window surface {}", result);
    }

}

void DestroyWindowSurface(flecs::iter it, Window* window)
{
    auto pf = it.term<const PlatformFramework>(2);
    auto rd = it.term<const RenderDevice>(3);
    vkDeviceWaitIdle(rd->logical);
    vkDestroySemaphore(rd->logical, window->imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(rd->logical, window->renderFinishedSemaphore, nullptr);
    vkDestroyFence(rd->logical, window->inFlightFence, nullptr);
    vkDestroyCommandPool(rd->logical, window->commandPool, nullptr);
    for (auto framebuffer : window->swapChainFramebuffers) {
        vkDestroyFramebuffer(rd->logical, framebuffer, nullptr);
    }
    for (auto imageView : window->swapChainImageViews)
    {
        vkDestroyImageView(rd->logical, imageView, nullptr);
    }
    vkDestroyPipeline(rd->logical, window->graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(rd->logical, window->pipelineLayout, nullptr);
    vkDestroyRenderPass(rd->logical, window->renderPass, nullptr);
    vkDestroySwapchainKHR(rd->logical, window->swapChain, nullptr);
    vkDestroySurfaceKHR(pf->instance, window->surface, nullptr);
}