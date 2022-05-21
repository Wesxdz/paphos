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
    ecs.set_threads(FLECS_THREAD_COUNT);

    ecs.trigger<PlatformFramework>().event(flecs::OnAdd).each(SetupFramework);
    ecs.observer<PlatformFramework, RenderDevice>().event(flecs::OnAdd).each(SelectPrimaryRenderDevice);
    ecs.observer<PlatformFramework, RenderDevice>().event(flecs::OnAdd).each(SpecifyLogicalDevice);
    ecs.trigger<Window>().event(flecs::OnAdd).each(CreateWindow);

    ecs.system<PlatformFramework>().kind(flecs::PreUpdate).iter(PollEvents);
    ecs.system<Window>().iter(CloseWindow);

    ecs.observer<PlatformFramework, RenderDevice>().event(flecs::OnRemove).each(ShutdownFramework);
    ecs.trigger<Window>().event(flecs::OnRemove).each(DestroyWindow);


    auto platform = ecs.entity("core")
        .add<PlatformFramework>()
        .add<RenderDevice>();
    auto window = ecs.entity().add<Window>();
    
    while (!ecs.should_quit())
    {
        ecs.progress();
    }

    return 0;
}