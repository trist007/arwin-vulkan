#include "vk_engine.h"
#include <SDL3/SDL_vulkan.h>
#include <vk_initializers.h>
#include <vk_images.h>

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

    SDL_Vulkan_CreateSurface(engine->window, engine->instance, 0, &engine->surface);

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

    // use VkBootstrap to get a Graphics queue
    engine->graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    engine->graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void
init_swapchain(VulkanEngine *engine)
{
    create_swapchain(engine, engine->windowExtent.width, engine->windowExtent.height);
}

void
init_commands(VulkanEngine *engine)
{
    // create a command pool for commands submitted to the graphics queue
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        engine->graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for(int i = 0;
    i < FRAME_OVERLAP;
    ++i)
    {
        VK_CHECK(vkCreateCommandPool(engine->device, &commandPoolInfo, 0, &engine->frames[i].commandPool));
        
        // allocate the default command buffer that wee will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(engine->frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(engine->device, &cmdAllocInfo, &engine->frames[i].mainCommandBuffer));
    }
}

void
init_sync_structures(VulkanEngine *engine)
{
    // create synchronization structure
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to synchronize rendering with swapchain, we want
    // the fence to start signalled so we can wait on it on the first frame
    // ! NOTE: trist007: On the fence, we are using the flag VK_FENCE_CREATE_SIGNALED_BIT.
    // ! This is very important, as it allows us to wait on a freshly created fence without causing errors.
    // ! If we did not have that bit, when we call into WaitFences the first frame, before the gpu is doing work,
    // ! the thread will be blocked.
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for(int i = 0;
    i < FRAME_OVERLAP;
    ++i)
    {
        VK_CHECK(vkCreateFence(engine->device, &fenceCreateInfo, 0, &engine->frames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCreateInfo, 0, &engine->frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCreateInfo, 0, &engine->frames[i].renderSemaphore));
    }
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
    vkDestroySwapchainKHR(engine->device, engine->swapchain, 0);    

    for(uint32_t i = 0;
        i < engine->swapchainImageCount;
        ++i)
        vkDestroyImageView(engine->device, engine->swapchainImageViews[i], 0);
}

void
cleanupVulkanEngine(VulkanEngine *engine)
{
    if(engine->isInitialized)
    {
        //make sure the gpu has stopped doing its business
        vkDeviceWaitIdle(engine->device);

        for(int i = 0;
        i < FRAME_OVERLAP;
        ++i)
        {
            // cannot destroy VkQueue just like VkPhysicalDevice
            vkDestroyCommandPool(engine->device, engine->frames[i].commandPool, 0);

            // destroy sync objects
            vkDestroyFence(engine->device, engine->frames[i].renderFence, nullptr);
		    vkDestroySemaphore(engine->device, engine->frames[i].renderSemaphore, nullptr);
		    vkDestroySemaphore(engine->device , engine->frames[i].swapchainSemaphore, nullptr);
        }

        destroy_swapchain(engine);

        vkDestroySurfaceKHR(engine->instance, engine->surface, 0);
        vkDestroyDevice(engine->device, 0);

        vkb::destroy_debug_utils_messenger(engine->instance, engine->debugMessenger);
        vkDestroyInstance(engine->instance, 0);
        SDL_DestroyWindow(engine->window);

        // clear engine pointer
        s_engine = 0;
    }
}

void
drawVulkanEngine(VulkanEngine *engine)
{
    FrameData *frame = getCurrentFrame(engine);

    // wait until the gpu has finished rendering the last frame.  Timeout of 1 second
    // ! NOTE: trist007: We use vkWaitForFences() to wait for the GPU to have finished its work, and after it we reset the fence.
    // ! Fences have to be reset between uses, you can’t use the same fence on multiple GPU commands without resetting it in the middle.
    // ! The timeout of the WaitFences call is of 1 second. It’s using nanoseconds for the wait time. If you call the function with 0 as
    // ! the timeout, you can use it to know if the GPU is still executing the command or not.
    VK_CHECK(vkWaitForFences(engine->device, 1, &frame->renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(engine->device, 1, &frame->renderFence));

    // request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(engine->device, engine->swapchain, 1000000000, frame->swapchainSemaphore, 0, &swapchainImageIndex));

    // naming it cmd for shorter writing
    VkCommandBuffer cmd = frame->mainCommandBuffer;

    // now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // begin the command buffer recording, we will use this command buffer exactly once, so we want to let vulkan know that
    // ! NOTE: trist007: we will give it the flag VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT This is optional, but we might get a
    // ! small speedup from our command encoding if we can tell the drivers
    // ! that this buffer will only be submitted and executed once. We are only doing 1 submit per frame before the command buffer
    // ! is reset, so this is perfectly good for us.
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); 

    // start the command buffer recording
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // make the swapchain image into writeable mode before rendering
    vkutil::transition_image(cmd, engine->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // make a clear-color from frame number.  This will flash with a 120 frame period
    VkClearColorValue clearValue;
    float flash = std::abs(std::sin(engine->frameNumber / 120.0f)); 
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(cmd, engine->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    // transition to presentable
    vkutil::transition_image(cmd, engine->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame->swapchainSemaphore);

    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame->renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
	// engine->renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(engine->graphicsQueue, 1, &submit, frame->renderFence));

    //prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &engine->swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &frame->renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(engine->graphicsQueue, &presentInfo));

    //increase the number of frames drawn
    engine->frameNumber++;
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

FrameData*
getCurrentFrame(VulkanEngine *engine)
{
    return &engine->frames[engine->frameNumber % FRAME_OVERLAP];
}

