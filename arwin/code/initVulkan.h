#include "vk_types.h"

#define MAX_PHYSICAL_DEVICES 2
#define MAX_PHYSICAL_DEVICE_EXTENSIONS 256

typedef struct DeviceInformation
{
    char name[256];
    uint32_t queueFamilyIndex;
    bool hasDynamicRendering;
    bool hasSynchronization2;
    bool hasKhrDescriptorIndexing;
    bool hasExtDescriptorIndexing;
    bool hasShaderDrawParameters;
    bool hasExtendedDynamicState;
} DeviceInformation;

uint32_t enableExtCount(struct DeviceInformation *deviceInfo);
uint32_t getDeviceCount(VkInstance instance, VkSurfaceKHR surface);
bool createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkDevice *outDevice, uint32_t numExt, struct DeviceInformation *deviceInfo);
bool mincreateLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkDevice *outDevice, uint32_t numExt, struct DeviceInformation *deviceInfo);
void getVulkanInstanceVersion();
int evalDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice device, DeviceInformation *deviceInfo);