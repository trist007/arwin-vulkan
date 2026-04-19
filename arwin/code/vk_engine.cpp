#include "vk_engine.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vk_pipelines.h"

#include "ktxvulkan.h"

#include "HandmadeMath.h"

#include "initVulkan.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define MAX_SWAPCHAIN_IMAGES 4

constexpr bool bUseValidationLayers = true;

static VulkanEngine *s_engine = 0;

void sdl_log_stderr(void *userData, int category, SDL_LogPriority priority,
                    const char *message) {
  fprintf(stderr, "%s\n", message);
}

VulkanEngine *getVulkanEngine(void) { return (s_engine); }

void initVulkanEngine(VulkanEngine *engine) {
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
  howtoVulkan(engine);

  // everything went fine
  engine->isInitialized = true;

  engine->mainCamera.velocity = {0.0f, 0.0f, 0.0f};
  engine->mainCamera.position = {0.0f, 0.0f, 5.0f};

  engine->mainCamera.pitch = 0;
  engine->mainCamera.yaw = 0;
}

void init_vulkan(VulkanEngine *engine) {
  if (!engine || !engine->window) {
    SDL_Log("Error: Invalid engine or window passed to init_vulkan\n");
    return;
  }

  VkResult result;

  // ------------------- 1. Create Vulkan Instance -------------------
  uint32_t instanceVersion = 0;
  vkEnumerateInstanceVersion(&instanceVersion);

  SDL_Log("Vulkan loader supports version %d.%d.%d",
          VK_VERSION_MAJOR(instanceVersion), VK_VERSION_MINOR(instanceVersion),
          VK_VERSION_PATCH(instanceVersion));

  uint32_t instanceExtensionsCount{0};
  const char *const *newinstanceExtensions{
      SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount)};
  // char const* const* newinstanceExtensions{
  // SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount) };

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
  if (!SDL_Vulkan_CreateSurface(engine->window, engine->instance, nullptr,
                                &engine->surface)) {
    SDL_Log("Failed to create Vulkan surface: %s", SDL_GetError());
    return;
  }

  SDL_Log("SDL Vulkan Surface created");

  // ------------------- 3. Select Best Physical Device + Queue Family
  // -------------------
  uint32_t deviceCount = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(engine->instance, &deviceCount, nullptr));
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

  // initialize the memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = engine->chosenGPU;
  allocatorInfo.device = engine->device;
  allocatorInfo.instance = engine->instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &engine->allocator);

  // ! NOTE: trist007: [&]() is the lambda [&] is capture by reference which for
  // a deletion queue ! can be dangerous if the variable goes out of scope
  // before flush() is called [=] capture by ! value is safter because it copies
  // the handle by value at push time
  engine->mainDeletionQueue.push_function(
      [=]() { vmaDestroyAllocator(engine->allocator); });
}

void init_swapchain(VulkanEngine *engine) {
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

  VkImageCreateInfo rimg_info = vkinit::image_create_info(
      engine->drawImage.imageFormat, drawImageUsages, drawImageExtent);

  VmaAllocationCreateInfo rimg_allocinfo = {};
  // ! NOTE: trist007: In vulkan, there are multiple memory regions we can
  // allocate images and buffers from. ! PC implementations with dedicated GPUs
  // will generally have a cpu ram region, a GPU Vram region, and a “upload
  // heap” ! which is a special region of gpu vram that allows cpu writes. If
  // you have resizable bar enabled, the upload heap can ! be the entire gpu
  // vram. Else it will be much smaller, generally only 256 megabytes. We tell
  // VMA to put it on GPU_ONLY ! which will prioritize it to be on the gpu vram
  // but outside of that upload heap region.
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vmaCreateImage(engine->allocator, &rimg_info, &rimg_allocinfo,
                 &engine->drawImage.image, &engine->drawImage.allocation,
                 nullptr);

  VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
      engine->drawImage.imageFormat, engine->drawImage.image,
      VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(vkCreateImageView(engine->device, &rview_info, nullptr,
                             &engine->drawImage.imageView));

  engine->mainDeletionQueue.push_function([=]() {
    vkDestroyImageView(engine->device, engine->drawImage.imageView, nullptr);
    vmaDestroyImage(engine->allocator, engine->drawImage.image,
                    engine->drawImage.allocation);
  });

  engine->depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
  engine->depthImage.imageExtent = drawImageExtent;
  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VkImageCreateInfo dimg_info = vkinit::image_create_info(
      engine->depthImage.imageFormat, depthImageUsages, drawImageExtent);

  // allocate and create the image
  vmaCreateImage(engine->allocator, &dimg_info, &rimg_allocinfo,
                 &engine->depthImage.image, &engine->depthImage.allocation,
                 nullptr);

  // build a image-view for the draw image to use for rendering
  VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
      engine->depthImage.imageFormat, engine->depthImage.image,
      VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(vkCreateImageView(engine->device, &dview_info, nullptr,
                             &engine->depthImage.imageView));

  engine->mainDeletionQueue.push_function([=]() {
    vkDestroyImageView(engine->device, engine->drawImage.imageView, nullptr);
    vmaDestroyImage(engine->allocator, engine->drawImage.image,
                    engine->drawImage.allocation);

    vkDestroyImageView(engine->device, engine->depthImage.imageView, nullptr);
    vmaDestroyImage(engine->allocator, engine->depthImage.image,
                    engine->depthImage.allocation);
  });
}

void create_swapchain(VulkanEngine *engine, uint32_t width, uint32_t height)
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
    swapchainExtent.width = std::clamp(swapchainExtent.width,
                                        surfaceCapabilities.minImageExtent.width,
                                        surfaceCapabilities.maxImageExtent.width);
    swapchainExtent.height = std::clamp(
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
        .pQueueFamilyIndices = nullptr,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE};

    VK_CHECK(vkCreateSwapchainKHR(engine->device, &createInfo, nullptr,
                                    &engine->swapchain));

    engine->swapchainExtent = swapchainExtent;

    // 7. Get swapchain images
    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(engine->device, engine->swapchain,
                            &swapchainImageCount, nullptr);
    SDL_Log("swapchainImageCount: %d", swapchainImageCount);

    VkImage images[MAX_SWAPCHAIN_IMAGES];
    vkGetSwapchainImagesKHR(engine->device, engine->swapchain,
                            &swapchainImageCount, images);

    engine->swapchainImageCount = swapchainImageCount;

    // check depth format
    std::vector<VkFormat> depthFormatList{
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
        };

        engine->depthFormat = VK_FORMAT_UNDEFINED;

    for (VkFormat &format : depthFormatList) {
        VkFormatProperties2 props{
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
        vkGetPhysicalDeviceFormatProperties2(engine->chosenGPU, format,
                                            &props);
        if (props.formatProperties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
        engine->depthFormat = format;
        const char* name = (format == VK_FORMAT_D32_SFLOAT) ? "D32_SFLOAT" :
        (format == VK_FORMAT_D24_UNORM_S8_UINT) ? "D24_UNORM_S8_UINT" : "D32_SFLOAT_S8_UINT";
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

        VK_CHECK(vkCreateImageView(engine->device, &viewInfo, nullptr,
                                &engine->swapchainImageViews[i]));
    }

    SDL_Log("Swapchain created successfully with %u images", swapchainImageCount);
}

void destroy_swapchain(VulkanEngine *engine) {
  vkDestroySwapchainKHR(engine->device, engine->swapchain, 0);

  for (uint32_t i = 0; i < engine->swapchainImageCount; ++i)
    vkDestroyImageView(engine->device, engine->swapchainImageViews[i], 0);
}

void howtoCleanupVulkanEngine(VulkanEngine *engine)
{
    if (!engine || !engine->device) return;

    VK_CHECK(vkDeviceWaitIdle(engine->device));

    // 1. Per-frame resources
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        if (engine->frames[i].renderFence)
            vkDestroyFence(engine->device, engine->frames[i].renderFence, nullptr);

        if (engine->frames[i].swapchainSemaphore)
            vkDestroySemaphore(engine->device, engine->frames[i].swapchainSemaphore, nullptr);

        if (engine->frames[i].renderSemaphore)
            vkDestroySemaphore(engine->device, engine->frames[i].renderSemaphore, nullptr);

        // Shader data buffer
        if (engine->frames[i].shaderDataBuffers.buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(engine->allocator, 
                             engine->frames[i].shaderDataBuffers.buffer, 
                             engine->frames[i].shaderDataBuffers.allocation);
            engine->frames[i].shaderDataBuffers.buffer = VK_NULL_HANDLE;
        }
    }

    // 2. Command pool (destroys all command buffers)
    if (engine->commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(engine->device, engine->commandPool, nullptr);
        engine->commandPool = VK_NULL_HANDLE;
    }

    // 3. Pipeline objects
    if (engine->graphicsPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(engine->device, engine->graphicsPipeline, nullptr);

    if (engine->pipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(engine->device, engine->pipelineLayout, nullptr);

    // 4. Descriptors
    if (engine->descriptorSetLayoutTex != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(engine->device, engine->descriptorSetLayoutTex, nullptr);

    if (engine->descriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(engine->device, engine->descriptorPool, nullptr);

    // 5. Main geometry buffer
    if (engine->vBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(engine->allocator, engine->vBuffer, engine->vBufferAllocation);
        engine->vBuffer = VK_NULL_HANDLE;
    }

    // 6. Textures
    for (uint32_t i = 0; i < engine->textureCount; i++) {
        if (engine->textures[i].view != VK_NULL_HANDLE)
            vkDestroyImageView(engine->device, engine->textures[i].view, nullptr);

        if (engine->textures[i].sampler != VK_NULL_HANDLE)
            vkDestroySampler(engine->device, engine->textures[i].sampler, nullptr);

        if (engine->textures[i].image != VK_NULL_HANDLE)
            vmaDestroyImage(engine->allocator, engine->textures[i].image, engine->textures[i].allocation);
    }

    // 7. Offscreen images
    if (engine->drawImage.imageView != VK_NULL_HANDLE)
        vkDestroyImageView(engine->device, engine->drawImage.imageView, nullptr);

    if (engine->drawImage.image != VK_NULL_HANDLE)
        vmaDestroyImage(engine->allocator, engine->drawImage.image, engine->drawImage.allocation);

    if (engine->depthImage.imageView != VK_NULL_HANDLE)
        vkDestroyImageView(engine->device, engine->depthImage.imageView, nullptr);

    if (engine->depthImage.image != VK_NULL_HANDLE)
        vmaDestroyImage(engine->allocator, engine->depthImage.image, engine->depthImage.allocation);

    // 8. Swapchain
    destroy_swapchain(engine);

    // 9. Final objects
    if (engine->surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(engine->instance, engine->surface, nullptr);

    if (engine->allocator != VK_NULL_HANDLE)
        vmaDestroyAllocator(engine->allocator);

    if (engine->device != VK_NULL_HANDLE)
        vkDestroyDevice(engine->device, nullptr);

    if (engine->instance != VK_NULL_HANDLE)
        vkDestroyInstance(engine->instance, nullptr);

    if (engine->window)
        SDL_DestroyWindow(engine->window);

    SDL_Quit();
}

void runVulkanEngine(VulkanEngine *engine) {
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
      engine->mainCamera.processSDLEvent(e);
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

    //drawVulkanEngine(engine);
    drawHowtoVulkanEngine(engine);
  }
}

FrameData *getCurrentFrame(VulkanEngine *engine) {
  return &engine->frames[engine->frameIndex % FRAME_OVERLAP];
}

VertexInputDescription Vertex::get_vertex_description() {
  VertexInputDescription description = {};

  // ====================== BINDING DESCRIPTION ======================
  VkVertexInputBindingDescription bindingDesc = {};
  bindingDesc.binding = 0;
  bindingDesc.stride = sizeof(Vertex);
  bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings.push_back(bindingDesc); // ← push into the vector

  // ====================== ATTRIBUTE DESCRIPTIONS ======================
  // We need to match the new Vertex struct with HMM_Vec2 texcoord

  VkVertexInputAttributeDescription attr = {};

  // Location 0: Position (HMM_Vec3)
  attr.binding = 0;
  attr.location = 0;
  attr.format = VK_FORMAT_R32G32B32_SFLOAT;
  attr.offset = offsetof(Vertex, position);
  description.attributes.push_back(attr);

  // Location 1: Normal (HMM_Vec3)
  attr.binding = 0;
  attr.location = 1;
  attr.format = VK_FORMAT_R32G32B32_SFLOAT;
  attr.offset = offsetof(Vertex, normal);
  description.attributes.push_back(attr);

  // Location 2: Texcoord (HMM_Vec2)
  attr.binding = 0;
  attr.location = 2;
  attr.format = VK_FORMAT_R32G32_SFLOAT;
  attr.offset = offsetof(Vertex, texcoord);
  description.attributes.push_back(attr);

  // Location 3: Color (HMM_Vec2)
  attr.binding = 0;
  attr.location = 3;
  attr.format = VK_FORMAT_R32G32_SFLOAT;
  attr.offset = offsetof(Vertex, color);
  description.attributes.push_back(attr);

  return description;
}

void
drawHowtoVulkanEngine(VulkanEngine *engine)
{
    // for glb
    vkCmdBindVertexBuffers(cmd, 0, 1, &engine->vBuffer, &vOffset);  // vOffset = 0
    vkCmdBindIndexBuffer(cmd, engine->vBuffer, engine->indexBufferOffset, VK_INDEX_TYPE_UINT32);  // note: uint32 now
    vkCmdDrawIndexed(cmd, engine->indexCount, 1, 0, 0, 0);   // instanceCount = 1 for now

    /*
    I used VK_INDEX_TYPE_UINT32 because glTF often has more than 65k vertices.
    If you want to support multiple meshes later, we can extend upload_gltf_to_gpu to create separate buffers per mesh.
    Skinning (joints/weights) is already in the Vertex struct, so you can add the skinning matrix uniform later.
    */

    // get current frame
    FrameData &currentFrame = engine->frames[engine->frameIndex];

    // Wait on fence for the last frame the GPU has worked and reset it for the next submission
    VK_CHECK(vkWaitForFences(engine->device, 1, &currentFrame.renderFence, true, UINT64_MAX));
    VK_CHECK(vkResetFences(engine->device, 1, &currentFrame.renderFence));

    // Acquire next image
    //! NOTE(trist007): Semaphores
    //! swapchainSemaphore is signaled by the presentation engine when the swapchain image is ready to be used.
    //! renderSemaphore is usually signaled by the queue submission when rendering is finished, so the presentation can wait on it.
    uint32_t imageIndex;
    VkResult e = vkAcquireNextImageKHR(
        engine->device,
        engine->swapchain,
        UINT64_MAX,
        currentFrame.swapchainSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if(e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        engine->resize_requested = true;
        return;
    }

    engine->imageIndex = imageIndex;

    // Update shader data
    currentFrame.shaderData.projection = HMM_Perspective_RH_ZO(
        HMM_AngleDeg(45.0f),
        (float)engine->windowExtent.width / (float)engine->windowExtent.height,
        0.1f,
        32.0f
    );
    currentFrame.shaderData.view = HMM_Translate(HMM_V3(engine->camPos.X, engine->camPos.Y, engine->camPos.Z));

    // create 3 monkey heads
    for (uint32_t i = 0; i < 3; i++) {

        // set positions of each
        HMM_Vec3 instancePos = HMM_V3(
            (i - 1) * 3.0f,
            0.0f,
            0.0f
        );

        HMM_Quat rotation = HMM_Q(0.0f, 0.0f, 0.0f, 1.0f);

        currentFrame.shaderData.model[i] = HMM_Translate(instancePos) * HMM_QToM4(rotation);
    }

    memcpy(currentFrame.shaderDataBuffers.allocationInfo.pMappedData, &currentFrame.shaderData, sizeof(ShaderData));

    // Record command buffer
    VkCommandBuffer cmd = currentFrame.commandBuffers; 
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    std::array<VkImageMemoryBarrier2, 2> outputBarriers{
        VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .image = engine->swapchainImages[engine->imageIndex],
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
        },
        VkImageMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .image = engine->depthImage.image,
            //.subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1 }
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
        }
            };
    VkDependencyInfo barrierDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = outputBarriers.data()
    };
    vkCmdPipelineBarrier2(cmd, &barrierDependencyInfo);

    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = engine->swapchainImageViews[engine->imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue{.color{ 0.0f, 0.0f, 0.2f, 1.0f }} // RGB this is blue
    };
    VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = engine->depthImage.imageView,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = {1.0f,  0}}
    };

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea{.extent{ .width = engine->windowExtent.width, .height = engine->windowExtent.height }},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo
    };

    vkCmdBeginRendering(cmd, &renderingInfo);

    // setting up viewport
    VkViewport vp{
        .width = (float)engine->windowExtent.width,
        .height = (float)engine->windowExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D scissor{ .extent{.width = engine->windowExtent.width, .height = engine->windowExtent.height }};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->graphicsPipeline);
    VkDeviceSize vOffset{ 0 };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->pipelineLayout, 0, 1, &engine->descriptorSetTex, 0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &engine->vBuffer, &vOffset);
    vkCmdBindIndexBuffer(cmd, engine->vBuffer, engine->indexBufferOffset, VK_INDEX_TYPE_UINT16);

    // draw command
    vkCmdDrawIndexed(cmd, engine->indexCount, 3, 0, 0, 0);
    vkCmdEndRendering(cmd);

    // transition swapchain image that we just used as an attachment
    VkImageMemoryBarrier2 barrierPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = engine->swapchainImages[engine->imageIndex],
        .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };
    VkDependencyInfo barrierPresentDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierPresent
    };
    vkCmdPipelineBarrier2(cmd, &barrierPresentDependencyInfo);

    // end command buffer
    vkEndCommandBuffer(cmd);

    // Submit command buffer
    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    //VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame.swapchainSemaphore,
        .pWaitDstStageMask = &waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &engine->renderSemaphores[imageIndex]
    };
    VK_CHECK(vkQueueSubmit(engine->graphicsQueue, 1, &submitInfo, currentFrame.renderFence));

    // Present image
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &engine->renderSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &engine->swapchain,
        .pImageIndices = &engine->imageIndex
    };

    VkResult presentResult = vkQueuePresentKHR(engine->graphicsQueue, &presentInfo);
    if(presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        engine->resize_requested = true;
        return;
    }

    // Advance frame index at the VERY END
    engine->frameIndex = (engine->frameIndex + 1) % FRAME_OVERLAP;
}

bool howtoVulkan(VulkanEngine *engine)
{
    engine->attrib;
    engine->shapes;
    engine->materials;
    std::string warn;
    std::string err;

    bool result = true;


    // === REMOVE ALL THIS OLD CODE ===
    // tinyobj::LoadObj(...)
    // the big for loop that built vertices and indices from attrib
    // the KTX texture loading loop
    // engine->textureCount = 3;  etc.

    // === REPLACE WITH THIS ===

    LoadedModel myModel = load_gltf_model(&gArena, "../data/models/your_model.glb");  // ← change path

    if (myModel.mesh.vertCount == 0) {
        SDL_Log("Failed to load glTF model!");
        return false;
    }

    // Upload to Vulkan GPU buffers
    if (!upload_gltf_to_gpu(engine, &myModel)) {
        SDL_Log("Failed to upload glTF mesh to GPU");
        return false;
    }

    SDL_Log("Successfully loaded and uploaded glTF model with %d vertices, %d indices", 
            myModel.mesh.vertCount, myModel.mesh.triCount);

    // load obj
    result = tinyobj::LoadObj(&engine->attrib, &engine->shapes, &engine->materials, &warn, &err, "../data/assets/suzanne.obj", "../data/assets");

    if (!warn.empty()) {
        std::cout << "Warning: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
    }
    if (!result) {
        std::cerr << "Failed to load OBJ file!" << std::endl;
        abort();
    }

        // At this point, attrib, shapes, and materials contain the model data.
        // You still need to convert them anto your std::vector<Vertex> and std::vector<uint32_t> indices.

    std::cout << "Loaded " << engine->shapes.size() << " shape(s) successfully!" << std::endl;
    std::cout << "Loaded " << engine->materials.size() << " material(s) successfully!" << std::endl;
        // interleaved vertex attributes, for every vertex three floats for the position are followed by three floats for
    // the normal vector (used for lighting), which in turn is followed by two floats for texture coordinates
    const VkDeviceSize indexCount{engine->shapes[0].mesh.indices.size()};
    std::vector<Vertex> vertices{};
    std::vector<uint16_t> indices{};

    // Load vertex and index data
    for(auto& index : engine->shapes[0].mesh.indices)
    {
        Vertex v{
            .position = HMM_V3(
                engine->attrib.vertices[index.vertex_index * 3],
                -engine->attrib.vertices[index.vertex_index * 3 + 1],   // flip Y (common for OBJ)
                engine->attrib.vertices[index.vertex_index * 3 + 2]
            ),

            .normal = HMM_V3(
                engine->attrib.normals[index.normal_index * 3],
                -engine->attrib.normals[index.normal_index * 3 + 1],    // flip Y
                engine->attrib.normals[index.normal_index * 3 + 2]
            ),

            .texcoord = HMM_V2(
                engine->attrib.texcoords[index.texcoord_index * 2],
                1.0f - engine->attrib.texcoords[index.texcoord_index * 2 + 1]  // flip V (OBJ uses bottom-left origin)
            ),

            .color = HMM_V2(1.0f, 1.0f)   // default white / unused for now
        };
        vertices.push_back(v);
        indices.push_back(index.vertex_index);
        //indices.push_back(indices.size());
    }

    // Saving to engine
    engine->vertexBufferSize = sizeof(Vertex) * vertices.size();
    engine->indexBufferOffset = engine->vertexBufferSize;
    engine->indexCount = (uint32_t)(indices.size());

    // Upload data to the GPU via buffers
    VkDeviceSize vBufSize{ sizeof(Vertex) * vertices.size() };
    VkDeviceSize iBufSize{ sizeof(uint16_t) * indices.size() };
    VkBufferCreateInfo bufferCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vBufSize + iBufSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    };

    VmaAllocationCreateInfo vBufferAllocCI{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    VmaAllocationInfo vBufferAllocInfo{};
    VK_CHECK(vmaCreateBuffer(engine->allocator, &bufferCI, &vBufferAllocCI, &engine->vBuffer, &engine->vBufferAllocation, &vBufferAllocInfo));

    memcpy(vBufferAllocInfo.pMappedData, vertices.data(), vBufSize);
    memcpy(((char*)vBufferAllocInfo.pMappedData) + vBufSize, indices.data(), iBufSize);

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VkBufferCreateInfo uBufferCI{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(ShaderData),
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        };
        VmaAllocationCreateInfo uBufferAllocCI{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        VK_CHECK(vmaCreateBuffer(engine->allocator, &uBufferCI, &uBufferAllocCI,
            &engine->frames[i].shaderDataBuffers.buffer,
            &engine->frames[i].shaderDataBuffers.allocation,
            &engine->frames[i].shaderDataBuffers.allocationInfo));

        VkBufferDeviceAddressInfo uBufferBdaInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = engine->frames[i].shaderDataBuffers.buffer
        };

        engine->frames[i].shaderDataBuffers.deviceAddress = vkGetBufferDeviceAddress(engine->device, &uBufferBdaInfo);
    }

    VkSemaphoreCreateInfo semaphoreCI{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceCI{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    //! NOTE(trist007): was doing 1 semaphore per frame but was getting VUID-vkQueueSubmit-pSignalSemaphores-00067
    //! moving to 1 semaphore per swapchainImage
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(engine->device, &fenceCI, nullptr, &engine->frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCI, nullptr, &engine->frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCI, nullptr, &engine->frames[i].renderSemaphore));
    }

    //! NOTE(trist007): creating 1 semaphore per swapchainImageCount
    for(uint32_t i = 0; i < engine->swapchainImageCount; i++)
    {
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCI, nullptr, &engine->renderSemaphores[i]));
    }

    VkCommandPoolCreateInfo commandPoolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = engine->graphicsQueueFamily
    };

    VK_CHECK(vkCreateCommandPool(engine->device, &commandPoolCI, nullptr, &engine->commandPool));

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VkCommandBufferAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = engine->commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VK_CHECK(vkAllocateCommandBuffers(engine->device, &allocInfo, &engine->frames[i].commandBuffers));
    }

    // there are 3 ktx files in assets
    engine->textureCount = 3;
    VkDescriptorImageInfo textureDescriptors[engine->textureCount];

    // Loading textures
    for (uint32_t i = 0; i < engine->textureCount; i++)
    {
        ktxTexture* ktxTexture{ nullptr };
        std::string filename = "../data/assets/suzanne" + std::to_string(i) + ".ktx";
        KTX_error_code ret = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

        if(ret != KTX_SUCCESS || ktxTexture == nullptr)
        {
            SDL_Log("current working directory: %s", SDL_GetCurrentDirectory());
            SDL_Log("Error: failed to load texture %s (error: %d)", filename.c_str(), ret);
            abort();
        }

        VkImageCreateInfo texImgCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ktxTexture_GetVkFormat(ktxTexture),
        .extent = {.width = ktxTexture->baseWidth, .height = ktxTexture->baseHeight, .depth = 1 },
        .mipLevels = ktxTexture->numLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VmaAllocationCreateInfo texImageAllocCI{ .usage = VMA_MEMORY_USAGE_AUTO };
        VK_CHECK(vmaCreateImage(engine->allocator, &texImgCI, &texImageAllocCI, &engine->textures[i].image, &engine->textures[i].allocation, nullptr));

        VkImageViewCreateInfo texVewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = engine->textures[i].image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texImgCI.format,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 }
        };

        VK_CHECK(vkCreateImageView(engine->device, &texVewCI, nullptr, &engine->textures[i].view));

        VkBuffer imgSrcBuffer{};
        VmaAllocation imgSrcAllocation{};
        VkBufferCreateInfo imgSrcBufferCI{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = (uint32_t)ktxTexture->dataSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };
        VmaAllocationCreateInfo imgSrcAllocCI{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };

        VmaAllocationInfo imgSrcAllocInfo{};

        VK_CHECK(vmaCreateBuffer(engine->allocator, &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation, &imgSrcAllocInfo));

        memcpy(imgSrcAllocInfo.pMappedData, ktxTexture->pData, ktxTexture->dataSize);

        VkFenceCreateInfo fenceOneTimeCI{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
        };

        VkFence fenceOneTime{};
        VK_CHECK(vkCreateFence(engine->device, &fenceOneTimeCI, nullptr, &fenceOneTime));
        VkCommandBuffer cbOneTime{};
        VkCommandBufferAllocateInfo cbOneTimeAI{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = engine->commandPool,
            .commandBufferCount = 1
        };
        VK_CHECK(vkAllocateCommandBuffers(engine->device, &cbOneTimeAI, &cbOneTime));

        VkCommandBufferBeginInfo cbOneTimeBI{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));
        VkImageMemoryBarrier2 barrierTexImage{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = engine->textures[i].image,
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 }
        };
        VkDependencyInfo barrierTexInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrierTexImage
        };

        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);

        std::vector<VkBufferImageCopy> copyRegions{};
        for (auto j = 0; j < ktxTexture->numLevels; j++) {
            ktx_size_t mipOffset{0};
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, j, 0, 0, &mipOffset);
            copyRegions.push_back({
                .bufferOffset = mipOffset,
                .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = (uint32_t)j, .layerCount = 1},
                .imageExtent{.width = ktxTexture->baseWidth >> j, .height = ktxTexture->baseHeight >> j, .depth = 1 },
            });
        }

        vkCmdCopyBufferToImage(cbOneTime, imgSrcBuffer, engine->textures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
        VkImageMemoryBarrier2 barrierTexRead{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = engine->textures[i].image,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1 }
        };
        barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;

        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);

        VK_CHECK(vkEndCommandBuffer(cbOneTime));

        VkSubmitInfo oneTimeSI{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cbOneTime
        };
        VK_CHECK(vkQueueSubmit(engine->graphicsQueue, 1, &oneTimeSI, fenceOneTime));
        VK_CHECK(vkWaitForFences(engine->device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX));

        VkSamplerCreateInfo samplerCI{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f, // 8 is a widely supported value for max anisotropy
            .maxLod = (float)ktxTexture->numLevels,
        };
        VK_CHECK(vkCreateSampler(engine->device, &samplerCI, nullptr, &engine->textures[i].sampler));

        ktxTexture_Destroy(ktxTexture);

        textureDescriptors[i].sampler = engine->textures[i].sampler;
        textureDescriptors[i].imageView = engine->textures[i].view;
        textureDescriptors[i].imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    }

    VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &descVariableFlag
    };

    VkDescriptorSetLayoutBinding bindings[2] = {};

    // Binding 0: Textures array
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = engine->textureCount;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 1: ShaderData uniform buffer
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // We no longer need variable descriptor count flags for binding 0 in this simple case
    VkDescriptorSetLayoutCreateInfo descLayoutCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &descLayoutCI, nullptr, &engine->descriptorSetLayoutTex));


    // === Descriptor Pool (must support both types) ===
    VkDescriptorPoolSize poolSizes[2] = {};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = engine->textureCount;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = FRAME_OVERLAP * 2;   // one per frame is safe

    VkDescriptorPoolCreateInfo descPoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes
    };

    VK_CHECK(vkCreateDescriptorPool(engine->device, &descPoolCI, nullptr, &engine->descriptorPool));


    // === Allocate Descriptor Set ===
    VkDescriptorSetAllocateInfo texDescSetAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = engine->descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &engine->descriptorSetLayoutTex
    };

    VK_CHECK(vkAllocateDescriptorSets(engine->device, &texDescSetAlloc, &engine->descriptorSetTex));

    // Binding 0: Textures (array)
    /*
    VkDescriptorSetLayoutBinding descLayoutBindingTex{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = engine->textureCount,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    // Binding 1: ShaderData (ConstantBuffer)
    VkDescriptorSetLayoutBinding descLayoutBindingShaderData{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { descLayoutBindingTex, descLayoutBindingShaderData };

    VkDescriptorSetLayoutCreateInfo descLayoutTexCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &descBindingFlags,
        .bindingCount = 2,
        //.pBindings = &descLayoutBindingTex
        .pBindings = bindings.data()
    };
    VK_CHECK(vkCreateDescriptorSetLayout(engine->device, &descLayoutTexCI, nullptr, &engine->descriptorSetLayoutTex));

    VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = engine->textureCount
    };
    VkDescriptorPoolCreateInfo descPoolCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    VK_CHECK(vkCreateDescriptorPool(engine->device, &descPoolCI, nullptr, &engine->descriptorPool));

    uint32_t variableDescCount{ engine->textureCount };

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &variableDescCount
    };
    VkDescriptorSetAllocateInfo texDescSetAlloc{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &variableDescCountAI,
        .descriptorPool = engine->descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &engine->descriptorSetLayoutTex
    };
    VK_CHECK(vkAllocateDescriptorSets(engine->device, &texDescSetAlloc, &engine->descriptorSetTex));
    */

    // Update existing texture write (binding 0)
    VkWriteDescriptorSet writeTex{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = engine->descriptorSetTex,
        .dstBinding = 0,
        .descriptorCount = engine->textureCount,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        .pImageInfo = textureDescriptors
    };

    // New write for shaderData (binding 1)
    VkDescriptorBufferInfo shaderDataBufferInfo{
        .buffer = engine->frames[0].shaderDataBuffers.buffer,   // use one from current frame
        .offset = 0,
        .range = sizeof(ShaderData)
    };

    VkWriteDescriptorSet writeShaderData{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = engine->descriptorSetTex,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &shaderDataBufferInfo
    };

    std::array<VkWriteDescriptorSet, 2> writes = { writeTex, writeShaderData };

    vkUpdateDescriptorSets(engine->device, 2, writes.data(), 0, nullptr);

    // Loading shaders
    slang::createGlobalSession(engine->slangGlobalSession.writeRef());
    auto slangTargets{ std::to_array<slang::TargetDesc>({ {
        .format{SLANG_SPIRV},
        .profile{engine->slangGlobalSession->findProfile("spirv_1_4")}
    } })};
    auto slangOptions{ std::to_array<slang::CompilerOptionEntry>({ {
        slang::CompilerOptionName::EmitSpirvDirectly,
        {slang::CompilerOptionValueKind::Int, 1}
    } })};
    slang::SessionDesc slangSessionDesc{
        .targets{slangTargets.data()},
        .targetCount{SlangInt(slangTargets.size())},
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries{slangOptions.data()},
        .compilerOptionEntryCount{uint32_t(slangOptions.size())}
    };
    Slang::ComPtr<slang::ISession> slangSession;
    engine->slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef());

    Slang::ComPtr<ISlangBlob> diagnosticsBlob;

    SDL_Log("Loading shader current working dir: %s", SDL_GetCurrentDirectory());
    // get shader of slang format which includes both vertex and fragment in one file
    Slang::ComPtr<slang::IModule> slangModule{
        slangSession->loadModuleFromSource("triangle", "../data/assets/shader.slang", nullptr, diagnosticsBlob.writeRef())
    };

    if (!slangModule) {
        const char* diagMsg = diagnosticsBlob ? (const char*)diagnosticsBlob->getBufferPointer() : "No diagnostics";
        SDL_Log("ERROR: Failed to load Slang module 'shader.slang'");
        SDL_Log("Diagnostics: %s", diagMsg);
        abort();   // or return false;
    }

    SDL_Log("Slang module loaded successfully");

        // compile shader
        Slang::ComPtr<ISlangBlob> spirv;
        slangModule->getTargetCode(0, spirv.writeRef());

    // create shader module
    VkShaderModuleCreateInfo shaderModuleCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv->getBufferSize(),
        .pCode = (uint32_t*)spirv->getBufferPointer()
    };

    VK_CHECK(vkCreateShaderModule(engine->device, &shaderModuleCI, nullptr, &engine->shaderModule));

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(VkDeviceAddress)
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &engine->descriptorSetLayoutTex,
        .pushConstantRangeCount = 0,
        //.pushConstantRangeCount = 1,
        .pPushConstantRanges = nullptr,
        //.pPushConstantRanges = &pushConstantRange
    };
    VK_CHECK(vkCreatePipelineLayout(engine->device, &pipelineLayoutCI, nullptr, &engine->pipelineLayout));

    VkVertexInputBindingDescription vertexBinding{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, texcoord) },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBinding,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
        .pVertexAttributeDescriptions = vertexAttributes.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = engine->shaderModule, .pName = "main"},
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = engine->shaderModule, .pName = "main" }
    };

    // configure viewport state
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };
    std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates.data()
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    };

    VkPipelineRenderingCreateInfo renderingCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &engine->swapchainImageFormat,
        .depthAttachmentFormat = engine->depthFormat
    };

    VkPipelineColorBlendAttachmentState blendAttachment{
        .colorWriteMask = 0xF
    };
    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment
    };
    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampleState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkGraphicsPipelineCreateInfo pipelineCI{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .stageCount = 2,
        .pStages = shaderStages.data(),
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
    VK_CHECK(vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &engine->graphicsPipeline));

    return true;
}

void
resize_swapchain(VulkanEngine *engine)
{
    if (engine->windowExtent.width == 0 || engine->windowExtent.height == 0)
        return;   // minimized, don't resize

    vkDeviceWaitIdle(engine->device);

    destroy_swapchain(engine);

        // Destroy offscreen images
    if (engine->drawImage.imageView != VK_NULL_HANDLE)
        vkDestroyImageView(engine->device, engine->drawImage.imageView, nullptr);
    if (engine->drawImage.image != VK_NULL_HANDLE)
        vmaDestroyImage(engine->allocator, engine->drawImage.image, engine->drawImage.allocation);

    if (engine->depthImage.imageView != VK_NULL_HANDLE)
        vkDestroyImageView(engine->device, engine->depthImage.imageView, nullptr);
    if (engine->depthImage.image != VK_NULL_HANDLE)
        vmaDestroyImage(engine->allocator, engine->depthImage.image, engine->depthImage.allocation);

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
                                      depthFormat,
                                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
                                      false);

    engine->resize_requested = false;

    SDL_Log("Swapchain and render targets resized to %ux%u", w, h);
}

AllocatedImage create_image(VulkanEngine* engine, VkExtent3D size,
                            VkFormat format, VkImageUsageFlags usage,
                            bool mipmapped)
{
    AllocatedImage newImage{};
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);

    if (mipmapped)
    {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(
                                 std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK(vmaCreateImage(engine->allocator, &img_info, &allocInfo,
                            &newImage.image, &newImage.allocation, nullptr));

    // Choose correct aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(
        format, newImage.image, aspectFlag);

    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(engine->device, &view_info, nullptr,
                               &newImage.imageView));

    return newImage;
}

void destroy_image(VulkanEngine *engine, const AllocatedImage &img) {
  vkDestroyImageView(engine->device, img.imageView, nullptr);
  vmaDestroyImage(engine->allocator, img.image, img.allocation);
}