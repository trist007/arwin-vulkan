#include "vk_engine.h"
#include <SDL3/SDL_vulkan.h>
#include <vk_initializers.h>

// bootstrap library
#include "VKBootstrap.h"

constexpr bool bUseValidationLayers = true;

static VulkanEngine *s_engine = 0;

VulkanEngine *
getVulkanEngine(void)
{
   return(s_engine);
}

void
initVulkanEngine(VulkanEngine *engine)
{
    assert(s_engine == 0);
    s_engine = engine;

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

    // VkBootstrap
    init_vulkan(engine);
    init_swapchain(engine);
    init_commands(engine);
    init_sync_structures(engine);
    
    // everything went fine
    engine->isInitialized = true;
}

void
init_vulkan(VulkanEngine *engine)
{
    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Vulkan Tutorial")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    if (!inst_ret) {
        SDL_Log("Failed to create Vulkan instance: %s", inst_ret.error().message().c_str());
        abort();
    }

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    engine->instance = vkb_inst.instance;
    engine->debugMessenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(engine->window, engine->instance, NULL, &engine->surface);

    //vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features.dynamicRendering = true;
	features.synchronization2 = true;

	//vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;


	//use vkbootstrap to select a gpu. 
	//We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_surface(engine->surface)
		.select()
		.value();


	//create the final vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a vulkan application
	engine->device = vkbDevice.device;
	engine->chosenGPU = physicalDevice.physical_device;
}

void
init_swapchain(VulkanEngine *engine)
{
    create_swapchain(engine, engine->windowExtent.width, engine->windowExtent.height);
}

void
init_commands(VulkanEngine *engine)
{
    // TODO
}

void
init_sync_structures(VulkanEngine *engine)
{
    // TODO
}

void
create_swapchain(VulkanEngine *engine, uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ engine->chosenGPU, engine->device, engine->surface };

	engine->swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = engine->swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		//use vsync present mode
         //! NOTE: trist007: here we specify the SwapChain Mode VK_PRESENT_MODE_FIFO_KHR does a Hard VSync
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) 
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	engine->swapchainExtent = vkbSwapchain.extent;
	engine->swapchain = vkbSwapchain.swapchain;

    //copy images into fixed array
    std::vector<VkImage> images = vkbSwapchain.get_images().value();
    std::vector<VkImageView> imageViews = vkbSwapchain.get_image_views().value();

    engine->swapchainImageCount = (uint32_t)images.size();
    for (uint32_t i = 0; i < engine->swapchainImageCount; ++i) {
        engine->swapchainImages[i] = images[i];
        engine->swapchainImageViews[i] = imageViews[i];
    }

}

void
destroy_swapchain(VulkanEngine *engine)
{
    vkDestroySwapchainKHR(engine->device, engine->swapchain, NULL);    

    for(uint32_t i = 0;
        i < engine->swapchainImageCount;
        ++i)
        vkDestroyImageView(engine->device, engine->swapchainImageViews[i], NULL);
}

void
cleanupVulkanEngine(VulkanEngine *engine)
{
    if(engine->isInitialized)
    {
        destroy_swapchain(engine);

        vkDestroySurfaceKHR(engine->instance, engine->surface, NULL);
        vkDestroyDevice(engine->device, NULL);

        vkb::destroy_debug_utils_messenger(engine->instance, engine->debugMessenger);
        vkDestroyInstance(engine->instance, NULL);
        SDL_DestroyWindow(engine->window);

        // clear engine pointer
        s_engine = 0;
    }
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
            if(e.type == SDL_EVENT_QUIT)
                bQuit = true;

            if(e.type == SDL_EVENT_WINDOW_MINIMIZED)
               engine->stop_rendering = true;

            if(e.type == SDL_EVENT_WINDOW_RESTORED)
               engine->stop_rendering = false;
        }

        // do not draw if we are minimized
        if(engine->stop_rendering)
        {
            // throttle the speed to avoid the endless spinning
            SDL_Delay(5000);
            continue;

        }

        drawVulkanEngine(engine);
    }
}

