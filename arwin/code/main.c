#include "vk_engine.h"

int main(int argc,  char *argv[])
{
    // Game Storage
    static uint8_t gameStorage[16 * 1024 * 1024];
    Arena gameArena; 
    arenaInit(&gameArena, gameStorage, sizeof(gameStorage));

    GameState gamestate = {0};

    gamestate.arena = &gameArena;

    // initializes everything to 0
    struct VulkanEngine engine = {0};

    initVulkanEngine(&engine, &gamestate);

    runVulkanEngine(&engine, &gamestate);

    howtoCleanupVulkanEngine(&engine);

    //cleanupVulkanEngine(&engine);

    return 0;
}