#include <vk_engine.h>

int main(int argc,  char *argv[])
{
    // initializes everything to 0
    VulkanEngine engine = {};

    initVulkanEngine(&engine);

    runVulkanEngine(&engine);

    cleanupVulkanEngine(&engine);
}