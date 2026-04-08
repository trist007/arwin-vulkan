#pragma once
#include <vk_types.h>
#include <SDL3/SDL.h>

struct VulkanEngine
{
    bool isInitialized;
    int frameNumber;
    VkExtent2D windowExtent;
    SDL_Window *window;
};

void initVulkanEngine(VulkanEngine *engine);
void cleanupVulkanEngine(VulkanEngine *engine);
void drawVulkanEngine(VulkanEngine *engine);
void runVulkanEngine(VulkanEngine * engine);