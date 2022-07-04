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
    // ecs.set_threads(FLECS_THREAD_COUNT);

    ecs.trigger<PlatformFramework>().event(flecs::OnAdd).each(SetupFramework);
    ecs.trigger<Window>().event(flecs::OnAdd).each(CreateWindow);

    ecs.system<PlatformFramework>().kind(flecs::PreUpdate).iter(PollEvents);
    ecs.system<Window>().iter(CloseWindow);

    auto platform = ecs.entity("core")
        .add<PlatformFramework>()
        .add<RenderDevice>()
        .add<SkiaGPU>();

    auto window = ecs.entity("window").add<Window>();

    ecs.observer<Window>()
        .term<PlatformFramework>().subj("core")
        .term<RenderDevice>().subj("core")
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateWindowSurface);

    ecs.observer<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(SelectPrimaryRenderDevice);

    ecs.observer<PlatformFramework, RenderDevice>().event(flecs::OnAdd).yield_existing().each(SpecifyLogicalDevice);
    
    ecs.observer<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateSwapChain);

    ecs.observer<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateRenderPass);

    ecs.observer<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateGraphicsPipeline);

    ecs.observer<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateFramebuffers);

    ecs.observer<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateCommandPool);

    ecs.observer<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateSyncObjects);

    ecs.observer<PlatformFramework, RenderDevice, SkiaGPU>()
        .term<Window>().subj("window")
        .event(flecs::OnAdd)
        .yield_existing()
        .iter(CreateSkiaSurface);

    ecs.observer<Window>()
        .term<PlatformFramework>().subj("core")
        .term<RenderDevice>().subj("core")
        .event(flecs::OnRemove)
        .iter(DestroyWindowSurface);

    ecs.observer<PlatformFramework, RenderDevice>().event(flecs::OnRemove).each(ShutdownFramework);
    ecs.trigger<Window>().event(flecs::OnRemove).each(DestroyWindow);

    ecs.system<SkiaGPU>()
        .iter(RenderSkiaTest);

    ecs.system<PlatformFramework, RenderDevice>()
        .term<Window>().subj("window").read_write()
        .iter(RenderFrame);
        
    while (!ecs.should_quit())
    {
        ecs.progress();
    }

    return 0;
}