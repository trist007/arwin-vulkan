#include <vk_engine.h>

int main(int argc,  char *argv[])
{
    VulkanEngine engine = {};

    initVulkanEngine(&engine);

    runVulkanEngine(&engine);

    cleanupVulkanEngine(&engine);
}