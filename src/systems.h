#pragma once

#include <flecs/flecs.h>
#include "components.h"

void CreateWindow(flecs::entity e, Window& window)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.object = glfwCreateWindow(800, 600, "Paphos", nullptr, nullptr);
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
        it.world().lookup("platform_framework").destruct();
    }
}

void DestroyWindow(flecs::entity e, Window& window)
{
    glfwDestroyWindow(window.object);
}

void SetupGLFW(flecs::entity e, const PlatformFramework& pf)
{
    glfwInit();
}

void ShutdownGLFW(flecs::entity e, const PlatformFramework& pf)
{
    glfwTerminate();
    e.world().quit();
}