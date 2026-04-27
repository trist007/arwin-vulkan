#include "vk_engine.h"
//#include "vk_images.h"
#include "vk_initializers.h"
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

//#define VMA_IMPLEMENTATION
//#include "vk_mem_alloc.h"

#include "vk_pipelines.h"

//#include "ktxvulkan.h"

#include "HandmadeMath.h"

#include "initVulkan.h"

//#define TINYOBJLOADER_DISABLE_FAST_FLOAT
//#define TINYOBJLOADER_IMPLEMENTATION
//#include "tiny_obj_loader.h"

#include "vk_loader.h"

bool bUseValidationLayers = true;

static struct VulkanEngine *s_engine = 0;
void sdl_log_stderr(void *userData, int category, SDL_LogPriority priority,
                    const char *message) {
  fprintf(stderr, "%s\n", message);
}

struct VulkanEngine *getVulkanEngine(void) { return (s_engine); }

void initVulkanEngine(struct VulkanEngine *engine, GameState *gameState)
{
    // redirect SDL_Log to stderr
    SDL_SetLogOutputFunction(sdl_log_stderr, NULL);

    assert(s_engine == 0);
    s_engine = engine;

    engine->windowExtent.width = 1700;
    engine->windowExtent.height = 900;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags window_flags =
        (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    engine->window = SDL_CreateWindow("Vulkan Engine", engine->windowExtent.width,
                                        engine->windowExtent.height, window_flags);

    // VkBootstrap
    init_vulkan(engine);
    init_swapchain(engine);
    init_mezzanine(engine, gameState);

    // everything went fine
    engine->isInitialized = true;

    // Set camera parameters
    engine->mainCamera.velocity = (HMM_Vec3){0.0f, 0.0f, 0.0f};
    engine->mainCamera.position = (HMM_Vec3){0.0f, 0.0f, 5.0f};

    engine->mainCamera.pitch = 0;
    engine->mainCamera.yaw = 0;
}

void init_vulkan(struct VulkanEngine *engine) {
    if (!engine || !engine->window) {
        SDL_Log("Error: Invalid engine or window passed to init_vulkan\n");
        return;
    }

    // ------------------- 1. Create Vulkan Instance -------------------
    uint32_t instanceVersion = 0;
    vkEnumerateInstanceVersion(&instanceVersion);

    SDL_Log("Vulkan loader supports version %d.%d.%d",
            VK_VERSION_MAJOR(instanceVersion), VK_VERSION_MINOR(instanceVersion),
            VK_VERSION_PATCH(instanceVersion));

    uint32_t instanceExtensionsCount = 0;
    const char *const *newinstanceExtensions = 
        SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount);

    uint32_t enabledLayerCount = 0;
    const char *enabledLayers[1] = {"VK_LAYER_KHRONOS_validation"};

    if (bUseValidationLayers) {
        enabledLayerCount = 1;
    }

    VkApplicationInfo appInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                .pApplicationName = "SDL3 Vulkan glTF Renderer",
                                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                .pEngineName = "Custom Engine",
                                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                .apiVersion = VK_API_VERSION_1_3};

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = enabledLayerCount,
        .ppEnabledLayerNames = enabledLayers,
        .enabledExtensionCount = instanceExtensionsCount,
        //.enabledExtensionCount = sizeof(instanceExtensions) /
        //sizeof(instanceExtensions[0]),
        .ppEnabledExtensionNames = newinstanceExtensions};

    VK_CHECK(vkCreateInstance(&instanceCreateInfo, NULL, &engine->instance));

    SDL_Log("Vulkan Instance created");

    // ------------------- 2. Create SDL Surface -------------------
    if (!SDL_Vulkan_CreateSurface(engine->window, engine->instance, NULL,
                                    &engine->surface)) {
        SDL_Log("Failed to create Vulkan surface: %s", SDL_GetError());
        return;
    }

    SDL_Log("SDL Vulkan Surface created");

    // ------------------- 3. Select Best Physical Device + Queue Family
    // -------------------
    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(engine->instance, &deviceCount, NULL));
    if (deviceCount == 0) {
        SDL_Log("Error: no physical devices found");
        abort();
    }

    if (deviceCount > MAX_PHYSICAL_DEVICES) {
        SDL_Log("Warning: Too many physical devices (%u), limiting to %d",
                deviceCount, MAX_PHYSICAL_DEVICES);
        deviceCount = MAX_PHYSICAL_DEVICES;
    }

    SDL_Log("Number of device found: %d", deviceCount);

    VkPhysicalDevice physicalDevices[MAX_PHYSICAL_DEVICES] = {VK_NULL_HANDLE};
    VK_CHECK(vkEnumeratePhysicalDevices(engine->instance, &deviceCount,
                                        physicalDevices));

    DeviceInformation deviceInfo = {};


    VkPhysicalDevice chosenDevice = VK_NULL_HANDLE;
    uint32_t bestQueueFamily = UINT32_MAX;
    int bestScore = -1;

    SDL_Log("device count = %d", deviceCount);
    SDL_Log("engine->instance: %p\n engine->surface: %p", engine->instance, engine->surface);

    for (uint32_t i = 0; i < deviceCount; ++i) {
        int score = evalDevice(engine->instance, engine->surface,
                            physicalDevices[i], &deviceInfo);

        SDL_Log("device: %s score = %d", deviceInfo.name, score);

        if (score > bestScore) {
        bestScore = score;
        chosenDevice = physicalDevices[i];
        bestQueueFamily =
            deviceInfo.queueFamilyIndex; // make sure evalDevice fills this!
        }
    }

    if (chosenDevice == VK_NULL_HANDLE) {
        SDL_Log("Error: Could not select a suitable physical device");
        abort();
    }

    engine->chosenGPU = chosenDevice;
    SDL_Log("Selected GPU: %s", deviceInfo.name); // better than printing garbage

    uint32_t numExt = enableExtCount(&deviceInfo);

    // ------------------- 4. Create Logical Device -------------------
    if (!createLogicalDevice(engine->chosenGPU, bestQueueFamily, &engine->device,
                            numExt, &deviceInfo)) {
        SDL_Log("Failed to create logical device!\n");
        return;
    }

    // ------------------- 5. Retrieve Graphics Queue -------------------
    engine->graphicsQueueFamily = bestQueueFamily;
    vkGetDeviceQueue(engine->device, bestQueueFamily, 0, &engine->graphicsQueue);

    SDL_Log("Vulkan initialization completed successfully!");
    SDL_Log("Graphics Queue Family Index: %u", bestQueueFamily);

    /*
    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = engine->chosenGPU;
    allocatorInfo.device = engine->device;
    allocatorInfo.instance = engine->instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &engine->allocator);
    */

    // ====================  MANUAL ARENA INITIALIZATION  ====================
    SDL_Log("Creating custom memory arenas...");

    // Device Local Arena (GPU VRAM) - for most permanent resources
    uint32_t deviceLocalType = find_memory_type(engine->chosenGPU,
        0xFFFFFFFF,                                 // accept any memoryTypeBits for now
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

    if (deviceLocalType != UINT32_MAX) {
        VkResult res = createVkArena(engine->chosenGPU, engine->device, deviceLocalType,
                                    512ULL * 1024 * 1024,   // 512 MB
                                    &engine->deviceLocalArena);
        if (res == VK_SUCCESS)
            SDL_Log("✓ DeviceLocal Arena created (type %u)", deviceLocalType);
    }

    // Staging Arena (Host Visible) - for uploading data
    uint32_t stagingType = find_memory_type(engine->chosenGPU,
        0xFFFFFFFF,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    if (stagingType != UINT32_MAX) {
        VkResult res = createVkArena(engine->chosenGPU, engine->device, stagingType,
                                    128ULL * 1024 * 1024,   // 128 MB
                                    &engine->stagingArena);
        if (res == VK_SUCCESS)
            SDL_Log("✓ Staging Arena created (type %u)", stagingType);
    }

    deletion_queue_init(&engine->mainDeletionQueue);

    // ! NOTE: trist007: [&]() is the lambda [&] is capture by reference which for
    // a deletion queue ! can be dangerous if the variable goes out of scope
    // before flush() is called [=] capture by ! value is safter because it copies
    // the handle by value at push time
    /*
    engine->mainDeletionQueue.push_function(
        [=]() { vmaDestroyAllocator(engine->allocator); });
    */
}

void init_swapchain(struct VulkanEngine *engine)
{
    create_swapchain(engine, engine->windowExtent.width,
                    engine->windowExtent.height);

    VkExtent3D drawImageExtent = {engine->windowExtent.width,
                                    engine->windowExtent.height, 1};

    engine->drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    engine->drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages = 0;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    engine->drawImage = create_image(engine, drawImageExtent,
                                     engine->drawImage.imageFormat,
                                     drawImageUsages, false);

    // ====================== DEPTH IMAGE ======================
    engine->depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    engine->depthImage.imageExtent = drawImageExtent;

    VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    engine->depthImage = create_image(engine, drawImageExtent,
                                      engine->depthImage.imageFormat,
                                      depthImageUsages, false);

    SDL_Log("Swapchain and render targets created successfully");
}

void create_swapchain(struct VulkanEngine *engine, uint32_t width, uint32_t height)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        engine->chosenGPU, engine->surface, &surfaceCapabilities));

    // 2. Choose swapchain format (B8G8R8A8 UNORM + SRGB is standard)
    // VK_FORMAT_B8G8R8A8_UNORM or VK_FORMAT_B8G8R8A8_SRGB
    engine->swapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;

    // 3. Choose present mode (FIFO = vsync, good default)
    // VK_PRESENT_MODE_FIFO_KHR is a v-synced mode and the only mode guaranteed to
    // be available everywhere.
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    // 4. Choose image count (triple buffering is nice)
    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;

    if (surfaceCapabilities.maxImageCount > 0 &&
        imageCount > surfaceCapabilities.maxImageCount) {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    // 5. Choose extent (clamp to surface capabilities)
    VkExtent2D swapchainExtent = surfaceCapabilities.currentExtent;
    if (swapchainExtent.width == UINT32_MAX ||
        swapchainExtent.height == UINT32_MAX) {
        swapchainExtent.width = width;
        swapchainExtent.height = height;
    }

    // Clamp to allowed range
    swapchainExtent.width = CLAMP(swapchainExtent.width,
                                        surfaceCapabilities.minImageExtent.width,
                                        surfaceCapabilities.maxImageExtent.width);
    swapchainExtent.height = CLAMP(
        swapchainExtent.height, surfaceCapabilities.minImageExtent.height,
        surfaceCapabilities.maxImageExtent.height);

    // 6. Create the swapchain
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = engine->surface,
        .minImageCount = imageCount,
        .imageFormat = engine->swapchainImageFormat,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE};

    VK_CHECK(vkCreateSwapchainKHR(engine->device, &createInfo, NULL,
                                    &engine->swapchain));

    engine->swapchainExtent = swapchainExtent;

    // 7. Get swapchain images
    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(engine->device, engine->swapchain,
                            &swapchainImageCount, NULL);
    SDL_Log("swapchainImageCount: %d", swapchainImageCount);

    VkImage images[MAX_SWAPCHAIN_IMAGES];
    vkGetSwapchainImagesKHR(engine->device, engine->swapchain,
                            &swapchainImageCount, images);

    engine->swapchainImageCount = swapchainImageCount;

    VkFormat depthFormatList[MAX_DEPTH_FORMATS] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    engine->depthFormat = VK_FORMAT_UNDEFINED;

    for (int32_t i = 0; i < MAX_DEPTH_FORMATS; i++) {
        VkFormatProperties2 props = {
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};

        vkGetPhysicalDeviceFormatProperties2(engine->chosenGPU, depthFormatList[i], &props);

        if (props.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
        engine->depthFormat = depthFormatList[i];
        const char* name = (depthFormatList[i] == VK_FORMAT_D32_SFLOAT) ? "D32_SFLOAT" :
        (depthFormatList[i] == VK_FORMAT_D24_UNORM_S8_UINT) ? "D24_UNORM_S8_UINT" : "D32_SFLOAT_S8_UINT";
        
        SDL_Log("Selected depth format: %s", name);
        break;
        }
    }

    SDL_Log("Selected depth format: %d", (int)engine->depthFormat);

    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        engine->swapchainImages[i] = images[i];

        // Create image view for each swapchain image
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            //.format     = depthFormat,
            .format = engine->swapchainImageFormat,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1}};

        VK_CHECK(vkCreateImageView(engine->device, &viewInfo, NULL,
                                &engine->swapchainImageViews[i]));
    }

    SDL_Log("Swapchain created successfully with %u images", swapchainImageCount);
}

void destroy_swapchain(struct VulkanEngine *engine) {
  vkDestroySwapchainKHR(engine->device, engine->swapchain, 0);

  for (uint32_t i = 0; i < engine->swapchainImageCount; ++i)
    vkDestroyImageView(engine->device, engine->swapchainImageViews[i], 0);
}

void howtoCleanupVulkanEngine(struct VulkanEngine *engine)
{
    if (!engine || !engine->device) return;

    VK_CHECK(vkDeviceWaitIdle(engine->device));

    // 1. Per-frame resources
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        if (engine->frames[i].renderFence)
            vkDestroyFence(engine->device, engine->frames[i].renderFence, NULL);

        if (engine->frames[i].swapchainSemaphore)
            vkDestroySemaphore(engine->device, engine->frames[i].swapchainSemaphore, NULL);

        if (engine->frames[i].renderSemaphore)
            vkDestroySemaphore(engine->device, engine->frames[i].renderSemaphore, NULL);

        // Shader data buffer
        if (engine->frames[i].shaderDataBuffers.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(engine->device, 
                engine->frames[i].shaderDataBuffers.buffer, 
                NULL);
            engine->frames[i].shaderDataBuffers.buffer = VK_NULL_HANDLE;
        }
    }

    // 2. Command pool (destroys all command buffers)
    if (engine->commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(engine->device, engine->commandPool, NULL);
        engine->commandPool = VK_NULL_HANDLE;
    }

    // 3. Pipeline objects
    if (engine->graphicsPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(engine->device, engine->graphicsPipeline, NULL);

    if (engine->pipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(engine->device, engine->pipelineLayout, NULL);

    // 4. Descriptors
    if (engine->descriptorSetLayoutTex != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(engine->device, engine->descriptorSetLayoutTex, NULL);

    if (engine->descriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(engine->device, engine->descriptorPool, NULL);

    // 5. Main geometry buffer
    if (engine->vBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(engine->device, engine->vBuffer, NULL);

        engine->vBuffer = VK_NULL_HANDLE;
    }

    // 6. Textures
    for (uint32_t i = 0; i < engine->textureCount; i++)
    {
        destroy_texture(engine, &engine->textures[i]);
        engine->textureCount = 0;
        /*
        if (engine->textures[i].view != VK_NULL_HANDLE)
            vkDestroyImageView(engine->device, engine->textures[i].view, NULL);

        if (engine->textures[i].sampler != VK_NULL_HANDLE)
            vkDestroySampler(engine->device, engine->textures[i].sampler, NULL);

        if (engine->textures[i].image != VK_NULL_HANDLE)
            destroy_texture(engine, engine->textures[i].image);
            */
    }

    // 7. Offscreen images
    if (engine->drawImage.image != VK_NULL_HANDLE)
        destroy_image(engine, &engine->drawImage);

    if (engine->depthImage.image!= VK_NULL_HANDLE)
        destroy_image(engine, &engine->depthImage);


    // 8. Swapchain
    destroy_swapchain(engine);

    // 9. Final objects
    if (engine->surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(engine->instance, engine->surface, NULL);

    if (engine->device != VK_NULL_HANDLE)
        vkDestroyDevice(engine->device, NULL);

    if (engine->instance != VK_NULL_HANDLE)
        vkDestroyInstance(engine->instance, NULL);

    if (engine->window)
        SDL_DestroyWindow(engine->window);

    SDL_Quit();

    if (&engine->mainDeletionQueue)
        deletion_queue_destroy(&engine->mainDeletionQueue);

}

void runVulkanEngine(struct VulkanEngine *engine, GameState *gamestate) {
  SDL_Event e;
  bool bQuit = false;

  // main loop
  while (!bQuit) {
    while (SDL_PollEvent(&e) != 0) {
      // close the window when user alt-f4s or clicks the X button
      if (e.type == SDL_EVENT_QUIT)
        bQuit = true;

      if (e.type == SDL_EVENT_KEY_DOWN)
        if (e.key.scancode == SDL_SCANCODE_ESCAPE)
          bQuit = true;

      if (e.type == SDL_EVENT_WINDOW_MINIMIZED)
        engine->stop_rendering = true;

      if (e.type == SDL_EVENT_WINDOW_RESTORED)
        engine->stop_rendering = false;

      // send SDL event to camera and imgui for handling
      processSDLEvent(&e, &engine->mainCamera);
      //ImGui_ImplSDL3_ProcessEvent(&e);
    }

    // do not draw if we are minimized
    if (engine->stop_rendering) {
      // throttle the speed to avoid the endless spinning
      SDL_Delay(5000);
      continue;
    }

    if (engine->resize_requested)
      resize_swapchain(engine);

    // imgui new frame
    //ImGui_ImplVulkan_NewFrame();
    //ImGui_ImplSDL3_NewFrame();
    //ImGui::NewFrame();

    // make imgui calculate internal draw structures
    //ImGui::Render();

    mainRenderLoop(engine, gamestate);
  }
}

struct FrameData *getCurrentFrame(struct VulkanEngine *engine) {
  return &engine->frames[engine->frameIndex % FRAME_OVERLAP];
}

VertexInputDescription get_vertex_description()
{
    VertexInputDescription description = {0};

    // === Binding 0 ===
    description.bindings[0] = (VkVertexInputBindingDescription){
        .binding   = 0,
        .stride    = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    description.bindingCount = 1;

    // === Attributes ===
    uint32_t loc = 0;

    // Location 0: Position
    description.attributes[loc++] = (VkVertexInputAttributeDescription){
        .location = 0,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = offsetof(Vertex, position)
    };

    // Location 1: Normal
    description.attributes[loc++] = (VkVertexInputAttributeDescription){
        .location = 1,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = offsetof(Vertex, normal)
    };

    // Location 2: Texcoord
    description.attributes[loc++] = (VkVertexInputAttributeDescription){
        .location = 2,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32_SFLOAT,
        .offset   = offsetof(Vertex, texcoord)
    };

    // Location 3: Color
    description.attributes[loc++] = (VkVertexInputAttributeDescription){
        .location = 3,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset   = offsetof(Vertex, color)
    };

    // Location 4: Joints (uint8x4)
    description.attributes[loc++] = (VkVertexInputAttributeDescription){
        .location = 4,
        .binding  = 0,
        .format   = VK_FORMAT_R8G8B8A8_UINT,
        .offset   = offsetof(Vertex, joints)
    };

    // Location 5: Weights
    description.attributes[loc++] = (VkVertexInputAttributeDescription){
        .location = 5,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset   = offsetof(Vertex, weights)
    };

    description.attributeCount = loc;

    return description;
}

void
mainRenderLoop(struct VulkanEngine *engine, GameState *gameState)
{
    // Get current frame
    //struct FrameData *currentFrame = &engine->frames[engine->frameIndex];
    struct FrameData *currentFrame = getCurrentFrame(engine);

    // 1. Update all shader data (CPU → GPU)
    update_shader_data(engine, gameState);

    // 2. Prepare frame (fence, acquire image, etc.)
    begin_frame(engine);

    // Early out if resize was requested
    if (engine->resize_requested) return;

    VkCommandBuffer cmd = currentFrame->commandBuffer;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // 3. Start rendering
    begin_rendering(engine, cmd);

    // 4. Draw 3D content
    draw_3d_scene(engine, gameState, cmd);

    // 5. Draw UI / Text
    draw_ui_text(engine, cmd);

    // 6. Finish rendering
    end_rendering(engine, cmd);

    // 7. Submit and Present
    submit_and_present(engine);

    // Advance frame
    engine->frameIndex = (engine->frameIndex + 1) % FRAME_OVERLAP;
}

bool init_mezzanine(struct VulkanEngine *engine, GameState *gameState)
{
    load_and_upload_gltf_models(engine, gameState);
 
    create_per_frame_uniform_buffers(engine);

    create_main_3d_descriptor_layout_and_set(engine);

    create_main_graphics_pipeline(engine);

    setup_font_atlas_and_text_pipeline(engine);

    return true;
}

void
resize_swapchain(struct VulkanEngine *engine)
{
    if (engine->windowExtent.width == 0 || engine->windowExtent.height == 0)
        return;   // minimized, don't resize

    vkDeviceWaitIdle(engine->device);

    destroy_swapchain(engine);

        // Destroy offscreen images
    if (engine->drawImage.image != VK_NULL_HANDLE)
        destroy_image(engine, &engine->drawImage);

    if (engine->depthImage.image != VK_NULL_HANDLE)
        destroy_image(engine, &engine->depthImage);

    // Get new window size
    int w, h;
    SDL_GetWindowSize(engine->window, &w, &h);
    engine->windowExtent.width = (uint32_t)w;
    engine->windowExtent.height = (uint32_t)h;

    // Recreate swapchain
    create_swapchain(engine, engine->windowExtent.width, engine->windowExtent.height);

    // Recreate offscreen render targets (drawImage + depthImage)
    VkExtent3D newExtent = { engine->windowExtent.width, engine->windowExtent.height, 1 };

    // Recreate drawImage (HDR color target)
    engine->drawImage = create_image(engine, newExtent, 
                                     VK_FORMAT_R16G16B16A16_SFLOAT,
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
                                     VK_IMAGE_USAGE_STORAGE_BIT | 
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
                                     false);

    // Recreate depth image
    engine->depthImage = create_image(engine, newExtent, 
                                      engine->depthFormat,
                                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
                                      false);

    engine->resize_requested = false;

    SDL_Log("Swapchain and render targets resized to %ux%u", w, h);
}

AllocatedImage create_image(struct VulkanEngine* engine, 
                            VkExtent3D size,
                            VkFormat format, 
                            VkImageUsageFlags usage,
                            bool mipmapped)
{
    AllocatedImage img = {0};

    img.imageFormat = format;
    img.imageExtent = size;

    // Create Image
    VkImageCreateInfo img_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = size,
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (mipmapped)
    {
        uint32_t maxDimension = MAX(size.width, size.height);
        if (maxDimension > 0)
        {
            double logValue = log2((double)maxDimension);
            img_info.mipLevels = (uint32_t)floor(logValue) + 1;
        }
    }

    VK_CHECK(vkCreateImage(engine->device, &img_info, NULL, &img.image));

    // Get memory requirements
    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(engine->device, img.image, &reqs);

    // Allocate from arena
    Allocation alloc = arena_alloc(&engine->deviceLocalArena, reqs.size, reqs.alignment);

    if (!allocation_valid(alloc))
    {
        SDL_Log("ERROR: Out of memory in deviceLocalArena for image!");
        abort();
    }

    VK_CHECK(vkBindImageMemory(engine->device, img.image, alloc.memory, alloc.offset));
    img.offset = alloc.offset;

    // === Create Image View (Pure C) ===
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = img.image,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .components       = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask     = aspectFlag,
            .baseMipLevel   = 0,
            .levelCount     = img_info.mipLevels,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    VK_CHECK(vkCreateImageView(engine->device, &view_info, NULL, &img.imageView));

    // Push to deletion queue
    deletion_queue_push_image(&engine->mainDeletionQueue, 
                              engine->device, 
                              img.imageView, 
                              img.image);

    return img;
}

void destroy_image(struct VulkanEngine *engine, AllocatedImage *img)
{
    if (img == NULL) return;

    if (img->imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(engine->device, img->imageView, NULL);
        img->imageView = VK_NULL_HANDLE;
    }

    if (img->image != VK_NULL_HANDLE)
    {
        vkDestroyImage(engine->device, img->image, NULL);
        img->image = VK_NULL_HANDLE;
    }

    // IMPORTANT: Do NOT free memory here!
    // The memory stays in the arena and is cleaned up at the end
    img->offset = 0;
}

void RenderText(struct VulkanEngine* engine, VkCommandBuffer cmd, FontAtlas* atlas,
                const char* text, float screenX, float screenY,
                float red, float green, float blue)
{
    if (!text || !atlas || atlas->atlasWidth == 0 || !engine->textPipeline) {
        return;
    }

    int len = 0;
    while (text[len]) ++len;
    if (len == 0) return;

    TextVertex* verts = (TextVertex*)alloca(len * 6 * sizeof(TextVertex));
    int vertCount = 0;

    float cursorX = floorf(screenX);
    float cursorY = floorf(screenY);

    for (int i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 127) {
            cursorX += 14.0f;
            continue;
        }

        Glyph* g = &atlas->glyphs[c];

        float charX = cursorX + g->xoff;
        float charY = cursorY - atlas->baseline * 0.7f + g->yoff;

        float x0 = charX, y0 = charY;
        float x1 = charX + (g->xoff2 - g->xoff);
        float y1 = charY + (g->yoff2 - g->yoff);

        float ndc_x0 = (x0 / engine->windowExtent.width)  * 2.0f - 1.0f;
        float ndc_y0 = 1.0f - (y0 / engine->windowExtent.height) * 2.0f;
        float ndc_x1 = (x1 / engine->windowExtent.width)  * 2.0f - 1.0f;
        float ndc_y1 = 1.0f - (y1 / engine->windowExtent.height) * 2.0f;

        float u0 = g->u0, v0 = g->v1;
        float u1 = g->u1, v1 = g->v0;

        verts[vertCount++] = (TextVertex){{ndc_x0, ndc_y0}, {u0, v0}};
        verts[vertCount++] = (TextVertex){{ndc_x1, ndc_y0}, {u1, v0}};
        verts[vertCount++] = (TextVertex){{ndc_x0, ndc_y1}, {u0, v1}};

        verts[vertCount++] = (TextVertex){{ndc_x1, ndc_y0}, {u1, v0}};
        verts[vertCount++] = (TextVertex){{ndc_x1, ndc_y1}, {u1, v1}};
        verts[vertCount++] = (TextVertex){{ndc_x0, ndc_y1}, {u0, v1}};

        cursorX += g->width + 6.0f;
    }

    if (vertCount == 0) return;

    // Push color
    struct PushColor push = { red, green, blue, 1.0f };
    vkCmdPushConstants(cmd, engine->textPipelineLayout, 
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

    // ====================== STAGING BUFFER (using your arena) ======================
    if (engine->textStagingBuffer == VK_NULL_HANDLE)
    {
        VkDeviceSize bufferSize = 1024 * 6 * sizeof(TextVertex); // ~1000 characters

        VkBufferCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = bufferSize,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };

        VK_CHECK(vkCreateBuffer(engine->device, &ci, NULL, &engine->textStagingBuffer));

        Allocation alloc = arena_alloc(&engine->stagingArena, bufferSize, 16);
        if (!allocation_valid(alloc)) {
            SDL_Log("ERROR: Out of staging memory for text!");
            return;
        }

        VK_CHECK(vkBindBufferMemory(engine->device, engine->textStagingBuffer, 
                                    alloc.memory, alloc.offset));

        engine->textStagingOffset = alloc.offset;
    }

    // Append vertices directly into persistently mapped arena
    void* mapped = (char*)engine->stagingArena.mapped + engine->textStagingOffset;
    TextVertex* dst = (TextVertex*)mapped + engine->textVertexCountThisFrame;

    memcpy(dst, verts, vertCount * sizeof(TextVertex));

    // Draw
    VkDeviceSize offset = (engine->textVertexCountThisFrame * sizeof(TextVertex)) 
                        + engine->textStagingOffset;

    vkCmdBindVertexBuffers(cmd, 0, 1, &engine->textStagingBuffer, &offset);
    vkCmdDraw(cmd, vertCount, 1, 0, 0);

    // Update count for next text this frame
    engine->textVertexCountThisFrame += vertCount;
}

bool
load_and_upload_gltf_models(struct VulkanEngine *engine, GameState *gameState)
{
    gameState->model = load_gltf_model(gameState->arena, "../data/models/arwin8.glb");

    if (gameState->model.mesh.vertCount == 0) {
        SDL_Log("Failed to load glTF model!");
        return false;
    }

    gameState->room = load_gltf_model(gameState->arena, "../data/rooms/room05.glb");

    if (gameState->room.mesh.vertCount == 0) {
        SDL_Log("Failed to load glTF room!");
        return false;
    }

    // Upload to Vulkan GPU buffers
    if (!upload_model_to_gpu(engine, &gameState->model)) {
        SDL_Log("Failed to upload glTF model mesh to GPU");
        return false;
    }

    SDL_Log("Successfully loaded and uploaded glTF model with %d vertices, %d indices", 
            gameState->model.mesh.vertCount, gameState->model.mesh.triCount);

    // Upload to Vulkan GPU buffers
    if (!upload_model_to_gpu(engine, &gameState->room)) {
        SDL_Log("Failed to upload glTF room mesh to GPU");
        return false;
    }

    SDL_Log("Successfully loaded and uploaded glTF room with %d vertices, %d indices", 
            gameState->room.mesh.vertCount, gameState->room.mesh.triCount);

    return true;
}

bool create_per_frame_uniform_buffers(struct VulkanEngine *engine)
{
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) 
    {
        // Create the buffer object
        VkBufferCreateInfo uBufferCI = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = sizeof(struct ShaderData),
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        };

        VK_CHECK(vkCreateBuffer(engine->device, &uBufferCI, NULL,
                                &engine->frames[i].shaderDataBuffers.buffer));

        // Get memory requirements
        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(engine->device, 
                                      engine->frames[i].shaderDataBuffers.buffer, 
                                      &reqs);

        // Use staging arena (host visible + coherent)
        Allocation alloc = arena_alloc(&engine->stagingArena, reqs.size, reqs.alignment);

        if (!allocation_valid(alloc)) {
            SDL_Log("ERROR: Out of staging memory for uniform buffer!");
            return false;
        }

        VK_CHECK(vkBindBufferMemory(engine->device, 
                                    engine->frames[i].shaderDataBuffers.buffer,
                                    alloc.memory, 
                                    alloc.offset));

        // Store offset for later use
        engine->frames[i].shaderDataBuffers.offset = alloc.offset;

        // Get device address (needed for shader)
        VkBufferDeviceAddressInfo uBufferBdaInfo = {
            .sType   = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer  = engine->frames[i].shaderDataBuffers.buffer
        };

        engine->frames[i].shaderDataBuffers.deviceAddress = 
            vkGetBufferDeviceAddress(engine->device, &uBufferBdaInfo);
    }

    // ====================== Semaphores & Fences ======================
    VkSemaphoreCreateInfo semaphoreCI = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceCI = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) 
    {
        VK_CHECK(vkCreateFence(engine->device, &fenceCI, NULL, 
                               &engine->frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCI, NULL, 
                                   &engine->frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCI, NULL, 
                                   &engine->frames[i].renderSemaphore));
    }

    for (uint32_t i = 0; i < engine->swapchainImageCount; i++)
    {
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCI, NULL, 
                                   &engine->renderSemaphores[i]));
    }

    // ====================== Command Pool ======================
    VkCommandPoolCreateInfo commandPoolCI = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = engine->graphicsQueueFamily
    };

    VK_CHECK(vkCreateCommandPool(engine->device, &commandPoolCI, NULL, 
                                 &engine->commandPool));

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) 
    {
        VkCommandBufferAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = engine->commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VK_CHECK(vkAllocateCommandBuffers(engine->device, &allocInfo, 
                                          &engine->frames[i].commandBuffer));
    }

    SDL_Log("Per-frame uniform buffers and sync objects created");
    return true;
}

bool
create_main_3d_descriptor_layout_and_set(struct VulkanEngine *engine)
{
    // glb files do not have textures
    engine->textureCount = 0;

    VkDescriptorSetLayoutBinding bindings[1] = {};

    // Binding 0: Textures array
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;


    // We no longer need variable descriptor count flags for binding 0 in this simple case
    VkDescriptorSetLayoutCreateInfo descLayoutCI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &descLayoutCI, NULL, &engine->descriptorSetLayoutTex));


    // === Descriptor Pool (must support both types) ===
    VkDescriptorPoolSize poolSizes[1] = {};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = FRAME_OVERLAP * 2;

    VkDescriptorPoolCreateInfo descPoolCI = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = FRAME_OVERLAP * 4,
        .poolSizeCount = 1,
        .pPoolSizes = poolSizes
    };

    VK_CHECK(vkCreateDescriptorPool(engine->device, &descPoolCI, NULL, &engine->descriptorPool));


    for(uint32_t i = 0; i < FRAME_OVERLAP; i++)
    {
        // === Allocate Descriptor Set ===
        VkDescriptorSetAllocateInfo texDescSetAlloc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = engine->descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &engine->descriptorSetLayoutTex
        };

        VK_CHECK(vkAllocateDescriptorSets(engine->device, &texDescSetAlloc, &engine->frames[i].descriptorSet));
    }

    for(uint32_t i = 0; i < FRAME_OVERLAP; i++)
    {
        // New write for shaderData (binding 0)
        VkDescriptorBufferInfo shaderDataBufferInfo = {
            .buffer = engine->frames[i].shaderDataBuffers.buffer,   // use one from current frame
            .offset = 0,
            .range = sizeof(struct ShaderData)
        };

        VkWriteDescriptorSet writeShaderData = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = engine->frames[i].descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &shaderDataBufferInfo
        };

        vkUpdateDescriptorSets(engine->device, 1, &writeShaderData, 0, NULL);
    }

    // Loading shaders
    //Slang::ComPtr<slang::IGlobalSession> globalSession;


    /*
    engine->slangGlobalSession = {};
    slang::createGlobalSession(engine->slangGlobalSession.writeRef());
    if (!engine->slangGlobalSession) {
    SDL_Log("Failed to create slangGlobalSession");
    }

    slang::TargetDesc target = {
        .format = SLANG_SPIRV,
        .profile = engine->slangGlobalSession->findProfile("spirv_1_6")
    };

    slang::CompilerOptionEntry option = {
        .name = slang::CompilerOptionName::EmitSpirvDirectly,
        .value = {
        .kind          = slang::CompilerOptionValueKind::Int,
        .intValue0     = 1,      // 1 = enable
        .intValue1     = 0,
        .stringValue0  = NULL,
        .stringValue1  = NULL
    }};

    slang::SessionDesc sessionDesc{
        .targets = &target,
        .targetCount = 1,
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries = &option,
        .compilerOptionEntryCount = 1
    };

    Slang::ComPtr<slang::ISession> slangSession;
    engine->slangGlobalSession->createSession(sessionDesc, slangSession.writeRef());

    Slang::ComPtr<ISlangBlob> diagnosticsBlob;

    SDL_Log("Compiling shader from source: shader.slang");
    */

    // Read the file ourselves so it always gets the latest version
    size_t sourceSize = 0;
    void* sourceData = SDL_LoadFile("../data/assets/shader.slang", &sourceSize);
    if (!sourceData)
    {
        SDL_Log("Failed to read shader.slang: %s", SDL_GetError());
        abort();
    }

    /*
    Slang::ComPtr<slang::IModule> slangModule; 
    {
        Slang::ComPtr<ISlangBlob> diagnosticsBlob;

        // this will recompile the shader code into a slang modeul
        slangModule = slangSession->loadModuleFromSourceString(
            "shader",                                 // module name (can be anything)
            "../data/assets/shader.slang",            // "path" for diagnostics / error messages
            (const char*)sourceData,                  // source code
            diagnosticsBlob.writeRef());

        if (!slangModule)
        {
            const char* diagMsg = diagnosticsBlob 
                ? (const char*)diagnosticsBlob->getBufferPointer() 
                : "No diagnostics";
            SDL_Log("ERROR: Failed to compile shader.slang");
            SDL_Log("Diagnostics: %s", diagMsg);
            abort();
        }
    }

    SDL_free(sourceData);   // important: free SDL's allocation

    SDL_Log("Shader compiled successfully");

    // Get SPIR-V
    Slang::ComPtr<ISlangBlob> spirv;
    slangModule->getTargetCode(0, spirv.writeRef());
    */

    // Create Vulkan shader module (rest of your code stays the same)
    VkShaderModuleCreateInfo shaderModuleCI = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sourceSize,
        .pCode = sourceData
    };

    VK_CHECK(vkCreateShaderModule(engine->device, &shaderModuleCI, NULL, &engine->shaderModule));

    SDL_free(sourceData);

    SDL_Log("Vulkan shader module created successfully");

    return true;
}

bool
create_main_graphics_pipeline(struct VulkanEngine *engine)
{
    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset     = 0,
        .size =       sizeof(float) * 4
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &engine->descriptorSetLayoutTex,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange 
    };
    VK_CHECK(vkCreatePipelineLayout(engine->device, &pipelineLayoutCI, NULL, &engine->pipelineLayout));


    VkVertexInputBindingDescription vertexBinding = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    uint32_t attributeCount = 6;

    VkVertexInputAttributeDescription *vertexAttributes = (VkVertexInputAttributeDescription*)arenaAlloc(engine->arena, attributeCount
        * sizeof(VkVertexInputAttributeDescription));

    // Location 0: Position
    vertexAttributes[0] = (VkVertexInputAttributeDescription){
        .location = 0,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = offsetof(Vertex, position)
    };

    // Location 1: Normal
    vertexAttributes[1] = (VkVertexInputAttributeDescription){
        .location = 1,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32_SFLOAT,
        .offset   = offsetof(Vertex, normal)
    };

    // Location 2: Texcoord
    vertexAttributes[2] = (VkVertexInputAttributeDescription){
        .location = 2,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32_SFLOAT,
        .offset   = offsetof(Vertex, texcoord)
    };

    // Location 3: Color
    vertexAttributes[3] = (VkVertexInputAttributeDescription){
        .location = 3,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset   = offsetof(Vertex, color)
    };

    // Location 4: Joints (uint8_t[4])
    vertexAttributes[4] = (VkVertexInputAttributeDescription){
        .location = 4,
        .binding  = 0,
        .format   = VK_FORMAT_R8G8B8A8_UINT,
        .offset   = offsetof(Vertex, joints)
    };

    // Location 5: Weights
    vertexAttributes[5] = (VkVertexInputAttributeDescription){
        .location = 5,
        .binding  = 0,
        .format   = VK_FORMAT_R32G32B32A32_SFLOAT,
        .offset   = offsetof(Vertex, weights)
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBinding,
        .vertexAttributeDescriptionCount = attributeCount,
        .pVertexAttributeDescriptions = vertexAttributes,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    // Entry points in the shaders
    VkPipelineShaderStageCreateInfo shaderStages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = engine->shaderModule, .pName = "VSMain"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = engine->shaderModule, .pName = "PSMain"
        }
    };

    // configure viewport state
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };
    
    VkDynamicState dynamicStates[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ArraySize(dynamicStates),
        .pDynamicStates = dynamicStates
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    };

    VkPipelineRenderingCreateInfo renderingCI = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &engine->swapchainImageFormat,
        .depthAttachmentFormat = engine->depthFormat
    };

    VkPipelineColorBlendAttachmentState blendAttachment = {
        .colorWriteMask = 0xF
    };
    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment
    };
    VkPipelineRasterizationStateCreateInfo rasterizationState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,      // ← Disable culling for testing
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampleState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkGraphicsPipelineCreateInfo pipelineCI = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .stageCount = ArraySize(shaderStages),
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = engine->pipelineLayout
    };
    VK_CHECK(vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, 1, &pipelineCI, NULL, &engine->graphicsPipeline));

    SDL_Log("Main 3D pipeline created successfully");

    return true;
}

bool
setup_font_atlas_and_text_pipeline(struct VulkanEngine *engine)
{
    if(!LoadFontAtlas(engine, &engine->fontAtlas))
        SDL_Log("ERROR: LoadFontAtlas failed completely!");

    SDL_Log("=== After LoadFontAtlas ===");
    SDL_Log("atlas->image      = %p", (void*)engine->fontAtlas.image);
    SDL_Log("atlas->imageView  = %p", (void*)engine->fontAtlas.imageView);
    SDL_Log("atlas->sampler    = %p", (void*)engine->fontAtlas.sampler);
    SDL_Log("atlasWidth=%d, atlasHeight=%d", engine->fontAtlas.atlasWidth, engine->fontAtlas.atlasHeight);

    // At this point these MUST be valid:
    SDL_Log("Font atlas ready - ImageView: %p, Sampler: %p", 
            (void*)engine->fontAtlas.imageView, (void*)engine->fontAtlas.sampler);

    // Create text-specific descriptor layout
    if (!create_text_descriptor_layout(engine)) {
        SDL_Log("Failed to create text descriptor layout");
        return false;
    }

    if(!update_text_descriptors(engine))
    {
        SDL_Log("Failed to create text descriptor layout");
        return false;
    }

    transition_font_atlas(engine);

    // Then create the text pipeline
    if (!create_text_pipeline(engine)) {
        SDL_Log("Failed to create text pipeline");
    }
    // === TEXT PIPELINE DEBUG ===
    SDL_Log("=== TEXT PIPELINE DEBUG ===");
    SDL_Log("textPipeline          = %p", (void*)engine->textPipeline);
    SDL_Log("textPipelineLayout    = %p", (void*)engine->textPipelineLayout);
    SDL_Log("textDescriptorSet     = %p", (void*)engine->textDescriptorSet);
    SDL_Log("textDescriptorSetLayout = %p", (void*)engine->textDescriptorSetLayout);
    SDL_Log("textShaderModule      = %p", (void*)engine->textShaderModule);
    SDL_Log("fontAtlas.imageView   = %p", (void*)engine->fontAtlas.imageView);
    SDL_Log("fontAtlas.sampler     = %p", (void*)engine->fontAtlas.sampler);
    SDL_Log("fontAtlas.atlasWidth  = %d", engine->fontAtlas.atlasWidth);
    SDL_Log("swapchainImageFormat  = %d", (int)engine->swapchainImageFormat);
    SDL_Log("depthFormat           = %d", (int)engine->depthFormat);

    return true;
}

void update_shader_data(struct VulkanEngine *engine, GameState *gameState)
{
    struct FrameData *currentFrame = getCurrentFrame(engine);

    // === Materials ===
    int numMaterials = gameState->model.mesh.materialCount;
    for (int m = 0; m < numMaterials && m < 8; ++m)
    {
        currentFrame->shaderData.baseColorFactor[m] = 
            gameState->model.mesh.materials[m].baseColorFactor;
    }

    // === Light ===
    currentFrame->shaderData.lightPos = HMM_V4(5.0f, 10.0f, 10.0f, 1.0f);

    // === Skinning ===
    int jointCount = gameState->model.pose.jointCount;
    for (int j = 0; j < jointCount && j < 64; ++j)
    {
        currentFrame->shaderData.skinMatrices[j] = gameState->model.pose.skinMatrices[j];
    }
    for (int j = jointCount; j < 64; ++j)
    {
        currentFrame->shaderData.skinMatrices[j] = HMM_M4D(1.0f);
    }

    // === Camera + Projection ===
    float aspect = (float)engine->windowExtent.width / (float)engine->windowExtent.height;

    HMM_Mat4 proj = HMM_Perspective_RH_ZO(HMM_AngleDeg(60.0f), aspect, 0.1f, 1000.0f);
    proj.Elements[1][1] *= -1.0f;        // Vulkan Y flip

    HMM_Mat4 view = HMM_LookAt_RH(
        HMM_V3(0.0f, 3.0f, 10.0f),
        HMM_V3(0.0f, 0.0f, 0.0f),
        HMM_V3(0.0f, 1.0f, 0.0f)
    );

    // === Model matrix ===
    HMM_Mat4 modelMat = HMM_MulM4(
        HMM_Translate(HMM_V3(0.0f, -5.0f, 0.0f)),
        HMM_Scale(HMM_V3(5.0f, 5.0f, 5.0f))
    );

    currentFrame->shaderData.projection = proj;
    currentFrame->shaderData.view       = view;
    currentFrame->shaderData.model      = modelMat;

    // ====================== UPLOAD TO GPU (Arena) ======================
    // Since we use the staging arena which is persistently mapped
    void* mapped = (char*)engine->stagingArena.mapped 
                 + currentFrame->shaderDataBuffers.offset;

    memcpy(mapped, &currentFrame->shaderData, sizeof(struct ShaderData));
}

void begin_frame(struct VulkanEngine *engine)
{
    struct FrameData *currentFrame = getCurrentFrame(engine);   // ← Use pointer, not reference

    // =============================================
    // 1. Reset per-frame text counter
    // =============================================
    engine->textVertexCountThisFrame = 0;

    // Note: We no longer destroy a per-frame staging buffer here.
    // Our stagingArena is persistent and reused across frames.

    // =============================================
    // 2. Wait for GPU to finish with this frame
    // =============================================
    VK_CHECK(vkWaitForFences(engine->device, 
                             1, 
                             &currentFrame->renderFence, 
                             VK_TRUE, 
                             UINT64_MAX));

    VK_CHECK(vkResetFences(engine->device, 
                           1, 
                           &currentFrame->renderFence));

    // =============================================
    // 3. Acquire next swapchain image
    // =============================================
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        engine->device,
        engine->swapchain,
        UINT64_MAX,
        currentFrame->swapchainSemaphore,
        VK_NULL_HANDLE,
        &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        engine->resize_requested = true;
        return;
    }
    else if (result != VK_SUCCESS)
    {
        SDL_Log("vkAcquireNextImageKHR failed: %d", result);
        engine->resize_requested = true;
        return;
    }

    engine->imageIndex = imageIndex;
}

void begin_rendering(struct VulkanEngine *engine, VkCommandBuffer cmd)
{
    struct FrameData *currentFrame = getCurrentFrame(engine);   // ← Fixed

    // === 1. Pipeline Barriers (Swapchain + Depth Image) ===
    VkImageMemoryBarrier2 outputBarriers[2] = {
        // Swapchain image barrier
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = engine->swapchainImages[engine->imageIndex],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1
            }
        },
        // Depth image barrier
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .image = engine->depthImage.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1
            }
        }
    };

    VkDependencyInfo barrierDependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = outputBarriers
    };

    vkCmdPipelineBarrier2(cmd, &barrierDependencyInfo);

    // === 2. Rendering Attachments ===
    VkRenderingAttachmentInfo colorAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = engine->swapchainImageViews[engine->imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { 0.0f, 0.0f, 0.2f, 1.0f } }
    };

    VkRenderingAttachmentInfo depthAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = engine->depthImage.imageView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { .depthStencil = { 1.0f, 0 } }
    };

    // === 3. Begin Rendering ===
    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = engine->windowExtent
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo
    };

    vkCmdBeginRendering(cmd, &renderingInfo);

    // === 4. Viewport and Scissor ===
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)engine->windowExtent.width,
        .height = (float)engine->windowExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = engine->windowExtent
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // === 5. Bind Graphics Pipeline and Descriptor Set ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->graphicsPipeline);

    vkCmdBindDescriptorSets(cmd, 
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            engine->pipelineLayout,
                            0,
                            1,
                            &currentFrame->descriptorSet,   // ← note the ->
                            0, NULL);
}

void
draw_3d_scene(struct VulkanEngine *engine, GameState *gameState, VkCommandBuffer cmd)
{
    VkDeviceSize offset = 0;

    // ======================
    // 1. Draw the ROOM
    // ======================
    if (gameState->room.mesh.vertCount > 0)
    {
        // Bind room geometry
        vkCmdBindVertexBuffers(cmd, 0, 1, &gameState->room.vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd, 
                             gameState->room.vertexBuffer,
                             gameState->room.indexBufferOffset,
                             VK_INDEX_TYPE_UINT32);

        // Draw each primitive/submesh
        for (int i = 0; i < gameState->room.mesh.primitiveCount; ++i)
        {
            Primitive* prim = &gameState->room.mesh.primitives[i];

            // Convert packed color to float
            float r = ((prim->color >>  0) & 0xFF) / 255.0f;
            float g = ((prim->color >>  8) & 0xFF) / 255.0f;
            float b = ((prim->color >> 16) & 0xFF) / 255.0f;

            struct PushColor {
                float r, g, b, a;
            } push = { r, g, b, 1.0f };

            vkCmdPushConstants(cmd,
                               engine->pipelineLayout,
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(push),
                               &push);

            vkCmdDrawIndexed(cmd,
                             prim->triCount * 3,   // index count
                             1,                    // instance count
                             prim->triOffset * 3,  // first index
                             0,                    // vertex offset
                             0);                   // first instance
        }
    }

    // ======================
    // 2. Draw the CHARACTER / MODEL
    // ======================
    if (gameState->model.mesh.vertCount > 0)
    {
        // Bind model geometry
        vkCmdBindVertexBuffers(cmd, 0, 1, &gameState->model.vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd,
                             gameState->model.vertexBuffer,
                             gameState->model.indexBufferOffset,
                             VK_INDEX_TYPE_UINT32);

        // Draw each primitive/submesh
        for (int i = 0; i < gameState->model.mesh.primitiveCount; ++i)
        {
            Primitive* prim = &gameState->model.mesh.primitives[i];

            float r = ((prim->color >>  0) & 0xFF) / 255.0f;
            float g = ((prim->color >>  8) & 0xFF) / 255.0f;
            float b = ((prim->color >> 16) & 0xFF) / 255.0f;

            struct PushColor {
                float r, g, b, a;
            } push = { r, g, b, 1.0f };

            vkCmdPushConstants(cmd,
                               engine->pipelineLayout,
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(push),
                               &push);

            vkCmdDrawIndexed(cmd,
                             prim->triCount * 3,
                             1,
                             prim->triOffset * 3,
                             0,
                             0);
        }
    }
}

void
draw_ui_text(struct VulkanEngine *engine, VkCommandBuffer cmd)
{
    vkCmdSetDepthTestEnable(cmd, VK_FALSE);
    vkCmdSetDepthWriteEnable(cmd, VK_FALSE);

    // Bind text pipeline + descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->textPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            engine->textPipelineLayout, 0, 1,
                            &engine->textDescriptorSet, 0, NULL);

    RenderText(engine, cmd, &engine->fontAtlas, "HELLO", 100.0f, 280.0f, 1.0f, 1.0f, 1.0f);
    RenderText(engine, cmd, &engine->fontAtlas, "Hello world", 100.0f, 340.0f, 1.0f, 1.0f, 1.0f);

    // Re-enable depth for next frame (good practice)
    vkCmdSetDepthTestEnable(cmd, VK_TRUE);
    vkCmdSetDepthWriteEnable(cmd, VK_TRUE);
}

void
end_rendering(struct VulkanEngine *engine, VkCommandBuffer cmd)
{
    // End the dynamic rendering
    vkCmdEndRendering(cmd);

    // Transition swapchain image from COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR
    VkImageMemoryBarrier2 presentBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        
        .srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        
        .dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = 0,
        
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // or ATTACHMENT_OPTIMAL
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        
        .image = engine->swapchainImages[engine->imageIndex],
        
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &presentBarrier
    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void
submit_and_present(struct VulkanEngine *engine)
{
    struct FrameData *currentFrame = getCurrentFrame(engine);
    VkCommandBuffer cmd = currentFrame->commandBuffer;

    // End command buffer recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit to queue
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &currentFrame->swapchainSemaphore,
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &engine->renderSemaphores[engine->imageIndex]
    };

    VK_CHECK(vkQueueSubmit(engine->graphicsQueue, 1, &submitInfo, currentFrame->renderFence));

    // Present
    VkPresentInfoKHR presentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &engine->renderSemaphores[engine->imageIndex],
        .swapchainCount     = 1,
        .pSwapchains        = &engine->swapchain,
        .pImageIndices      = &engine->imageIndex
    };

    VkResult presentResult = vkQueuePresentKHR(engine->graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        engine->resize_requested = true;
    }
    else if (presentResult != VK_SUCCESS)
    {
        // Optional: log error, but don't crash in release
        SDL_Log("vkQueuePresentKHR failed: %d", presentResult);
    }
}

void
deletion_queue_init(struct DeletionQueue* queue)
{
    queue->count = 0;
    queue->capacity = DELETION_QUEUE_INITIAL_CAPACITY;
    queue->entries = (struct DeletionEntry*)malloc(sizeof(struct DeletionEntry) * queue->capacity);
}

void
deletion_queue_push(struct DeletionQueue* queue, DeletionFunc func, void* userdata)
{
    if (queue->count >= queue->capacity)
    {
        // Grow the queue
        queue->capacity *= 2;
        queue->entries = (struct DeletionEntry*)realloc(queue->entries, 
                                                 sizeof(struct DeletionEntry) * queue->capacity);
    }

    queue->entries[queue->count].func = func;
    queue->entries[queue->count].userdata = userdata;
    queue->count++;
}

void
deletion_queue_flush(struct DeletionQueue* queue)
{
    // Destroy in reverse order (LIFO) - most recent first
    for (int i = (int)queue->count - 1; i >= 0; --i)
    {
        queue->entries[i].func(queue->entries[i].userdata);
    }

    queue->count = 0;
}

void
deletion_queue_destroy(struct DeletionQueue* queue)
{
    if (queue->entries)
    {
        free(queue->entries);
        queue->entries = NULL;
    }
    queue->count = 0;
    queue->capacity = 0;
}

void destroy_allocated_image(void* userdata)
{
    struct AllocatedImageDeletion* d = (struct AllocatedImageDeletion*)userdata;
    
    if (d == NULL) return;

    if (d->imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(d->device, d->imageView, NULL);
        d->imageView = VK_NULL_HANDLE;
    }

    if (d->image != VK_NULL_HANDLE)
    {
        vkDestroyImage(d->device, d->image, NULL);
        d->image = VK_NULL_HANDLE;
    }

    free(d);   // important: free the struct we allocated
}

void deletion_queue_push_image(struct DeletionQueue *queue,
                               VkDevice device,
                               VkImageView view,
                               VkImage image)
{
    if (image == VK_NULL_HANDLE)
        return;

    struct AllocatedImageDeletion* del = (struct AllocatedImageDeletion*)malloc(sizeof(struct AllocatedImageDeletion));
    if (del == NULL)
        return;

    *del = (struct AllocatedImageDeletion){
        .device    = device,
        .imageView = view,
        .image     = image
    };

    deletion_queue_push(queue, destroy_allocated_image, del);
}

void destroy_texture(struct VulkanEngine *engine, struct Texture *tex)
{
    if (tex == NULL) return;

    if (tex->sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(engine->device, tex->sampler, NULL);
        tex->sampler = VK_NULL_HANDLE;
    }

    if (tex->view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(engine->device, tex->view, NULL);
        tex->view = VK_NULL_HANDLE;
    }

    if (tex->image != VK_NULL_HANDLE)
    {
        vkDestroyImage(engine->device, tex->image, NULL);
        tex->image = VK_NULL_HANDLE;
    }
}