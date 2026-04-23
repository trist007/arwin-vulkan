#pragma once

#include <vector>
#include <iostream>
#include <math.h>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

#include "HandmadeMath.h"
#include <SDL3/SDL.h>

#include "arena.h"
#include "cgltf.h"
#include "glad/glad.h"


#define ARRAYSIZE(arr) (uint32_t)(sizeof(arr) / sizeof(arr[0]))
#define CLAMP(v, low, high) ((v) < (low) ? (low) : (v) > (high) ? (high) : (v))
#define MAX(x, y) ((x) > (y) ? (x) : (y))


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

struct AllocatedImage
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    VkExtent3D imageExtent = {0, 0, 0};
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

struct PushColor {
    float r, g, b, a;
};

// Mesh Buffers on GPU
struct Vertex
{
    HMM_Vec3 position;
    HMM_Vec3 normal;
    HMM_Vec2 texcoord;

    HMM_Vec4 color = HMM_V4(1.0f, 1.0f, 1.0f, 1.0f);

    // skinning
    uint8_t   joints[4] = {0,0,0,0};
    float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    static VertexInputDescription get_vertex_description();
};

// holds the resources needed for a mesh
struct GPUMeshBuffers
{
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants
{
    HMM_Mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};