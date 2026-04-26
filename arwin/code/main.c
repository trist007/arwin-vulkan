#include "vk_engine.h"

int arwin(int argc,  char *argv[])
{
    // Game Storage
    static uint8_t gameStorage[16 * 1024 * 1024];
    Arena gameArena; 
    arenaInit(&gameArena, gameStorage, sizeof(gameStorage));

    // Engine Storage
    static uint8_t engineStorage[16 * 1024 * 1024];
    Arena engineArena;
    arenaInit(&engineArena, engineStorage, sizeof(engineStorage));

    GameState gameState = {};

    gameState.arena = &gameArena;

    // initializes everything to 0
    struct VulkanEngine engine = {};

    engine.arena = &engineArena;

    initVulkanEngine(&engine, &gameState);

    runVulkanEngine(&engine, &gameState);

    howtoCleanupVulkanEngine(&engine);

    //cleanupVulkanEngine(&engine);
}