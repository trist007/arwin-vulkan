#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include "vk_mem_alloc.h"

#include "HandmadeMath.h"
#include <SDL3/SDL.h>


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
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};
struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

struct DrawContext;

class IRenderable
{
    virtual void Draw(const HMM_Mat4 &topMatrix, DrawContext &ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    HMM_Mat4 localTransform;
    HMM_Mat4 worldTransform;

    void refreshTransform(const HMM_Mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void Draw(const HMM_Mat4& topMatrix, DrawContext& ctx)
    {
        // draw children
        for (auto& c : children) {
            c->Draw(topMatrix, ctx);
        }
    }
};

struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};

// Mesh Buffers on GPU
struct Vertex
{
    HMM_Vec3 position;
    float uv_x;
    HMM_Vec3 normal;
    float uv_y;
    HMM_Vec4 color;

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