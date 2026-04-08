#include "vk_engine.h"
#include <SDL3/SDL_vulkan.h>
#include <vk_initializers.h>

void
initVulkanEngine(VulkanEngine *engine)
{
    engine->windowExtent.width = 1700;
    engine->windowExtent.height = 900;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    engine->window = SDL_CreateWindow(
        "Vulkan Engine",
        engine->windowExtent.width,
        engine->windowExtent.height,
        window_flags
    );

    engine->isInitialized = true;
}

void
cleanupVulkanEngine(VulkanEngine *engine)
{
    if(engine->isInitialized)
        SDL_DestroyWindow(engine->window);
}

void
drawVulkanEngine(VulkanEngine *engine)
{
    // nothing yet
}

void
runVulkanEngine(VulkanEngine *engine)
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while(!bQuit)
    {
        while(SDL_PollEvent(&e) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if(e.type == SDL_EVENT_QUIT) bQuit = true;
        }

        drawVulkanEngine(engine);
    }
}

