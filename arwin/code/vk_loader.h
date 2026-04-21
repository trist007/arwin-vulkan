#pragma once

#include "vk_types.h"
#include "vk_engine.h"
#include "vk_loader.h"
#include "vk_text.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

#define MAX_DEBUG_LINES 16

// ================================================================
// Simple types
// ================================================================

struct LineVertex
{
    HMM_Vec3 position;
};

struct LinePipeline
{
    // SDL_GPU version (you can keep or remove if fully Vulkan)
    // SDL_GPUGraphicsPipeline *Pipeline;
    // SDL_GPUTransferBuffer   *transferBuffer;
    // SDL_GPUBuffer           *LineVertexBuffer;

    LineVertex   LineVerts[MAX_DEBUG_LINES * 2];
    int          LineVertCount;
};

struct pipelineObjects
{
    LinePipeline lp;
};

struct Tri { int v[3]; };

// ================================================================
// Animation & Skeleton
// ================================================================

// per joint
struct Joint
{
    HMM_Mat4  inverseBindMatrix;   // column-major, compatible with HMM_Mat4
    int       parent;                  // -1 if root
    char      name[64];
    HMM_Vec3  defaultTranslation;
    HMM_Quat  defaultRotation;         // Changed to HMM_Quat (better than Vec4)
    HMM_Vec3  defaultScale;
};

// animation keyframes
struct Keyframe
{
    float    time;
    HMM_Vec4 value;   // vec3 for trans/scale (w=0), quat for rotation
};

struct AnimChannel
{
    int       jointIndex;
    int       type;        // 0=translation, 1=rotation, 2=scale
    Keyframe *keyframes;
    int       keyframeCount;
};

struct Animation
{
    char         name[64];
    AnimChannel *channels;
    int          channelCount;
    float        duration;
};

struct Material
{
    HMM_Vec4 baseColorFactor = HMM_V4(1.0f, 1.0f, 1.0f, 1.0f);
    // We can add more later: metallicFactor, roughnessFactor, etc.
};

struct Primitive
{
    int          vertOffset;
    int          triOffset;
    int          triCount;
    int          materialIndex = 0;
    unsigned int color;   // keep as unsigned int if you prefer ARGB packing
};

// mesh
struct Mesh
{
    Vertex    *verts;
    int        vertCount;
    Tri       *tris;
    int        triCount;

    Primitive  primitives[16];
    int        primitiveCount;

    Material   materials[8];
    int        materialCount = 0;
};

// Skeleton
struct Skeleton
{
    Joint *joints;
    int    jointCount;
};

struct Transform
{
    HMM_Vec3 translation;
    HMM_Quat rotation;      // quaternion
    HMM_Vec3 scale;
};

struct Pose
{
    Transform *joints;         // local space transforms
    std::vector<HMM_Mat4>  skinMatrices;   // final skinning matrices (one per joint)
    int        jointCount;
};

// Background (if still needed)
struct Background
{
    // SDL_GPUTexture          *backgroundTexture;   // remove if fully Vulkan
    // SDL_GPUSampler          *backgroundSampler;
    // SDL_GPUGraphicsPipeline *backgroundPipeline;
    int width;
    int height;
};

// ================================================================
// Main Model
// ================================================================

struct Model
{
    // CPU side
    Mesh       mesh;
    Skeleton   skeleton;
    Animation *animations;
    Pose       pose;
    int        animCount;
    int        jointCount;

    // GPU side - Vulkan
    VkBuffer          vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation     vertexBufferAllocation = VK_NULL_HANDLE;
    VkDeviceSize      vertexBufferSize = 0;
    VkDeviceSize      indexBufferOffset = 0;

    VkBuffer          indexBuffer = VK_NULL_HANDLE;
    VmaAllocation     indexBufferAlloc = VK_NULL_HANDLE;
    VkDeviceSize      indexBufferSize = 0;

    uint32_t          indexCount = 0;
    VkIndexType       indexType  = VK_INDEX_TYPE_UINT32;

    // Skinning buffer (SSBO recommended)
    VkBuffer          skinningBuffer = VK_NULL_HANDLE;
    VmaAllocation     skinningBufferAlloc = VK_NULL_HANDLE;
    void*             skinningMapped = nullptr;

    // Per-model uniform buffer (model matrix, etc.)
    VkBuffer          uniformBuffer = VK_NULL_HANDLE;
    VmaAllocation     uniformBufferAlloc = VK_NULL_HANDLE;
    void*             uniformMapped = nullptr;

    // glTF textures
    VkImage     textureImage = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler   textureSampler = VK_NULL_HANDLE;
};

struct Player
{
    HMM_Vec3 position;
};

struct GameState
{
    Arena *arena;
    Model model;
    Model room;
    Player player;
};


// ================================================================
// Small helpers (now static inline and safe)
// ================================================================

static inline HMM_Mat4 mat4_from_float16(const float m[16])
{
    HMM_Mat4 result;

    // Handmade Math stores matrices in column-major order, just like glTF
    result.Columns[0] = HMM_V4(m[0],  m[1],  m[2],  m[3]);
    result.Columns[1] = HMM_V4(m[4],  m[5],  m[6],  m[7]);
    result.Columns[2] = HMM_V4(m[8],  m[9],  m[10], m[11]);
    result.Columns[3] = HMM_V4(m[12], m[13], m[14], m[15]);

    return result;
}

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }

static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }

static inline void read_float_n(cgltf_accessor* acc, int index, float* out, int n)
{
    cgltf_accessor_read_float(acc, index, out, n);
}

static inline void read_uint(cgltf_accessor* acc, int index, unsigned int* out)
{
    cgltf_accessor_read_uint(acc, index, out, 1);
}

// ================================================================
// Forward declarations for your extraction functions
// ================================================================

struct VulkanEngine;
Model load_gltf_model(Arena* arena, const char* path);   // or Model if you renamed it
bool upload_model_to_gpu(VulkanEngine* engine, Model* model);
bool LoadFontAtlas(VulkanEngine *engine, FontAtlas *atlas);

// You should implement these using HMM:
void extract_materials(cgltf_data* data, Mesh* mesh);
void extract_skeleton(cgltf_data* data, Skeleton* skeleton);
void extract_animations(cgltf_data* data, Model* model);
// void update_pose(Model* model, float time);   // example
static VkCommandBuffer begin_single_time_commands(VulkanEngine* engine);
static void end_single_time_commands(VulkanEngine* engine, VkCommandBuffer commandBuffer);