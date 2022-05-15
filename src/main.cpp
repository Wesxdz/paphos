#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>
#include <flecs/flecs.h>

#include <iostream>

#include "systems.h"
#include "components.h"

int main()
{
    flecs::world ecs;

    ecs.trigger<PlatformFramework>().event(flecs::OnAdd).each(SetupGLFW);
    ecs.trigger<Window>().event(flecs::OnAdd).each(CreateWindow);

    ecs.system<PlatformFramework>().kind(flecs::PreUpdate).iter(PollEvents);
    ecs.system<Window>().iter(CloseWindow);

    ecs.trigger<PlatformFramework>().event(flecs::OnRemove).each(ShutdownGLFW);
    ecs.trigger<Window>().event(flecs::OnRemove).each(DestroyWindow);


    auto platform = ecs.entity("platform_framework").add<PlatformFramework>();
    auto window = ecs.entity().add<Window>();
    
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    spdlog::info("{} extensions supported", extensionCount);

    glm::mat4 matrix;
    glm::vec4 vec;
    auto test = matrix * vec;

    while (!ecs.should_quit())
    {
        ecs.progress();
    }

    return 0;
}