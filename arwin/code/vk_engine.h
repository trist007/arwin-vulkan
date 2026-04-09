#pragma once
#include <vk_types.h>
#include <vk_descriptors.h>
#include <SDL3/SDL.h>

#define MAX_SWAPCHAIN_IMAGES 8

// ! NOTE: trist007: deque is a double ended Queue that allows fast insertion and deletion at both
// ! front and back ends, unlike a regular array which is only fast at the back
// ! std::deque works like a mix between a vector and a linked list
/* Example of a deque impl
std::deque<int> d;
d.push_back(1);   // add to back
d.push_front(2);  // add to front
d.pop_back();     // remove from back
d.pop_front();    // remove from front
*/

// this is a poor performance way of doing callbacks as it is inefficient at scale storing
// whole std::functions for every object, but will suffice for our exercise
// ! NOTE: trist007: if you need to delete thousands of objects a better impl
// ! would be to store arrays of vulkan handles of various types such as VkImage, VkBuffer,
// ! and so on and then delete those from a loop
struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    // this pushes a lambda meant for deletion
    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(function); // adds it to the back of the deque
    }

    // this starts at the last/most recent element that was added to the queue and calls
    // the lambda to delete, in Vulkan you need to delete in reverse order
    void flush()
    {
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) // last added first destroyed
        {
            (*it)();
        }
        deletors.clear();
    }
};


struct FrameData
{
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;
    VkSemaphore swapchainSemaphore;
    VkSemaphore renderSemaphore;
    VkFence renderFence;
    DeletionQueue deletionQueue;
};

constexpr unsigned int FRAME_OVERLAP = 2;

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
    FrameData frames[FRAME_OVERLAP];
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
    DeletionQueue deletionQueue;
    DeletionQueue mainDeletionQueue;
    VmaAllocator allocator;

    // draw resources
    AllocatedImage drawImage;
    VkExtent2D drawExtent;

    // VkDescriptor
    DescriptorAllocator globalDescriptorAllocator;
    VkDescriptorSet drawImageDescriptors;
    VkDescriptorSetLayout drawImageDescriptorLayout;

    // VkPipeline
    VkPipeline gradientPipeline;
    VkPipelineLayout gradientPipelineLayout;
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
void init_descriptors(VulkanEngine *engine);
void init_pipelines(VulkanEngine *engine);
void init_background_pipelines(VulkanEngine *engine);
void create_swapchain(VulkanEngine *engine, uint32_t width, uint32_t height);
void destroy_swapchain(VulkanEngine *engine);
void draw_background(VulkanEngine *engine, VkCommandBuffer cmd);
// end of VkBootstrap functions

void initVulkanEngine(VulkanEngine *engine);
void cleanupVulkanEngine(VulkanEngine *engine);
void drawVulkanEngine(VulkanEngine *engine);
void runVulkanEngine(VulkanEngine *engine);
FrameData *getCurrentFrame(VulkanEngine *engine);
