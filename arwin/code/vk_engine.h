#pragma once
#include <vk_types.h>
#include <SDL3/SDL.h>

#define MAX_SWAPCHAIN_IMAGES 8

struct VulkanEngine
{
    bool isInitialized;
    bool stop_rendering;
    int frameNumber;
    VkExtent2D windowExtent;
    SDL_Window *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkDevice device;
    VkPhysicalDevice chosenGPU;
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchainImageViews[MAX_SWAPCHAIN_IMAGES];
    uint32_t swapchainImageCount;
    VkExtent2D swapchainExtent;
};

// singleton for pointer retrieval
VulkanEngine *getVulkanEngine(void);

// VkBootstrap functions
/*
	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface
*/

void init_vulkan(VulkanEngine *engine);
void init_swapchain(VulkanEngine *engine);
void init_commands(VulkanEngine *engine);
void init_sync_structures(VulkanEngine *engine);
void create_swapchain(VulkanEngine *engine, uint32_t width, uint32_t height);
void destroy_swapchain(VulkanEngine *engine);
// end of VkBootstrap functions

void initVulkanEngine(VulkanEngine *engine);
void cleanupVulkanEngine(VulkanEngine *engine);
void drawVulkanEngine(VulkanEngine *engine);
void runVulkanEngine(VulkanEngine * engine);
