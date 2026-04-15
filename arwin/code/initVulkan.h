#include "vk_types.h"

#define MAX_PHYSICAL_DEVICES 2
#define MAX_PHYSICAL_DEVICE_EXTENSIONS 256

#define ArraySize(a) (sizeof(a) / sizeof(a[0]))

typedef uint32_t u32;

struct DeviceInformation
{
    char name[256];
    uint32_t queueFamilyIndex;
    bool hasDynamicRendering;
    bool hasSynchronization2;
    bool hasKhrDescriptorIndexing;
    bool hasExtDescriptorIndexing;
    bool hasShaderDrawParameters;
    bool hasExtendedDynamicState;
};

uint32_t enableExtCount(DeviceInformation *deviceInfo);
uint32_t getDeviceCount(VkInstance instance, VkSurfaceKHR surface);
bool createLogicalDevice(VkPhysicalDevice physicalDevice, u32 queueFamilyIndex, VkDevice *outDevice, uint32_t numExt, DeviceInformation *deviceInfo);
bool mincreateLogicalDevice(VkPhysicalDevice physicalDevice, u32 queueFamilyIndex, VkDevice *outDevice, uint32_t numExt, DeviceInformation *deviceInfo);
void getVulkanInstanceVersion();
int evalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice device, DeviceInformation *deviceInfo);