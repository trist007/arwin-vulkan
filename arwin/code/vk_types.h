#pragma once

#include <math.h>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

#include "HandmadeMath.h"
#include "SDL3/SDL.h"

#include "arena.h"
#include "cgltf.h"


#define ArraySize(arr) (uint32_t)(sizeof(arr) / sizeof(arr[0]))
#define CLAMP(v, low, high) ((v) < (low) ? (low) : (v) > (high) ? (high) : (v))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define MAX_BINDINGS 24
#define MAX_ATTRIBUTES 16


/*
#ifdef DEBUG
#define assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define assert(Expression)
#endif
*/

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            SDL_Log("Detected Vulkan error: %s", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

typedef struct AllocatedImage
{
    VkImage image;
    VkImageView imageView ;
    VmaAllocation allocation;

    VkExtent3D imageExtent;
    VkFormat imageFormat;
} AllocatedImage;

typedef struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
} AllocatedBuffer;

typedef struct VertexInputDescription
{
    VkVertexInputBindingDescription bindings[MAX_BINDINGS];
    VkVertexInputAttributeDescription attributes[MAX_ATTRIBUTES];

    uint32_t bindingCount;
    uint32_t attributeCount;
} VertexInputDescription;

typedef struct PushColor {
    float r, g, b, a;
} PushColor;

// Mesh Buffers on GPU
typedef struct Vertex
{
    HMM_Vec3 position;
    HMM_Vec3 normal;
    HMM_Vec2 texcoord;

    HMM_Vec4 color;

    // skinning
    uint8_t   joints[4];
    float weights[4];

} Vertex;

static VertexInputDescription get_vertex_description();

// holds the resources needed for a mesh
typedef struct GPUMeshBuffers
{
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
} GPUMeshBuffers;

// push constants for our mesh object draws
typedef struct GPUDrawPushConstants
{
    HMM_Mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
} GPUDrawPushConstants;