#ifndef VK_ENGINE_H
#define VK_ENGINE_H

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"
#include "vk_text.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan_core.h>
//#include "tiny_obj_loader.h"

//#include "slang/slang.h"
//#include "slang/slang-com-ptr.h"

#define FRAME_OVERLAP 2
#define MAX_SWAPCHAIN_IMAGES 4
#define MAX_TEXTURES 3
#define MAX_DEPTH_FORMATS 3

#define DELETION_QUEUE_INITIAL_CAPACITY 64

typedef void (*DeletionFunc)(void* userdata);

struct DeletionEntry
{
    DeletionFunc func;
    void*        userdata;
};

struct DeletionQueue
{
    struct DeletionEntry* entries;
    uint32_t count;
    uint32_t capacity;
};

// For image + view + allocation
struct ImageDeletion
{
    VkDevice device;
    VkImageView view;
    VkImage image;
};

struct AllocatedImageDeletion
{
    VkDevice       device;
    VkImageView    imageView;
    VkImage        image;
};


struct GPUSceneData
{
    HMM_Mat4 view;
    HMM_Mat4 proj;
    HMM_Mat4 viewproj;
    HMM_Vec4 ambientColor;
    HMM_Vec4 sunlightDirection;  // w for sun power
    HMM_Vec4 sunlightColor;
};


struct ShaderData
{
    HMM_Mat4 projection;
    HMM_Mat4 view;
    HMM_Mat4 model;

    HMM_Vec4 lightPos;

    // Per-material data - we'll use index 0 for now (single model)
    //HMM_Vec4 baseColorFactor = HMM_V4(1.0f, 1.0f, 1.0f, 1.0f);
    HMM_Vec4 baseColorFactor[8];
    uint32_t selected;
    HMM_Mat4 skinMatrices[64];
};

struct ShaderDataBuffer
{
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceAddress deviceAddress;
};

struct FrameData
{
    struct DeletionQueue deletionQueue;
    DescriptorAllocatorGrowable frameDescriptors;

    // howtoVulkan
    VkSemaphore swapchainSemaphore;
    VkSemaphore renderSemaphore;
    VkFence renderFence;

    struct ShaderDataBuffer shaderDataBuffers; // GPU buffer
    struct ShaderData shaderData;              // CPU data
    VkCommandBuffer commandBuffer;
    VkDescriptorSet descriptorSet;
};

struct ComputePushConstants
{
    HMM_Vec4 data1;
    HMM_Vec4 data2;
    HMM_Vec4 data3;
    HMM_Vec4 data4;
};

struct ComputeEffect
{
    const char *name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    struct ComputePushConstants data;
};

struct ivec2
{
    int x, y;
};

struct Texture
{
    VkImage image;
    VkImageView view;
    VkSampler sampler;
};

struct VulkanEngine
{
    //! NOTE(trist007): HowtoVulkan

    bool isInitialized;
    bool stop_rendering;
    bool resize_requested;
    Camera mainCamera;

    // vulkan core
    VkInstance instance;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkExtent2D swapchainExtent;
    VkExtent2D windowExtent;
    SDL_Window *window;
    VkSurfaceKHR surface;
    VkPhysicalDevice chosenGPU;
    uint32_t swapchainImageCount;
    VkSemaphore renderSemaphores[MAX_SWAPCHAIN_IMAGES];

    // queues
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
    struct DeletionQueue mainDeletionQueue;

    VkBuffer vBuffer;
    VkDeviceSize vertexBufferSize;
    VkDeviceSize indexBufferOffset;

    // pools
    VkCommandPool commandPool;

    // frames
    struct FrameData frames[FRAME_OVERLAP];
    uint32_t imageIndex;
    uint32_t frameIndex;

    // tiny obj loader
    //tinyobj::attrib_t attrib;
    //std::vector<tinyobj::shape_t> shapes;
    //std::vector<tinyobj::material_t> materials;

    // textures
    struct Texture textures[MAX_TEXTURES];
    uint32_t textureCount;
    
    // meshes
    uint32_t indexCount;

    // descriptors
    VkDescriptorSetLayout descriptorSetLayoutTex;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSetTex;

    VkDescriptorSetLayout textDescriptorSetLayout;
    VkDescriptorPool textDescriptorPool;
    VkDescriptorSet       textDescriptorSet;

    // model
    HMM_Vec3 camPos;
    HMM_Vec3 objectRotations[3];
    struct ivec2 windowSize;

    // slang
    //Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    VkShaderModule shaderModule;
    VkShaderModule textShaderModule;

    // swapchain
    VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchainImageViews[MAX_SWAPCHAIN_IMAGES];
    //VkImage depthImage{ VK_NULL_HANDLE };
    AllocatedImage depthImage;
    AllocatedImage drawImage;
    VkFormat swapchainImageFormat;
    VkFormat depthFormat;
    VkImageView depthImageView;

    // pipeline
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    // text rendering with Vulkan
    VkPipeline      textPipeline;
    VkPipelineLayout textPipelineLayout;

    // Font atlas
    FontAtlas       fontAtlas;

    // text rendering with OpenGL
    GameFont font;
    int pixelHeight;
    VkBuffer  stagingBuffer;
    VkBuffer  textStagingBuffer;
    VkDeviceSize  textStagingOffset;
    int textVertexCountThisFrame;
    
    // game memory arena
    Arena *arena;

    // vk memory arena
    struct vkArena *deviceLocalArena; // GPU
    struct vkArena *stagingArena;     // CPU

};

// singleton for pointer retrieval
struct VulkanEngine *getVulkanEngine(void);

struct GameState;
// init functions
void init_vulkan(struct VulkanEngine *engine);
void init_swapchain(struct VulkanEngine *engine);
bool init_mezzanine(struct VulkanEngine *engine, struct GameState *gamestate);

// init_mezzanine
bool load_and_upload_gltf_models(struct VulkanEngine *engine, struct GameState *gamestate);
bool create_per_frame_uniform_buffers(struct VulkanEngine *engine);
bool create_main_3d_descriptor_layout_and_set(struct VulkanEngine *engine);
bool create_main_graphics_pipeline(struct VulkanEngine *engine);
bool setup_font_atlas_and_text_pipeline(struct VulkanEngine *engine);

// main render loop
void mainRenderLoop(struct VulkanEngine *engine, struct GameState *gamestate);
void update_shader_data(struct VulkanEngine *engine, struct GameState *gamestate);
void begin_frame(struct VulkanEngine *engine);
void begin_rendering(struct VulkanEngine *engine, VkCommandBuffer cmd);
void draw_3d_scene(struct VulkanEngine *engine, struct GameState *gamestate, VkCommandBuffer cmd);
void draw_ui_text(struct VulkanEngine *engine, VkCommandBuffer cmd);
void end_rendering(struct VulkanEngine *engine, VkCommandBuffer cmd);
void submit_and_present(struct VulkanEngine *engine);

void create_swapchain(struct VulkanEngine *engine, uint32_t width, uint32_t height);
void destroy_swapchain(struct VulkanEngine *engine);
void draw_imgui(struct VulkanEngine *engine, VkCommandBuffer cmd, VkImageView targetImageView);
void resize_swapchain(struct VulkanEngine *engine);

void initVulkanEngine(struct VulkanEngine *engine, struct GameState *gamestate);
void cleanupVulkanEngine(struct VulkanEngine *engine);
void howtoCleanupVulkanEngine(struct VulkanEngine *engine);

void runVulkanEngine(struct VulkanEngine *engine, struct GameState *gamestate);
struct FrameData *getCurrentFrame(struct VulkanEngine *engine);
void RenderText(struct VulkanEngine *engine, VkCommandBuffer cmd, FontAtlas *atlas,
const char *text, float x, float y, float red, float green, float blue);

// Textures
AllocatedImage create_image(struct VulkanEngine *engine, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
//AllocatedImage create_image(struct VulkanEngine *engine, void *data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
void destroy_image(struct VulkanEngine *engine, AllocatedImage *img);
void destroy_texture(struct VulkanEngine *engine, struct Texture *tex);

// DeletionQueue
void deletion_queue_init(struct DeletionQueue *queue);
void deletion_queue_push(struct DeletionQueue *queue, DeletionFunc func, void* userdata);
void deletion_queue_flush(struct DeletionQueue *queue);
void deletion_queue_destroy(struct DeletionQueue *queue);
void destroy_allocated_image(void* userdata);
void deletion_queue_push_image(struct DeletionQueue *queue, VkDevice device, VkImageView view, VkImage image);

#endif