#include "vk_engine.h"

Arena gArena;


int main(int argc,  char *argv[])
{
    static uint8_t permStorage[16 * 1024 * 1024];
    gArena = {}; 
    arenaInit(&gArena, permStorage, sizeof(permStorage));

    GameState gameState = {};

    gameState.arena = &gArena;

    // initializes everything to 0
    VulkanEngine engine = {};

    initVulkanEngine(&engine, &gameState);

    runVulkanEngine(&engine, &gameState);

    howtoCleanupVulkanEngine(&engine);

    //cleanupVulkanEngine(&engine);
}