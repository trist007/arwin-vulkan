#include "vk_engine.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include <SDL3/SDL_vulkan.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "vk_pipelines.h"

#include "ktxvulkan.h"

// IMGui
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

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
  init_commands(engine);
  init_sync_structures(engine);
  init_descriptors(engine);
  init_pipelines(engine);
  init_imgui(engine);
  // init_default_data(engine);
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

void init_commands(VulkanEngine *engine) {
  // create a command pool for commands submitted to the graphics queue
  // we also want the pool to allow for resetting of individual command buffers
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
      engine->graphicsQueueFamily,
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (int i = 0; i < FRAME_OVERLAP; ++i) {
    VK_CHECK(vkCreateCommandPool(engine->device, &commandPoolInfo, 0,
                                 &engine->frames[i].commandPool));

    // allocate the default command buffer that we will use for rendering
    VkCommandBufferAllocateInfo cmdAllocInfo =
        vkinit::command_buffer_allocate_info(engine->frames[i].commandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(engine->device, &cmdAllocInfo,
                                      &engine->frames[i].mainCommandBuffer));
  }

  // allocate the default command buffer that we will use for rendering
  VK_CHECK(vkCreateCommandPool(engine->device, &commandPoolInfo, nullptr,
                               &engine->immCommandPool));

  // allocate the command buffer for immediate submits
  VkCommandBufferAllocateInfo cmdAllocInfo =
      vkinit::command_buffer_allocate_info(engine->immCommandPool, 1);

  VK_CHECK(vkAllocateCommandBuffers(engine->device, &cmdAllocInfo,
                                    &engine->immCommandBuffer));

  engine->mainDeletionQueue.push_function([=]() {
    vkDestroyCommandPool(engine->device, engine->immCommandPool, 0);
  });
}

void init_sync_structures(VulkanEngine *engine) {
  // create synchronization structure
  // one fence to control when the gpu has finished rendering the frame,
  // and 2 semaphores to synchronize rendering with swapchain, we want
  // the fence to start signalled so we can wait on it on the first frame
  // ! NOTE: trist007: On the fence, we are using the flag
  // VK_FENCE_CREATE_SIGNALED_BIT. ! This is very important, as it allows us to
  // wait on a freshly created fence without causing errors. ! If we did not
  // have that bit, when we call into WaitFences the first frame, before the gpu
  // is doing work, ! the thread will be blocked.
  VkFenceCreateInfo fenceCreateInfo =
      vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

  for (int i = 0; i < FRAME_OVERLAP; ++i) {
    VK_CHECK(vkCreateFence(engine->device, &fenceCreateInfo, 0,
                           &engine->frames[i].renderFence));

    VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCreateInfo, 0,
                               &engine->frames[i].swapchainSemaphore));
    VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCreateInfo, 0,
                               &engine->frames[i].renderSemaphore));
  }

  VK_CHECK(
      vkCreateFence(engine->device, &fenceCreateInfo, 0, &engine->immFence));
  engine->mainDeletionQueue.push_function(
      [=]() { vkDestroyFence(engine->device, engine->immFence, 0); });
}

void init_descriptors(VulkanEngine *engine) {
  // create a descriptor pool that will hold 10 sets with 1 image each
  /*
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes  =
  {
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
  };
  */

  DescriptorAllocatorGrowable::PoolSizeRatio globalSizes[2] = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};

  uint32_t sizeCount = 2;

  engine->globalDescriptorAllocator.init_pool(engine->device, 10, globalSizes,
                                              2);

  // make the descriptor set layout for our compute draw
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    engine->drawImageDescriptorLayout =
        builder.build(engine->device, VK_SHADER_STAGE_COMPUTE_BIT);
    if (engine->drawImageDescriptorLayout == VK_NULL_HANDLE) {
      SDL_Log("ERROR: Failed to create singleImageDescriptorLayout!");
      return;
    }
  }
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    engine->singleImageDescriptorLayout =
        builder.build(engine->device, VK_SHADER_STAGE_FRAGMENT_BIT);

    if (engine->singleImageDescriptorLayout == VK_NULL_HANDLE) {
      SDL_Log("ERROR: Failed to create singleImageDescriptorLayout!");
      return;
    }
  }
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    engine->gpuSceneDataDescriptorLayout =
        builder.build(engine->device, VK_SHADER_STAGE_VERTEX_BIT |
                                          VK_SHADER_STAGE_FRAGMENT_BIT);
    if (engine->gpuSceneDataDescriptorLayout == VK_NULL_HANDLE) {
      SDL_Log("ERROR: Failed to create singleImageDescriptorLayout!");
      return;
    }
  }

  // allocate a descriptor set for our draw image
  engine->drawImageDescriptors = engine->globalDescriptorAllocator.allocate(
      engine->device, engine->drawImageDescriptorLayout);

  if (engine->drawImageDescriptors == VK_NULL_HANDLE) {
    SDL_Log("ERROR: Failed to allocate drawImageDescriptors!");
    return;
  } else {
    DescriptorWriter writer;
    writer.write_image(0, engine->drawImage.imageView, VK_NULL_HANDLE,
                       VK_IMAGE_LAYOUT_GENERAL,
                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.update_set(engine->device, engine->drawImageDescriptors);
  }

  // make sure both the descriptor allocator and the new layout get cleaned up
  // properly
  engine->mainDeletionQueue.push_function([&]() {
    engine->globalDescriptorAllocator.destroy_pools(engine->device);

    vkDestroyDescriptorSetLayout(engine->device,
                                 engine->drawImageDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(engine->device,
                                 engine->singleImageDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(engine->device,
                                 engine->gpuSceneDataDescriptorLayout, nullptr);
  });

  for (int i = 0; i < FRAME_OVERLAP; i++) {
    // create a descriptor pool
    DescriptorAllocatorGrowable::PoolSizeRatio frame_sizes[4] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };

    engine->frames[i].frameDescriptors = DescriptorAllocatorGrowable{};
    engine->frames[i].frameDescriptors.init_pool(engine->device, 1000,
                                                 frame_sizes, 4);

    engine->mainDeletionQueue.push_function([&, i]() {
      engine->frames[i].frameDescriptors.destroy_pools(engine->device);
    });
  }
}

void init_background_pipelines(VulkanEngine *engine) {
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &engine->drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  VkPushConstantRange pushConstant{};
  pushConstant.offset = 0;
  pushConstant.size = sizeof(ComputePushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pPushConstantRanges = &pushConstant;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(engine->device, &computeLayout, nullptr,
                                  &engine->gradientPipelineLayout));

  VkShaderModule gradientShader;
  if (!vkutil::load_shader_module("../shaders/gradient_color.comp.spv",
                                  engine->device, &gradientShader)) {
    SDL_Log("Error when building the compute shader \n");
  }

  VkShaderModule skyShader;
  if (!vkutil::load_shader_module("../shaders/sky.comp.spv", engine->device,
                                  &skyShader)) {
    SDL_Log("Error when building the compute shader \n");
  }

  VkPipelineShaderStageCreateInfo stageinfo{};
  stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageinfo.pNext = nullptr;
  stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageinfo.module = gradientShader;
  stageinfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = engine->gradientPipelineLayout;
  computePipelineCreateInfo.stage = stageinfo;

  ComputeEffect gradient;
  gradient.layout = engine->gradientPipelineLayout;
  gradient.name = "gradient";
  gradient.data = {};

  // default colors
  gradient.data.data1 = HMM_Vec4{1, 0, 0, 1};
  gradient.data.data2 = HMM_Vec4{0, 0, 1, 1};

  VK_CHECK(vkCreateComputePipelines(engine->device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &gradient.pipeline));

  // change the shader module only to create the sky shader
  computePipelineCreateInfo.stage.module = skyShader;

  ComputeEffect sky;
  sky.layout = engine->gradientPipelineLayout;
  sky.name = "sky";
  sky.data = {};
  // default sky parameters
  sky.data.data1 = HMM_Vec4{0.1, 0.2, 0.4, 0.97};

  VK_CHECK(vkCreateComputePipelines(engine->device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &sky.pipeline));

  // add the 2 background effects into the array
  engine->backgroundEffects.push_back(gradient);
  engine->backgroundEffects.push_back(sky);

  // destroy structures properly
  vkDestroyShaderModule(engine->device, gradientShader, nullptr);
  vkDestroyShaderModule(engine->device, skyShader, nullptr);
  engine->mainDeletionQueue.push_function([=]() {
    vkDestroyPipelineLayout(engine->device, engine->gradientPipelineLayout,
                            nullptr);
    vkDestroyPipeline(engine->device, sky.pipeline, nullptr);
    vkDestroyPipeline(engine->device, gradient.pipeline, nullptr);
  });
}

void init_pipelines(VulkanEngine *engine) {
  // COMPUTE PIPELINES
  init_background_pipelines(engine);

  // GRAPHICS PIPELINES
  // init_triangle_pipeline(engine);

  // MESH PIPELINES
  init_mesh_pipeline(engine);

  engine->metalRoughMaterial.build_pipelines(engine);
}

void create_swapchain(VulkanEngine *engine, uint32_t width, uint32_t height) {
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
  std::vector<VkFormat> depthFormatList{VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        VK_FORMAT_D24_UNORM_S8_UINT};
  VkFormat depthFormat{VK_FORMAT_UNDEFINED};
  for (VkFormat &format : depthFormatList) {
    VkFormatProperties2 formatProperties{
        .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
    vkGetPhysicalDeviceFormatProperties2(engine->chosenGPU, format,
                                         &formatProperties);
    if (formatProperties.formatProperties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      depthFormat = format;
      break;
    }
  }

  SDL_Log("Selected depth format: %d", (int)depthFormat);

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

void cleanupVulkanEngine(VulkanEngine *engine) {
  if (engine->isInitialized) {
    // make sure the gpu has stopped doing its business
    vkDeviceWaitIdle(engine->device);

    for (int i = 0; i < FRAME_OVERLAP; ++i) {
      // cannot destroy VkQueue just like VkPhysicalDevice
      vkDestroyCommandPool(engine->device, engine->frames[i].commandPool, 0);

      // destroy sync objects
      vkDestroyFence(engine->device, engine->frames[i].renderFence, nullptr);
      vkDestroySemaphore(engine->device, engine->frames[i].renderSemaphore,
                         nullptr);
      vkDestroySemaphore(engine->device, engine->frames[i].swapchainSemaphore,
                         nullptr);

      engine->frames[i].deletionQueue.flush();
    }

    for (auto &mesh : engine->testMeshes) {
      destroy_buffer(engine, mesh->meshBuffers.indexBuffer);
      destroy_buffer(engine, mesh->meshBuffers.vertexBuffer);
    }

    // flush the global deltion queue
    engine->mainDeletionQueue.flush();

    destroy_swapchain(engine);

    vkDestroySurfaceKHR(engine->instance, engine->surface, 0);
    vkDestroyDevice(engine->device, 0);

    // vkb::destroy_debug_utils_messenger(engine->instance,
    // engine->debugMessenger);
    vkDestroyInstance(engine->instance, 0);
    SDL_DestroyWindow(engine->window);

    // clear engine pointer
    s_engine = 0;
  }
}

void drawVulkanEngine(VulkanEngine *engine) {
  FrameData *frame = getCurrentFrame(engine);

  // update_scene(engine);

  // ! NOTE: trist007: We use vkWaitForFences() to wait for the GPU to have
  // finished its work, and after it we reset the fence. ! Fences have to be
  // reset between uses, you can’t use the same fence on multiple GPU commands
  // without resetting it in the middle. ! The timeout of the WaitFences call is
  // of 1 second. It’s using nanoseconds for the wait time. If you call the
  // function with 0 as ! the timeout, you can use it to know if the GPU is
  // still executing the command or not. wait until the gpu has finished
  // rendering the last frame.  Timeout of 1 second
  VK_CHECK(vkWaitForFences(engine->device, 1, &frame->renderFence, VK_TRUE,
                           1000000000ULL));
  // VK_CHECK(vkWaitForFences(engine->device, 1, &frame->renderFence, true,
  // 1000000000));
  VK_CHECK(vkResetFences(engine->device, 1, &frame->renderFence));

  frame->deletionQueue.flush();
  frame->frameDescriptors.clear_pools(engine->device);

  // request image from the swapchain
  uint32_t swapchainImageIndex;
  // VK_CHECK(vkAcquireNextImageKHR(engine->device, engine->swapchain,
  // 1000000000, frame->swapchainSemaphore, 0, &swapchainImageIndex));
  VkResult e =
      vkAcquireNextImageKHR(engine->device, engine->swapchain, 1000000000ULL,
                            frame->swapchainSemaphore, 0, &swapchainImageIndex);
  if (e == VK_ERROR_OUT_OF_DATE_KHR) {
    engine->resize_requested = true;
    return;
  }

  engine->drawExtent.height = std::min(engine->swapchainExtent.height,
                                       engine->drawImage.imageExtent.height) *
                              engine->renderScale;
  engine->drawExtent.width = std::min(engine->swapchainExtent.width,
                                      engine->drawImage.imageExtent.width) *
                             engine->renderScale;

  // VK_CHECK(vkResetFences(engine->device, 1, &frame->renderFence));

  // now that we are sure that the commands finished executing, we can safely
  // reset the command buffer to begin recording again
  // VK_CHECK(vkResetCommandBuffer(cmd, 0));
  VK_CHECK(vkResetCommandBuffer(frame->mainCommandBuffer, 0));

  // naming it cmd for shorter writing
  VkCommandBuffer cmd = frame->mainCommandBuffer;

  // begin the command buffer recording, we will use this command buffer exactly
  // once, so we want to let vulkan know that ! NOTE: trist007: we will give it
  // the flag VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT This is optional, but
  // we might get a ! small speedup from our command encoding if we can tell the
  // drivers ! that this buffer will only be submitted and executed once. We are
  // only doing 1 submit per frame before the command buffer ! is reset, so this
  // is perfectly good for us.
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  // engine->drawExtent.width   = engine->drawImage.imageExtent.width;
  // engine->drawExtent.height  = engine->drawImage.imageExtent.height;

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // 1. Transition drawImage to COLOR_ATTACHMENT_OPTIMAL and clear it to black
  vkutil::transition_image(cmd, engine->drawImage.image,
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  draw_background(engine, cmd);

  vkutil::transition_image(cmd, engine->drawImage.image,
                           VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // 2. Transition depth image
  vkutil::transition_image(cmd, engine->depthImage.image,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  // VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}}; // Dark gray
  // VkClearValue clearDepth = {{{ 1.0f, 0 }}};

  VkRenderingAttachmentInfo colorAttachment =
      vkinit::attachment_info(engine->drawImage.imageView, nullptr,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      engine->depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo = vkinit::rendering_info(
      engine->drawExtent, &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);

  // 3. Draw the geometry (monkey)
  draw_geometry(engine, cmd);

  vkCmdEndRendering(cmd);

  // 4. Transition drawImage for copy to swapchain
  vkutil::transition_image(cmd, engine->drawImage.image,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  vkutil::transition_image(cmd, engine->swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // 5. Copy drawImage → swapchain
  vkutil::copy_image_to_image(cmd, engine->drawImage.image,
                              engine->swapchainImages[swapchainImageIndex],
                              engine->drawExtent, engine->swapchainExtent);

  // 6. Transition swapchain image for ImGui / present
  vkutil::transition_image(cmd, engine->swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  draw_imgui(engine, cmd, engine->swapchainImageViews[swapchainImageIndex]);

  vkutil::transition_image(cmd, engine->swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK(vkEndCommandBuffer(cmd));

  // prepare the submission to the queue.
  // we want to wait on the _presentSemaphore, as that semaphore is signaled
  // when the swapchain is ready we will signal the _renderSemaphore, to signal
  // that rendering has finished
  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      frame->swapchainSemaphore);

  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame->renderSemaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

  // submit command buffer to the queue and execute it.
  //  engine->renderFence will now block until the graphic commands finish
  //  execution
  VK_CHECK(
      vkQueueSubmit2(engine->graphicsQueue, 1, &submit, frame->renderFence));

  // prepare present
  //  this will put the image we just rendered to into the visible window.
  //  we want to wait on the _renderSemaphore for that,
  //  as its necessary that drawing commands have finished before the image is
  //  displayed to the user
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;
  presentInfo.pSwapchains = &engine->swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &frame->renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  // VK_CHECK(vkQueuePresentKHR(engine->graphicsQueue, &presentInfo));
  VkResult presentResult =
      vkQueuePresentKHR(engine->graphicsQueue, &presentInfo);
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
      presentResult == VK_SUBOPTIMAL_KHR) {
    engine->resize_requested = true;
  }

  // increase the number of frames drawn
  engine->frameNumber++;
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
      ImGui_ImplSDL3_ProcessEvent(&e);
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
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // some imgui UI to test
    // ImGui::ShowDemoWindow();

    if (ImGui::Begin("background")) {

      ComputeEffect &selected =
          engine->backgroundEffects[engine->currentBackgroundEffect];

      ImGui::Text("Selected effect: ", selected.name);

      ImGui::SliderFloat("Render Scale", &engine->renderScale, 0.3f, 1.f);
      ImGui::SliderInt("Effect Index", &engine->currentBackgroundEffect, 0,
                       (int)engine->backgroundEffects.size() - 1);

      ImGui::InputFloat4("data1", (float *)&selected.data.data1);
      ImGui::InputFloat4("data2", (float *)&selected.data.data2);
      ImGui::InputFloat4("data3", (float *)&selected.data.data3);
      ImGui::InputFloat4("data4", (float *)&selected.data.data4);
    }
    ImGui::End();

    // make imgui calculate internal draw structures
    ImGui::Render();

    //drawVulkanEngine(engine);
    drawHowtoVulkanEngine(engine);
  }
}

FrameData *getCurrentFrame(VulkanEngine *engine) {
  return &engine->frames[engine->frameNumber % FRAME_OVERLAP];
}

void draw_background(VulkanEngine *engine, VkCommandBuffer cmd) {
  ComputeEffect &effect =
      engine->backgroundEffects[engine->currentBackgroundEffect];

  // bind the background compute pipeline
  // ! NOTE: trist007: VK_PIPELINE_BIND_POINT_COMPUTE is for compute shaders
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

  // bind the descriptor set containing the draw image for the compute pipeline
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          engine->gradientPipelineLayout, 0, 1,
                          &engine->drawImageDescriptors, 0, nullptr);

  vkCmdPushConstants(cmd, engine->gradientPipelineLayout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(ComputePushConstants), &effect.data);

  // execute the compute pipeline dispatch, we are using the 16x16 workgroup
  // size so we need to divide by it
  vkCmdDispatch(cmd, std::ceil(engine->drawExtent.width / 16.0),
                std::ceil(engine->drawExtent.height / 16.0), 1);
}

void immediate_submit(VulkanEngine *engine,
                      std::function<void(VkCommandBuffer cmd)> &&function) {
  VK_CHECK(vkResetFences(engine->device, 1, &engine->immFence));

  VK_CHECK(vkResetCommandBuffer(engine->immCommandBuffer, 0));

  VkCommandBuffer cmd = engine->immCommandBuffer;

  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit2(engine->graphicsQueue, 1, &submit, engine->immFence));

  VK_CHECK(
      vkWaitForFences(engine->device, 1, &engine->immFence, true, 9999999999));
}

void init_imgui(VulkanEngine *engine) {
  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo
  //  itself.
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1000,
      .poolSizeCount = (uint32_t)std::size(pool_sizes),
      .pPoolSizes = pool_sizes};

  VkDescriptorPool imguiPool;
  VK_CHECK(
      vkCreateDescriptorPool(engine->device, &pool_info, nullptr, &imguiPool));

  // 2: initialize imgui library
  // this initializes the core structures of imgui
  ImGui::CreateContext();

  // this initializes imgui for SDL
  ImGui_ImplSDL3_InitForVulkan(engine->window);

  // this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = engine->instance,
      .PhysicalDevice = engine->chosenGPU,
      .Device = engine->device,
      .Queue = engine->graphicsQueue,
      .DescriptorPool = imguiPool,
      .MinImageCount = 3,
      .ImageCount = 3,
      .UseDynamicRendering = true,
  };

  // dynamic rendering parameters for imgui to use
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount =
      1;
  init_info.PipelineInfoMain.PipelineRenderingCreateInfo
      .pColorAttachmentFormats = &engine->swapchainImageFormat;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  // ! NOTE: trist007: this function has been removed, font texture upload is
  // now ! handled within ImGui_ImplVulkan_NewFrame()
  // ImGui_ImplVulkan_CreateFontsTexture();

  // add the destroy the imgui created structures
  engine->mainDeletionQueue.push_function([=]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(engine->device, imguiPool, nullptr);
  });
}

void draw_imgui(VulkanEngine *engine, VkCommandBuffer cmd,
                VkImageView targetImageView) {
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(
      engine->swapchainExtent, &colorAttachment, nullptr);

  vkCmdBeginRendering(cmd, &renderInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}

/*
void
init_triangle_pipeline(VulkanEngine *engine)
{
        VkShaderModule triangleFragShader;
        if
(!vkutil::load_shader_module("../arwin/shaders/colored_triangle.frag.spv",
engine->device, &triangleFragShader)) { SDL_Log("Error when building the
triangle fragment shader module");
        }
        else {
                SDL_Log("Triangle fragment shader succesfully loaded");
        }

        VkShaderModule triangleVertexShader;
        if
(!vkutil::load_shader_module("../arwin/shaders/colored_triangle.vert.spv",
engine->device, &triangleVertexShader)) { SDL_Log("Error when building the
triangle vertex shader module");
        }
        else {
                SDL_Log("Triangle vertex shader succesfully loaded");
        }

        //build the pipeline layout that controls the inputs/outputs of the
shader
        //we are not using descriptor sets or other systems yet, so no need to
use anything other than empty default VkPipelineLayoutCreateInfo
pipeline_layout_info = vkinit::pipeline_layout_create_info();
        VK_CHECK(vkCreatePipelineLayout(engine->device, &pipeline_layout_info,
nullptr, &engine->trianglePipelineLayout));

        PipelineBuilder pipelineBuilder = {};
    clear(&pipelineBuilder);

        //use the triangle layout we created
        pipelineBuilder.pipelineLayout = engine->trianglePipelineLayout;
        //connecting the vertex and pixel shaders to the pipeline
        set_shaders(&pipelineBuilder, triangleVertexShader, triangleFragShader);
        //it will draw triangles
        set_input_topology(&pipelineBuilder,
VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        //filled triangles
        set_polygon_mode(&pipelineBuilder, VK_POLYGON_MODE_FILL);
        //no backface culling
        set_cull_mode(&pipelineBuilder, VK_CULL_MODE_NONE,
VK_FRONT_FACE_CLOCKWISE);
        //no multisampling
        set_multisampling_none(&pipelineBuilder);
        //no blending
        disable_blending(&pipelineBuilder);
        //no depth testing
        disable_depthtest(&pipelineBuilder);

        //connect the image format we will draw into, from draw image
        set_color_attachment_format(&pipelineBuilder,
engine->drawImage.imageFormat); set_depth_format(&pipelineBuilder,
VK_FORMAT_UNDEFINED);

        //finally build the pipeline
        engine->trianglePipeline = build_pipeline(&pipelineBuilder,
engine->device);

        //clean structures
        vkDestroyShaderModule(engine->device, triangleFragShader, nullptr);
        vkDestroyShaderModule(engine->device, triangleVertexShader, nullptr);

        engine->mainDeletionQueue.push_function([&]() {
                vkDestroyPipelineLayout(engine->device,
engine->trianglePipelineLayout, nullptr); vkDestroyPipeline(engine->device,
engine->trianglePipeline, nullptr);
        });
}
*/

void draw_geometry(VulkanEngine *engine, VkCommandBuffer cmd) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->meshPipeline);

  // === Bind texture descriptor set ===
  FrameData *frame = getCurrentFrame(engine);
  VkDescriptorSet imageSet = frame->frameDescriptors.allocate(
      engine->device, engine->singleImageDescriptorLayout);

  {
    DescriptorWriter writer;
    writer.write_image(0, engine->errorCheckerboardImage.imageView,
                       engine->defaultSamplerNearest,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(engine->device, imageSet);
  }

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          engine->meshPipelineLayout, 0, 1, &imageSet, 0,
                          nullptr);

  // === DYNAMIC STATES - MUST BE SET BEFORE DRAW ===
  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(engine->drawExtent.width);
  viewport.height = static_cast<float>(engine->drawExtent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = {engine->drawExtent.width, engine->drawExtent.height};
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // VkCmdDraw(cmd, 3, 1, 0, 0);

  // Push constants (MVP only)
  HMM_Mat4 view = HMM_Translate(HMM_V3(0.0f, 0.0f, -8.0f));
  float aspectRatio =
      (float)engine->windowExtent.width / (float)engine->windowExtent.height;

  HMM_Mat4 projection =
      HMM_Perspective_LH_NO(HMM_ToRad(70.0f), aspectRatio, 0.1f, 10000.0f);

  // Flip Y axis for Vulkan
  projection.Columns[1].Y = -projection.Columns[1].Y;

  GPUDrawPushConstants push_constants = {};
  push_constants.worldMatrix = projection * view;
  push_constants.vertexBuffer =
      engine->testMeshes[2]->meshBuffers.vertexBufferAddress;

  vkCmdPushConstants(cmd, engine->meshPipelineLayout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(GPUDrawPushConstants), &push_constants);

  vkCmdBindIndexBuffer(cmd,
                       engine->testMeshes[2]->meshBuffers.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  // Draw the monkey head
  vkCmdDrawIndexed(cmd, engine->testMeshes[2]->surfaces[0].count, 1,
                   engine->testMeshes[2]->surfaces[0].startIndex, 0, 0);

  // allocate a new uniform buffer for the scene data
  AllocatedBuffer gpuSceneDataBuffer = create_buffer(
      engine, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU);

  // add it to the deletion queue of this frame so it gets deleted once its been
  // used
  frame->deletionQueue.push_function(
      [=]() { destroy_buffer(engine, gpuSceneDataBuffer); });

  // write the buffer
  GPUSceneData *sceneUniformData =
      (GPUSceneData *)gpuSceneDataBuffer.allocation->GetMappedData();
  *sceneUniformData = engine->sceneData;

  // create a descriptor set that binds that buffer and update it
  VkDescriptorSet globalDescriptor = frame->frameDescriptors.allocate(
      engine->device, engine->gpuSceneDataDescriptorLayout);

  DescriptorWriter writer;
  writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.update_set(engine->device, globalDescriptor);
  for (const RenderObject &draw : engine->mainDrawContext.OpaqueSurfaces) {

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      draw.material->pipeline->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            draw.material->pipeline->layout, 0, 1,
                            &globalDescriptor, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            draw.material->pipeline->layout, 1, 1,
                            &draw.material->materialSet, 0, nullptr);

    vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    GPUDrawPushConstants pushConstants;
    pushConstants.vertexBuffer = draw.vertexBufferAddress;
    pushConstants.worldMatrix = draw.transform;
    vkCmdPushConstants(cmd, draw.material->pipeline->layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(GPUDrawPushConstants), &pushConstants);

    vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);
  }
}

AllocatedBuffer create_buffer(VulkanEngine *engine, size_t allocSize,
                              VkBufferUsageFlags usage,
                              VmaMemoryUsage memoryUsage) {
  // allocate buffer
  VkBufferCreateInfo bufferInfo = {.sType =
                                       VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.pNext = nullptr;
  bufferInfo.size = allocSize;

  bufferInfo.usage = usage;

  VmaAllocationCreateInfo vmaallocInfo = {};
  vmaallocInfo.usage = memoryUsage;
  vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  AllocatedBuffer newBuffer;

  // allocate the buffer
  VK_CHECK(vmaCreateBuffer(engine->allocator, &bufferInfo, &vmaallocInfo,
                           &newBuffer.buffer, &newBuffer.allocation,
                           &newBuffer.info));

  return newBuffer;
}

void destroy_buffer(VulkanEngine *engine, const AllocatedBuffer &buffer) {
  vmaDestroyBuffer(engine->allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers uploadMesh(VulkanEngine *engine, std::span<uint32_t> indices,
                          std::span<Vertex> vertices) {
  const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

  GPUMeshBuffers newSurface;

  // create vertex buffer
  /*
      newSurface.vertexBuffer = create_buffer(engine, vertexBufferSize,
     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
      */
  newSurface.vertexBuffer = create_buffer(
      engine, vertexBufferSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | // keep for vertex pulling if you
                                           // want later
          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, // ← ADD THIS
      VMA_MEMORY_USAGE_GPU_ONLY);

  // find the adress of the vertex buffer
  VkBufferDeviceAddressInfo deviceAdressInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = newSurface.vertexBuffer.buffer};
  newSurface.vertexBufferAddress =
      vkGetBufferDeviceAddress(engine->device, &deviceAdressInfo);

  // create index buffer
  newSurface.indexBuffer = create_buffer(engine, indexBufferSize,
                                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY);

  AllocatedBuffer staging = create_buffer(
      engine, vertexBufferSize + indexBufferSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

  void *data = staging.allocation->GetMappedData();

  // copy vertex buffer
  memcpy(data, vertices.data(), vertexBufferSize);
  // copy index buffer
  memcpy((char *)data + vertexBufferSize, indices.data(), indexBufferSize);

  immediate_submit(engine, [&](VkCommandBuffer cmd) {
    VkBufferCopy vertexCopy{0};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1,
                    &vertexCopy);

    VkBufferCopy indexCopy{0};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = vertexBufferSize;
    indexCopy.size = indexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1,
                    &indexCopy);
  });

  destroy_buffer(engine, staging);

  return newSurface;
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

void init_mesh_pipeline(VulkanEngine *engine) {
  VkShaderModule vertShader = VK_NULL_HANDLE;
  VkShaderModule fragShader = VK_NULL_HANDLE;

  if (!vkutil::load_shader_module("../shaders/colored_triangle_mesh.vert.spv",
                                  engine->device, &vertShader)) {
    SDL_Log("ERROR: Failed to load colored_triangle_mesh.vert.spv from "
            "directory: %s",
            SDL_GetCurrentDirectory());
    vkDestroyShaderModule(engine->device, vertShader, nullptr);
    return;
  }
  if (!vkutil::load_shader_module("../shaders/tex_image.frag.spv",
                                  engine->device, &fragShader)) {
    SDL_Log("ERROR: Failed to load tex_image.frag.spv from directory: %s",
            SDL_GetCurrentDirectory());
    vkDestroyShaderModule(engine->device, fragShader, nullptr);
    return;
  }

  SDL_Log("Mesh vertex and fragment shaders loaded successfully");

  // Push constant range (only worldMatrix for this simple pipeline)
  VkPushConstantRange pushConstant{};
  pushConstant.offset = 0;
  pushConstant.size = sizeof(GPUDrawPushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  // Descriptor layout (for the texture)
  VkPipelineLayoutCreateInfo pipelineLayoutInfo =
      vkinit::pipeline_layout_create_info();
  pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pSetLayouts = &engine->singleImageDescriptorLayout;
  pipelineLayoutInfo.setLayoutCount = 1;

  VK_CHECK(vkCreatePipelineLayout(engine->device, &pipelineLayoutInfo, nullptr,
                                  &engine->meshPipelineLayout));

  // === Vertex Input Description ===
  // VertexInputDescription vertexDescription =
  // Vertex::get_vertex_description();

  // Build the pipeline
  PipelineBuilder pipelineBuilder = {};
  clear(&pipelineBuilder); // make sure it's zeroed

  pipelineBuilder.pipelineLayout = engine->meshPipelineLayout;

  set_shaders(&pipelineBuilder, vertShader, fragShader);
  set_input_topology(&pipelineBuilder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  // === CRITICAL: Make sure the pipeline actually writes to the color
  // attachment ===
  /*
  pipelineBuilder.colorBlendAttachment.colorWriteMask =
  VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  */

  // pipelineBuilder.colorBlendAttachment.blendEnable = VK_FALSE;

  set_polygon_mode(&pipelineBuilder, VK_POLYGON_MODE_FILL);
  set_cull_mode(&pipelineBuilder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  set_multisampling_none(&pipelineBuilder);
  disable_blending(&pipelineBuilder); // or enable if you want alpha
  // enable_depthtest(&pipelineBuilder, true, VK_COMPARE_OP_LESS_OR_EQUAL);
  disable_depthtest(&pipelineBuilder);

  set_color_attachment_format(&pipelineBuilder, engine->drawImage.imageFormat);
  set_depth_format(&pipelineBuilder, engine->depthImage.imageFormat);

  SDL_Log("Vertex input being set: bindings=%u, attributes=%u",
          pipelineBuilder.vertexInputInfo.vertexBindingDescriptionCount,
          pipelineBuilder.vertexInputInfo.vertexAttributeDescriptionCount);

  // Build it!
  engine->meshPipeline = build_pipeline(&pipelineBuilder, engine->device);

  // Cleanup shader modules
  vkDestroyShaderModule(engine->device, vertShader, nullptr);
  vkDestroyShaderModule(engine->device, fragShader, nullptr);

  // Deletion
  engine->mainDeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(engine->device, engine->meshPipelineLayout,
                            nullptr);
    vkDestroyPipeline(engine->device, engine->meshPipeline, nullptr);
  });

  SDL_Log("Mesh pipeline created successfully");
  /*
      VkShaderModule triangleFragShader;

      if
  (!vkutil::load_shader_module("../arwin/shaders/tex_image_classic.frag.spv",
  engine->device, &triangleFragShader)) { SDL_Log("Error when building the
  triangle fragment shader module");
      }
      else {
              SDL_Log("Triangle fragment shader succesfully loaded");
      }

      VkShaderModule triangleVertexShader;
      if
  (!vkutil::load_shader_module("../arwin/shaders/colored_triangle_mesh_classic.vert.spv",
  engine->device, &triangleVertexShader)) { SDL_Log("Error when building the
  triangle vertex shader module");
      }
      else {
              SDL_Log("Triangle vertex shader succesfully loaded");
      }

      VkPushConstantRange bufferRange{};
      bufferRange.offset = 0;
      bufferRange.size = sizeof(GPUDrawPushConstants);
      bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

      VkPipelineLayoutCreateInfo pipeline_layout_info =
  vkinit::pipeline_layout_create_info();
      pipeline_layout_info.pPushConstantRanges = &bufferRange;
      pipeline_layout_info.pushConstantRangeCount = 1;
  pipeline_layout_info.pSetLayouts = &engine->singleImageDescriptorLayout;
      pipeline_layout_info.setLayoutCount = 1;

      VK_CHECK(vkCreatePipelineLayout(engine->device, &pipeline_layout_info,
  nullptr, &engine->meshPipelineLayout));

      PipelineBuilder pipelineBuilder = {};
  clear(&pipelineBuilder);

      //use the triangle layout we created
      pipelineBuilder.pipelineLayout = engine->meshPipelineLayout;
      //connecting the vertex and pixel shaders to the pipeline
      set_shaders(&pipelineBuilder, triangleVertexShader, triangleFragShader);
      //it will draw triangles
      set_input_topology(&pipelineBuilder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
      //filled triangles
      set_polygon_mode(&pipelineBuilder, VK_POLYGON_MODE_FILL);
      //no backface culling
      set_cull_mode(&pipelineBuilder, VK_CULL_MODE_NONE,
  VK_FRONT_FACE_CLOCKWISE);
      //no multisampling
      set_multisampling_none(&pipelineBuilder);
      //no blending
      // disable_blending(&pipelineBuilder);
  // enable_blending_additive(&pipelineBuilder);

      // disable_depthtest(&pipelineBuilder);
  enable_depthtest(&pipelineBuilder, true, VK_COMPARE_OP_LESS_OR_EQUAL); //
  safer default

      //connect the image format we will draw into, from draw image
      set_color_attachment_format(&pipelineBuilder,
  engine->drawImage.imageFormat); set_depth_format(&pipelineBuilder,
  engine->depthImage.imageFormat);

  // ! NOTE: trist007: Classic Vertex Input
  VertexInputDescription vertexDescription = Vertex::get_vertex_description();

  pipelineBuilder.vertexInputInfo.pVertexBindingDescriptions =
  vertexDescription.bindings.data();
  pipelineBuilder.vertexInputInfo.vertexBindingDescriptionCount =
  (uint32_t)vertexDescription.bindings.size();

  pipelineBuilder.vertexInputInfo.pVertexAttributeDescriptions =
  vertexDescription.attributes.data();
  pipelineBuilder.vertexInputInfo.vertexAttributeDescriptionCount =
  (uint32_t)vertexDescription.attributes.size();

      //finally build the pipeline
      engine->meshPipeline = build_pipeline(&pipelineBuilder, engine->device);

      //clean structures
      vkDestroyShaderModule(engine->device, triangleFragShader, nullptr);
      vkDestroyShaderModule(engine->device, triangleVertexShader, nullptr);

      engine->mainDeletionQueue.push_function([&]() {
              vkDestroyPipelineLayout(engine->device,
  engine->meshPipelineLayout, nullptr); vkDestroyPipeline(engine->device,
  engine->meshPipeline, nullptr);
      });
  */
}

void
drawHowtoVulkanEngine(VulkanEngine *engine)
{

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
        indices.push_back(indices.size());
    }

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

    engine->frames[FRAME_OVERLAP];

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VkBufferCreateInfo uBufferCI{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(ShaderData),
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
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

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(engine->device, &fenceCI, nullptr, &engine->frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(engine->device, &semaphoreCI, nullptr, &engine->frames[i].renderSemaphore));
    }

    for(uint32_t i = 0; i < engine->swapchainImageCount; i++)
    {
        VkSemaphoreCreateInfo semaphoreCI{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .flags = 0
        };

        VK_CHECK(vkCreateSemaphore(engine->device, 
                                &semaphoreCI, 
                                nullptr, 
                                &engine->renderSemaphores[i]));
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

    VkDescriptorImageInfo textureDescriptors[engine->textureCount];

    // Loading textures
    for (uint32_t i = 0; i < engine->textureCount; i++)
    {
        ktxTexture* ktxTexture{ nullptr };
        std::string filename = "assets/suzanne" + std::to_string(i) + ".ktx";
        ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

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
    VkDescriptorSetLayoutBinding descLayoutBindingTex{
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = engine->textureCount,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo descLayoutTexCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &descBindingFlags,
        .bindingCount = 1,
        .pBindings = &descLayoutBindingTex
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

    VkWriteDescriptorSet writeDescSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = engine->descriptorSetTex,
        .dstBinding = 0,
        .descriptorCount = (uint32_t)ArraySize(textureDescriptors),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        .pImageInfo = textureDescriptors
    };
    vkUpdateDescriptorSets(engine->device, 1, &writeDescSet, 0, nullptr);

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

}

bool howtoVulkan(VulkanEngine *engine)
{
    engine->attrib;
    engine->shapes;
    engine->materials;
    std::string warn;
    std::string err;

    bool result = true;

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
    return true;
}

void init_default_data(VulkanEngine *engine) 
{
  /*
      std::array<Vertex,4> rect_vertices;

      rect_vertices[0].position = {0.5,-0.5, 0};
      rect_vertices[1].position = {0.5,0.5, 0};
      rect_vertices[2].position = {-0.5,-0.5, 0};
      rect_vertices[3].position = {-0.5,0.5, 0};

      rect_vertices[0].color = {0,0, 0,1};
      rect_vertices[1].color = { 0.5,0.5,0.5 ,1};
      rect_vertices[2].color = { 1,0, 0,1 };
      rect_vertices[3].color = { 0,1, 0,1 };

      std::array<uint32_t,6> rect_indices;

      rect_indices[0] = 0;
      rect_indices[1] = 1;
      rect_indices[2] = 2;

      rect_indices[3] = 2;
      rect_indices[4] = 1;
      rect_indices[5] = 3;

      engine->rectangle = uploadMesh(engine, rect_indices, rect_vertices);

      //delete the rectangle data on engine shutdown
      engine->mainDeletionQueue.push_function([&](){
              destroy_buffer(engine, engine->rectangle.indexBuffer);
              destroy_buffer(engine, engine->rectangle.vertexBuffer);
      });

      //3 default textures, white, grey, black. 1 pixel each
  uint32_t white   = HMM_PackRGBA(255, 255, 255, 255);
  uint32_t grey    = HMM_PackRGBA(168, 168, 168, 255);     // approx 0.66f
  uint32_t black   = HMM_PackRGBA(0,   0,   0,   255);
  uint32_t magenta = HMM_PackRGBA(255, 0,   255, 255);
  // Checkerboard (16x16)
  uint32_t pixels[16 * 16];

  for (int y = 0; y < 16; y++)
  {
      for (int x = 0; x < 16; x++)
      {
          pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
      }
  }

  engine->errorCheckerboardImage = create_image(engine, pixels,
                                              VkExtent3D{16, 16, 1},
                                              VK_FORMAT_R8G8B8A8_UNORM,
                                              VK_IMAGE_USAGE_SAMPLED_BIT);


  */
  VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;

  vkCreateSampler(engine->device, &sampl, nullptr,
                  &engine->defaultSamplerNearest);

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(engine->device, &sampl, nullptr,
                  &engine->defaultSamplerLinear);

  engine->mainDeletionQueue.push_function([&]() {
    vkDestroySampler(engine->device, engine->defaultSamplerNearest, nullptr);
    vkDestroySampler(engine->device, engine->defaultSamplerLinear, nullptr);

    destroy_image(engine, engine->whiteImage);
    destroy_image(engine, engine->greyImage);
    destroy_image(engine, engine->blackImage);
    destroy_image(engine, engine->errorCheckerboardImage);
  });

  GLTFMetallic_Roughness::MaterialResources materialResources;
  // default the material textures
  materialResources.colorImage = engine->whiteImage;
  materialResources.colorSampler = engine->defaultSamplerLinear;
  materialResources.metalRoughImage = engine->whiteImage;
  materialResources.metalRoughSampler = engine->defaultSamplerLinear;

  // set the uniform buffer for the material data
  AllocatedBuffer materialConstants = create_buffer(
      engine, sizeof(GLTFMetallic_Roughness::MaterialConstants),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

  // write the buffer
  GLTFMetallic_Roughness::MaterialConstants *sceneUniformData =
      (GLTFMetallic_Roughness::MaterialConstants *)
          materialConstants.allocation->GetMappedData();
  sceneUniformData->colorFactors = HMM_Vec4{1, 1, 1, 1};
  sceneUniformData->metal_rough_factors = HMM_Vec4{1, 0.5, 0, 0};

  engine->mainDeletionQueue.push_function(
      [=]() { destroy_buffer(engine, materialConstants); });

  materialResources.dataBuffer = materialConstants.buffer;
  materialResources.dataBufferOffset = 0;

  engine->defaultData = engine->metalRoughMaterial.write_material(
      engine->device, MaterialPass::MainColor, materialResources,
      engine->globalDescriptorAllocator);

  engine->testMeshes = loadGltfMeshes(engine, "../data/assets/basicmesh.glb");

  for (auto &m : engine->testMeshes) {
    std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
    newNode->mesh = m;

    newNode->localTransform = HMM_Mat4{1.f};
    newNode->worldTransform = HMM_Mat4{1.f};

    for (auto &s : newNode->mesh->surfaces) {
      s.material = std::make_shared<GLTFMaterial>(engine->defaultData);
    }

    engine->loadedNodes[m->name] = std::move(newNode);
  }
}

void resize_swapchain(VulkanEngine *engine) {
  vkDeviceWaitIdle(engine->device);

  destroy_swapchain(engine);

  int w, h;
  SDL_GetWindowSize(engine->window, &w, &h);
  engine->windowExtent.width = w;
  engine->windowExtent.height = h;

  create_swapchain(engine, engine->windowExtent.width,
                   engine->windowExtent.height);

  engine->resize_requested = false;
}

AllocatedImage create_image(VulkanEngine *engine, VkExtent3D size,
                            VkFormat format, VkImageUsageFlags usage,
                            bool mipmapped) {
  AllocatedImage newImage;
  newImage.imageFormat = format;
  newImage.imageExtent = size;

  VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
  if (mipmapped) {
    img_info.mipLevels = static_cast<uint32_t>(std::floor(
                             std::log2(std::max(size.width, size.height)))) +
                         1;
  }

  // always allocate images on dedicated GPU memory
  VmaAllocationCreateInfo allocinfo = {};
  allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocinfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // allocate and create the image
  VK_CHECK(vmaCreateImage(engine->allocator, &img_info, &allocinfo,
                          &newImage.image, &newImage.allocation, nullptr));

  // if the format is a depth format, we will need to have it use the correct
  // aspect flag
  VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT) {
    aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  // build a image-view for the image
  VkImageViewCreateInfo view_info =
      vkinit::imageview_create_info(format, newImage.image, aspectFlag);
  view_info.subresourceRange.levelCount = img_info.mipLevels;

  VK_CHECK(vkCreateImageView(engine->device, &view_info, nullptr,
                             &newImage.imageView));

  return newImage;
}

AllocatedImage create_image(VulkanEngine *engine, void *data, VkExtent3D size,
                            VkFormat format, VkImageUsageFlags usage,
                            bool mipmapped) {
  size_t data_size = size.depth * size.width * size.height * 4;
  AllocatedBuffer uploadbuffer =
      create_buffer(engine, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_CPU_TO_GPU);

  memcpy(uploadbuffer.info.pMappedData, data, data_size);

  AllocatedImage new_image = create_image(
      engine, size, format,
      usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      mipmapped);

  immediate_submit(engine, [&](VkCommandBuffer cmd) {
    vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = size;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copyRegion);

    vkutil::transition_image(cmd, new_image.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  });

  destroy_buffer(engine, uploadbuffer);

  return new_image;
}

void destroy_image(VulkanEngine *engine, const AllocatedImage &img) {
  vkDestroyImageView(engine->device, img.imageView, nullptr);
  vmaDestroyImage(engine->allocator, img.image, img.allocation);
}

void GLTFMetallic_Roughness::build_pipelines(VulkanEngine *engine) {
  VkShaderModule meshFragShader;
  if (!vkutil::load_shader_module("../shaders/mesh.frag.spv", engine->device,
                                  &meshFragShader)) {
    SDL_Log("Error when building the triangle fragment shader module");
  }

  VkShaderModule meshVertexShader;
  if (!vkutil::load_shader_module("../shaders/mesh.vert.spv", engine->device,
                                  &meshVertexShader)) {
    SDL_Log("Error when building the triangle vertex shader module");
  }

  VkPushConstantRange matrixRange{};
  matrixRange.offset = 0;
  matrixRange.size = sizeof(GPUDrawPushConstants);
  matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  DescriptorLayoutBuilder layoutBuilder;
  layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  materialLayout =
      layoutBuilder.build(engine->device, VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT);

  VkDescriptorSetLayout layouts[] = {engine->gpuSceneDataDescriptorLayout,
                                     materialLayout};

  VkPipelineLayoutCreateInfo mesh_layout_info =
      vkinit::pipeline_layout_create_info();
  mesh_layout_info.setLayoutCount = 2;
  mesh_layout_info.pSetLayouts = layouts;
  mesh_layout_info.pPushConstantRanges = &matrixRange;
  mesh_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout newLayout;
  VK_CHECK(vkCreatePipelineLayout(engine->device, &mesh_layout_info, nullptr,
                                  &newLayout));

  opaquePipeline.layout = newLayout;
  transparentPipeline.layout = newLayout;

  // build the stage-create-info for both vertex and fragment stages. This lets
  // the pipeline know the shader modules per stage
  PipelineBuilder pipelineBuilder;
  set_shaders(&pipelineBuilder, meshVertexShader, meshFragShader);
  set_input_topology(&pipelineBuilder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  set_polygon_mode(&pipelineBuilder, VK_POLYGON_MODE_FILL);
  set_cull_mode(&pipelineBuilder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  set_multisampling_none(&pipelineBuilder);
  disable_blending(&pipelineBuilder);
  enable_depthtest(&pipelineBuilder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

  // render format
  set_color_attachment_format(&pipelineBuilder, engine->drawImage.imageFormat);
  set_depth_format(&pipelineBuilder, engine->depthImage.imageFormat);

  // use the triangle layout we created
  pipelineBuilder.pipelineLayout = newLayout;

  // finally build the pipeline
  opaquePipeline.pipeline = build_pipeline(&pipelineBuilder, engine->device);

  // create the transparent variant
  enable_blending_additive(&pipelineBuilder);

  enable_depthtest(&pipelineBuilder, false, VK_COMPARE_OP_GREATER_OR_EQUAL);

  transparentPipeline.pipeline =
      build_pipeline(&pipelineBuilder, engine->device);

  vkDestroyShaderModule(engine->device, meshFragShader, nullptr);
  vkDestroyShaderModule(engine->device, meshVertexShader, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(
    VkDevice device, MaterialPass pass, const MaterialResources &resources,
    DescriptorAllocatorGrowable &descriptorAllocator) {
  MaterialInstance matData;
  matData.passType = pass;
  if (pass == MaterialPass::Transparent) {
    matData.pipeline = &transparentPipeline;
  } else {
    matData.pipeline = &opaquePipeline;
  }

  matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

  writer.clear();
  writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants),
                      resources.dataBufferOffset,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.write_image(1, resources.colorImage.imageView, resources.colorSampler,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(2, resources.metalRoughImage.imageView,
                     resources.metalRoughSampler,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  writer.update_set(device, matData.materialSet);

  return matData;
}

void MeshNode::Draw(const HMM_Mat4 &topMatrix, DrawContext &ctx) {
  HMM_Mat4 nodeMatrix = topMatrix * worldTransform;

  for (auto &s : mesh->surfaces) {
    RenderObject def;
    def.indexCount = s.count;
    def.firstIndex = s.startIndex;
    def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
    def.material = &s.material->data;

    def.transform = nodeMatrix;
    def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

    ctx.OpaqueSurfaces.push_back(def);
  }

  // recurse down
  Node::Draw(topMatrix, ctx);
}

void update_scene(VulkanEngine *engine) {
  engine->mainCamera.update();

  HMM_Mat4 view = engine->mainCamera.getViewMatrix();

  // camera projection
  float aspectRatio =
      (float)engine->windowExtent.width / (float)engine->windowExtent.height;

  HMM_Mat4 projection =
      HMM_Perspective_LH_NO(HMM_ToRad(70.0f), // FOV in radians
                            aspectRatio,      // aspect ratio (width/height)
                            0.1f,             // near plane
                            10000.0f          // far plane
      );
  // invert the Y direction on projection matrix so that we are more similar
  // to opengl and gltf axis
  projection[1][1] *= -1;

  engine->sceneData.view = view;
  engine->sceneData.proj = projection;
  engine->sceneData.viewproj = projection * view;

  // some default lighting parameters
  engine->sceneData.ambientColor = HMM_V4(0.1f, 0.1f, 0.1f, 1.0f);
  engine->sceneData.sunlightColor = HMM_V4(1.0f, 1.0f, 1.0f, 1.0f);
  engine->sceneData.sunlightDirection = HMM_V4(0.0f, 1.0f, 0.5f, 1.0f);
}
