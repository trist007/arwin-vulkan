#include "initVulkan.h"

uint32_t
enableExtCount(DeviceInformation *deviceInfo)
{
    if(!deviceInfo) return 0;

    uint32_t count = 0;

    if(deviceInfo->hasDynamicRendering)      count++;
    if(deviceInfo->hasSynchronization2)      count++;
    if(deviceInfo->hasKhrDescriptorIndexing) count++;
    if(deviceInfo->hasExtDescriptorIndexing) count++;
    if(deviceInfo->hasShaderDrawParameters)  count++;
    if(deviceInfo->hasExtendedDynamicState)  count++;

    return(count);
}

uint32_t
getDeviceCount(VkInstance instance, VkSurfaceKHR surface)
{
    if(instance == VK_NULL_HANDLE)
    {
        SDL_Log("Error: invalid VkInstance passed");
        abort();
    }    

    VkResult result = VK_SUCCESS;

    uint32_t deviceCount = 0;

    result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if(result != VK_SUCCESS)
    {
        SDL_Log("Error: vkEnumeratePhysicalDevices failed");
        abort();
    }

    return(deviceCount);
}

bool createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkDevice* outDevice, uint32_t numExt, DeviceInformation *deviceInfo)
{
    if (physicalDevice == VK_NULL_HANDLE || queueFamilyIndex == UINT32_MAX || outDevice == NULL) {
        SDL_Log("ERROR: Invalid parameters to createLogicalDevice\n");
        return false;
    }

    float queuePriority = 1.0f;

    VkPhysicalDeviceVulkan12Features enabledVk12Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .bufferDeviceAddress = true
    };
    VkPhysicalDeviceVulkan13Features enabledVk13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Features,
        .synchronization2 = true,
        .dynamicRendering = true,
    };
    VkPhysicalDeviceFeatures enabledVk10Features{
        .samplerAnisotropy = VK_TRUE
    };

    const char* extensions[1] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
    };

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabledVk13Features,          // ← Important!
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = &enabledVk10Features                     // Can stay NULL if using pNext chain
    };

        /*
        if(deviceInfo->hasDynamicRendering)
           extensions[i++] = "VK_KHR_dynamic_rendering";
        if(deviceInfo->hasSynchronization2)
           extensions[i++] = "VK_KHR_synchronization2";
        if(deviceInfo->hasKhrDescriptorIndexing)
           extensions[i++] = "VK_KHR_descriptor_indexing";
        if(deviceInfo->hasExtDescriptorIndexing)
           extensions[i++] = "VK_EXT_descriptor_indexing";
        if(deviceInfo->hasShaderDrawParameters)
           extensions[i++] = VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME;
        if(deviceInfo->hasExtendedDynamicState)
           extensions[i++] = VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME;

    SDL_Log("Enabling %u device extensions", totalExt);

    // Feature structures
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = deviceInfo->hasDynamicRendering ? VK_TRUE : VK_FALSE
    };

    VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
        .synchronization2 = deviceInfo->hasSynchronization2 ? VK_TRUE : VK_FALSE
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .extendedDynamicState = deviceInfo->hasExtendedDynamicState ? VK_TRUE : VK_FALSE
    };

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
    .pNext = NULL,

    // Most commonly needed features:
    .descriptorBindingPartiallyBound = deviceInfo->hasKhrDescriptorIndexing ? VK_TRUE : VK_FALSE,
    .runtimeDescriptorArray          = deviceInfo->hasKhrDescriptorIndexing ? VK_TRUE : VK_FALSE,

    // You can enable more if you actually use them:
    // .descriptorBindingVariableDescriptorCount = VK_TRUE,
    // .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
    // .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
    // etc.
    };

    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
        .pNext = NULL,
        .shaderDrawParameters = deviceInfo->hasShaderDrawParameters ? VK_TRUE : VK_FALSE
    };

    // Chain them together
    dynamicRenderingFeatures.pNext     = &synchronization2Features;
    synchronization2Features.pNext     = &extendedDynamicStateFeatures;
    extendedDynamicStateFeatures.pNext = &descriptorIndexingFeatures;
    descriptorIndexingFeatures.pNext   = &shaderDrawParametersFeatures;


        */
    

    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, outDevice);

    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create logical device: %d\n", result);
        return false;
    }

    SDL_Log("Logical device created successfully");
    return(true);
}

bool mincreateLogicalDevice(VkPhysicalDevice physicalDevice,
                         uint32_t queueFamilyIndex,
                         VkDevice* outDevice,
                         uint32_t numExt,              // ignored for now
                         DeviceInformation *deviceInfo)
{
    if (physicalDevice == VK_NULL_HANDLE || 
        queueFamilyIndex == UINT32_MAX || 
        outDevice == NULL)
    {
        SDL_Log("ERROR: Invalid parameters to createLogicalDevice\n");
        return false;
    }

    float queuePriority = 1.0f;

    const char* extensions[1] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
    };

    // Completely empty create info - no pNext, no extra features
    VkDeviceCreateInfo createInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = NULL,                    // ← No feature chain at all
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &queueCreateInfo,
        .enabledExtensionCount   = 1,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures        = NULL
    };

    VkResult result = vkCreateDevice(physicalDevice, &createInfo, NULL, outDevice);

    if (result != VK_SUCCESS) {
        SDL_Log("Failed to create logical device: %d\n", (int)result);
        return false;
    }

    SDL_Log("Logical device created successfully (minimal config)");
    return true;
}

void
getVulkanInstanceVersion()
{
    uint32_t instanceVersion = 0;

    VkResult result = vkEnumerateInstanceVersion(&instanceVersion);

    uint32_t major = VK_API_VERSION_MAJOR(instanceVersion);
    uint32_t minor = VK_API_VERSION_MINOR(instanceVersion);
    uint32_t patch = VK_API_VERSION_PATCH(instanceVersion);

    SDL_Log("Vulkan loader support version %d.%d.%d", major, minor, patch);
}

int
evalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice device, DeviceInformation *deviceInfo)
{
    if(instance == VK_NULL_HANDLE)
    {
        SDL_Log("Error: invalid VkInstance passed!");
        return -1;
    }

    int score = 0;

    VkResult result = VK_SUCCESS;

    uint32_t deviceCount = 1;

    result = vkEnumeratePhysicalDevices(instance, &deviceCount, &device);

    VkPhysicalDeviceProperties properties = {};

    vkGetPhysicalDeviceProperties(device, &properties);

   //SDL_Log("Device: %s\n", deviceName);
    //strncpy(deviceInfo->name, properties.deviceName, sizeof(deviceInfo->name) - 1);
    //snprintf(deviceInfo->name, sizeof(deviceInfo->name), "%s", properties.deviceName);
    SDL_Log("  Vendor ID: 0x%X\n", properties.vendorID);
    SDL_Log("  Device ID: 0x%X\n", properties.deviceID);
    SDL_Log("  Device Type: %d\n", properties.deviceType);
    SDL_Log("  API Version: %u.%u.%u\n",
            VK_VERSION_MAJOR(properties.apiVersion),
            VK_VERSION_MINOR(properties.apiVersion),
            VK_VERSION_PATCH(properties.apiVersion));

    if (device != nullptr)
    {
        //strncpy(deviceInfo->name, deviceName, sizeof(deviceName) - 1);
        //deviceInfo->name[sizeof(deviceInfo->name) - 1] = '\0';   // Guarantee null-termination
    }
    else
    {
        SDL_Log("WARNING: deviceInfo pointer is NULL!");
    }

    // === Get Queue Family Count ===
    uint32_t queueFamilyCount = 0;

    // First call: Get the count
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0) {
        SDL_Log("Device has no queue families!\n");
    }

    // Second call: Fill the array
    VkQueueFamilyProperties queueFamilies[32];   // Usually 8~16 is enough

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

    SDL_Log("Device has %u queue families:\n", queueFamilyCount);

    // Now check each queue family
    for(uint32_t q = 0; q < queueFamilyCount; ++q)
    {
        bool isGraphics = (queueFamilies[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        bool canPresent = false;

        if (surface != VK_NULL_HANDLE)
        {
            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, q, surface, &supported);
            canPresent = (supported == VK_TRUE);
            if(canPresent) score += 10;
        }

        SDL_Log("  Queue Family %u: Graphics=%s, Present=%s, Count=%u\n",
                q,
                isGraphics ? "Yes" : "No",
                canPresent ? "Yes" : "No",
                queueFamilies[q].queueCount);

        if(isGraphics && canPresent)
        {
            deviceInfo->queueFamilyIndex = q;
            break;
        }
    }

    u32 extensionCount = 0;
    VkExtensionProperties extensionArray[MAX_PHYSICAL_DEVICE_EXTENSIONS];

    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    if (extensionCount > MAX_PHYSICAL_DEVICE_EXTENSIONS) extensionCount = MAX_PHYSICAL_DEVICE_EXTENSIONS;

    result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionArray);

    if(result == VK_SUCCESS || result == VK_INCOMPLETE)
    {
        bool hasDynamicRendering = false;
        bool hasSynchronization2 = false;
        bool hasKhrDescriptorIndexing = false;
        bool hasExtDescriptorIndexing = false;
        bool hasShaderDrawParameters = false;
        bool hasExtendedDynamicState = false;


        for(uint32_t i = 0;
        i < extensionCount;
        ++i)
        {
            const char *name = extensionArray[i].extensionName;

            if (strcmp(name, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0)
            {
                hasDynamicRendering = true;
                deviceInfo->hasDynamicRendering = true;
                score += 10;
            }

            else if (strcmp(name, "VK_KHR_synchronization2") == 0)
            {
                hasSynchronization2 = true;
                deviceInfo->hasSynchronization2 = true;
                score += 10;
            }
            else if (strcmp(name, "VK_KHR_descriptor_indexing") == 0)
            {
                hasKhrDescriptorIndexing = true;
                deviceInfo->hasKhrDescriptorIndexing = true;
                score += 10;
            }
            else if (strcmp(name, "VK_EXT_descriptor_indexing") == 0)
            {
                hasExtDescriptorIndexing = true;
                deviceInfo->hasExtDescriptorIndexing = true;
                score += 10;
            }
            else if (strcmp(name, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME) == 0)
            {
                hasShaderDrawParameters = true;
                deviceInfo->hasShaderDrawParameters = true;
                score += 10;
            }
            else if (strcmp(name, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) == 0)
            {
                hasExtendedDynamicState = true;
                deviceInfo->hasExtendedDynamicState = true;
                score += 10;
            }
        }

        if (!hasDynamicRendering)
            SDL_Log("WARNING: VK_KHR_dynamic_rendering is MISSING!\n");

        if (!hasSynchronization2)
            SDL_Log("WARNING: VK_KHR_synchronization2 is MISSING!\n");

        if (!hasKhrDescriptorIndexing)
            SDL_Log("WARNING: KHR Descriptor Indexing support is MISSING!\n");

        if(!hasExtDescriptorIndexing)
            SDL_Log("WARNING: EXT Descriptor Indexing support is MISSING!\n");

        if (!hasShaderDrawParameters)
            SDL_Log("Note: VK_KHR_shader_draw_parameters is missing (optional)\n");
    }
    else
    {
        SDL_Log("Failed to enumerate device extensions: %d\n", result); 
    }

    return(score);
}