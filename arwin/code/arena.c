#include "arena.h"

//#################################################################################
// Game Arena
//#################################################################################

void
arenaInit(Arena *a, void *buf, size_t size)
{
    a->base   = (uint8_t *)buf;
    a->size   = size;
    a->offset = 0;
}

void *
arenaAlloc(Arena *a, size_t s)
{
    if(a->offset + s > a->size)
        return NULL;
    void *ptr = a->base + a->offset;
    a->offset += s;
    
    return ptr;
}

//#################################################################################
// Vk Arena
//#################################################################################

VkResult createVkArena(VkDevice device, uint32_t memoryTypeIndex, VkDeviceSize size, struct vkArena *arena)
{

    arena->memory = VK_NULL_HANDLE;
    arena->mapped = NULL;
    arena->offset = 0;
    arena->size = size;
    arena->memoryTypeIndex = memoryTypeIndex;

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        NULL,
        size,
        memoryTypeIndex
    };

    VkResult res = vkAllocateMemory(device, &allocInfo, NULL, arena->memory);
    if (res != VK_SUCCESS) return res;

    // Optional: persistent map for host-visible arenas
    VkMemoryPropertyFlags props = /* query from physical device */;
    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        vkMapMemory(device, arena->memory, 0, VK_WHOLE_SIZE, 0, arena->mapped);
    }
    return VK_SUCCESS;
}

Allocation arena_alloc(struct vkArena *arena, VkDeviceSize size, VkDeviceSize alignment)
{
    if(size == 0) return (struct Allocation){0};

    // Align the current offset
    VkDeviceSize aligned_offset = (arena->offset + alignment - 1) & ~(alignment - 1);

    if (aligned_offset + size > arena->size) {
        // Out of space - grow, reset, or fail depending on your policy
        // For a simple linear arena: reset at frame end or use multiple blocks
        return (struct Allocation){VK_NULL_HANDLE, 0, 0};
    }

    Allocation alloc = {
        .memory = arena->memory,
        .offset = aligned_offset,
        .size = size
    };
        
    arena->offset = aligned_offset + size;

    return alloc;
}

static inline bool allocation_valid(Allocation a)
{
    return a.memory != VK_NULL_HANDLE && a.size > 0;
}