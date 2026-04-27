#ifndef ARENA_H
#define ARENA_H

#include "vk_types.h"

#include <stdint.h>
#include <stddef.h>

//#################################################################################
// Game Arena
//#################################################################################

typedef struct Arena
{
    uint8_t  *base;
    size_t   size;
    size_t   offset;
} Arena;

void arenaInit(Arena *a, void *buf, size_t size);

void *arenaAlloc(Arena *a, size_t s);

//#################################################################################
// Vk Arena
//#################################################################################

typedef struct vkArena
{
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize offset;
    uint32_t memoryTypeIndex;
    void *mapped;
} VkArena;

typedef struct Allocation
{
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize offset;
    // ! can add generation ID for safety
} Allocation;

VkResult createVkArena(VkPhysicalDevice chosenGPU, VkDevice device, uint32_t memoryTypeIndex, VkDeviceSize size, struct vkArena *arena);
Allocation arena_alloc(struct vkArena *arena, VkDeviceSize size, VkDeviceSize alignment);
bool allocation_valid(struct Allocation a);

#endif