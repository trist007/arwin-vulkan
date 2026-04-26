#pragma once

#include "vk_types.h"
#include "vk_engine.h"
#include "vk_loader.h"
#include "vk_text.h"
#include <vulkan/vulkan_core.h>

#define MAX_DEBUG_LINES 16

// ================================================================
// Simple types
// ================================================================

typedef struct LineVertex
{
    HMM_Vec3 position;
} LineVertex;

typedef struct LinePipeline
{
    // SDL_GPU version (you can keep or remove if fully Vulkan)
    // SDL_GPUGraphicsPipeline *Pipeline;
    // SDL_GPUTransferBuffer   *transferBuffer;
    // SDL_GPUBuffer           *LineVertexBuffer;

    struct LineVertex   LineVerts[MAX_DEBUG_LINES * 2];
    int          LineVertCount;
} LinePipeline;

typedef struct pipelineObjects
{
    LinePipeline lp;
} pipelineObjects;

typedef struct Tri { int v[3]; } Tri;

// ================================================================
// Animation & Skeleton
// ================================================================

// per joint
typedef struct Joint
{
    HMM_Mat4  inverseBindMatrix;   // column-major, compatible with HMM_Mat4
    int       parent;                  // -1 if root
    char      name[64];
    HMM_Vec3  defaultTranslation;
    HMM_Quat  defaultRotation;         // Changed to HMM_Quat (better than Vec4)
    HMM_Vec3  defaultScale;
} Joint;

// animation keyframes
typedef struct Keyframe
{
    float    time;
    HMM_Vec4 value;   // vec3 for trans/scale (w=0), quat for rotation
} Keyframe;

typedef struct AnimChannel
{
    int       jointIndex;
    int       type;        // 0=translation, 1=rotation, 2=scale
    Keyframe *keyframes;
    int       keyframeCount;
} AnimChannel;

typedef struct Animation
{
    char         name[64];
    AnimChannel *channels;
    int          channelCount;
    float        duration;
} Animation;

typedef struct Material
{
    HMM_Vec4 baseColorFactor;
    // We can add more later: metallicFactor, roughnessFactor, etc.
} Material;

typedef struct Primitive
{
    int          vertOffset;
    int          triOffset;
    int          triCount;
    int          materialIndex;
    unsigned int color;   // keep as unsigned int if you prefer ARGB packing
} Primitive;

// mesh
typedef struct Mesh
{
    Vertex    *verts;
    int        vertCount;
    Tri       *tris;
    int        triCount;

    Primitive  primitives[16];
    int        primitiveCount;

    Material   materials[8];
    int        materialCount;
} Mesh;

// Skeleton
typedef struct Skeleton
{
    Joint *joints;
    int    jointCount;
} Skeleton;

typedef struct Transform
{
    HMM_Vec3 translation;
    HMM_Quat rotation;      // quaternion
    HMM_Vec3 scale;
} Transform;

typedef struct Pose
{
    Transform *joints;         // local space transforms
    HMM_Mat4  *skinMatrices;   // final skinning matrices (one per joint)
    int        jointCount;
} Pose;

// Background (if still needed)
typedef struct Background
{
    // SDL_GPUTexture          *backgroundTexture;   // remove if fully Vulkan
    // SDL_GPUSampler          *backgroundSampler;
    // SDL_GPUGraphicsPipeline *backgroundPipeline;
    int width;
    int height;
} Background;

// ================================================================
// Main Model
// ================================================================

typedef struct Model
{
    // CPU side
    Mesh       mesh;
    Skeleton   skeleton;
    Animation *animations;
    Pose       pose;
    int        animCount;
    int        jointCount;

    // GPU side - Vulkan
    VkBuffer          vertexBuffer;
    VmaAllocation     vertexBufferAllocation;
    VkDeviceSize      vertexBufferSize;
    VkDeviceSize      indexBufferOffset;

    VkBuffer          indexBuffer;
    VmaAllocation     indexBufferAlloc;
    VkDeviceSize      indexBufferSize;

    uint32_t          indexCount;
    VkIndexType       indexType;

    // Skinning buffer (SSBO recommended)
    VkBuffer          skinningBuffer;
    VmaAllocation     skinningBufferAlloc;
    void*             skinningMapped;

    // Per-model uniform buffer (model matrix, etc.)
    VkBuffer          uniformBuffer;
    VmaAllocation     uniformBufferAlloc;
    void*             uniformMapped;

    // glTF textures
    VkImage     textureImage;
    VkImageView textureImageView;
    VkSampler   textureSampler;
} Model;

typedef struct Player
{
    HMM_Vec3 position;
} Player;

typedef struct GameState
{
    Arena *arena;
    Model model;
    Model room;
    Player player;
    HMM_Vec3 position;
    HMM_Vec3 velocity;
} GameState;


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
struct FontAtlas;

Model load_gltf_model(Arena* arena, const char* path);   // or Model if you renamed it
bool upload_model_to_gpu(struct VulkanEngine *engine, struct Model *model);
bool LoadFontAtlas(struct VulkanEngine *engine, struct FontAtlas *atlas);

// You should implement these using HMM:
void extract_materials(Arena *arena, cgltf_data* data, Mesh* mesh);
void extract_skeleton(Arena *arena, cgltf_data* data, Skeleton* skeleton);
void extract_animations(Arena *arena, cgltf_data* data, Model* model);
// void update_pose(Model* model, float time);   // example
static VkCommandBuffer begin_single_time_commands(struct VulkanEngine* engine);
static void end_single_time_commands(struct VulkanEngine* engine, VkCommandBuffer commandBuffer);