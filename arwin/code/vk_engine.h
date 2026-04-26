#pragma once
#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"
#include "vk_text.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan_core.h>
#include "tiny_obj_loader.h"

#include "slang/slang.h"
#include "slang/slang-com-ptr.h"

#define MAX_SWAPCHAIN_IMAGES 4
#define MAX_TEXTURES 3
#define MAX_DEPTH_FORMATS 3

// ! NOTE: trist007: deque is a double ended Queue that allows fast insertion and deletion at both
// ! front and back ends, unlike a regular array which is only fast at the back
// ! std::deque works like a mix between a vector and a linked list
/* Example of a deque impl
std::deque<int> d;
d.push_back(1);   // add to back
d.push_front(2);  // add to front
d.pop_back();     // remove from back
d.pop_front();    // remove from front
*/

// ====================== DELETION QUEUE ======================

#define DELETION_QUEUE_INITIAL_CAPACITY 64

typedef void (*DeletionFunc)(void* userdata);

struct DeletionEntry
{
    DeletionFunc func;
    void*        userdata;
};

struct DeletionQueue
{
    DeletionEntry* entries;
    uint32_t count;
    uint32_t capacity;
};

// For image + view + allocation
struct ImageDeletion
{
    VkDevice device;
    VkImageView view;
    VmaAllocator allocator;
    VkImage image;
    VmaAllocation allocation;
};

struct AllocatedImageDeletion
{
    VkDevice       device;
    VkImageView    imageView;
    VmaAllocator   allocator;
    VkImage        image;
    VmaAllocation  allocation;
};


/*
// this is a poor performance way of doing callbacks as it is inefficient at scale storing
// whole std::functions for every object, but will suffice for our exercise
// ! NOTE: trist007: if you need to delete thousands of objects a better impl
// ! would be to store arrays of vulkan handles of various types such as VkImage, VkBuffer,
// ! and so on and then delete those from a loop
struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    // this pushes a lambda meant for deletion
    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(function); // adds it to the back of the deque
    }

    // this starts at the last/most recent element that was added to the queue and calls
    // the lambda to delete, in Vulkan you need to delete in reverse order
    void flush()
    {
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) // last added first destroyed
        {
            (*it)();
        }
        deletors.clear();
    }
};
*/

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

    HMM_Vec4 lightPos = HMM_V4(5.0f, 10.0f, 10.0f, 1.0f);   // better default position

    // Per-material data - we'll use index 0 for now (single model)
    //HMM_Vec4 baseColorFactor = HMM_V4(1.0f, 1.0f, 1.0f, 1.0f);
    HMM_Vec4 baseColorFactor[8];
    uint32_t selected;
    HMM_Mat4 skinMatrices[64];
};

struct ShaderDataBuffer
{
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo allocationInfo{};
    VkBuffer buffer{ VK_NULL_HANDLE };
    VkDeviceAddress deviceAddress{};
};

struct FrameData
{
    DeletionQueue deletionQueue;
    DescriptorAllocatorGrowable frameDescriptors;

    // howtoVulkan
    VkSemaphore swapchainSemaphore;
    VkSemaphore renderSemaphore;
    VkFence renderFence;

    ShaderDataBuffer shaderDataBuffers; // GPU buffer
    ShaderData shaderData;              // CPU data
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
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

    ComputePushConstants data;
};

struct ivec2
{
    int x, y;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct Texture
{
    VmaAllocation allocation;
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
    VkExtent2D swapchainExtent{};
    VkExtent2D windowExtent;
    SDL_Window *window;
    VkSurfaceKHR surface{ VK_NULL_HANDLE };
    VkPhysicalDevice chosenGPU;
    uint32_t swapchainImageCount;
    VkSemaphore renderSemaphores[MAX_SWAPCHAIN_IMAGES];

    // queues
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
    DeletionQueue mainDeletionQueue;

    // VMA
    VmaAllocator allocator;
    VkBuffer vBuffer = VK_NULL_HANDLE;
    VmaAllocation vBufferAllocation = VK_NULL_HANDLE;
    VkDeviceSize vertexBufferSize = 0;
    VkDeviceSize indexBufferOffset = 0;

    // pools
    VkCommandPool commandPool{ VK_NULL_HANDLE };

    // frames
    FrameData frames[FRAME_OVERLAP];
    uint32_t imageIndex{};
    uint32_t frameIndex{};

    // tiny obj loader
    //tinyobj::attrib_t attrib;
    //std::vector<tinyobj::shape_t> shapes;
    //std::vector<tinyobj::material_t> materials;

    // textures
    Texture textures[MAX_TEXTURES];
    uint32_t textureCount = MAX_TEXTURES;
    
    // meshes
    uint32_t indexCount = 0;

    // descriptors
    VkDescriptorSetLayout descriptorSetLayoutTex = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSetTex = VK_NULL_HANDLE;

    VkDescriptorSetLayout textDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool textDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       textDescriptorSet = VK_NULL_HANDLE;

    // model
    HMM_Vec3 camPos{0.0f, 0.0f, -6.0f};
    HMM_Vec3 objectRotations[3]{};
    ivec2 windowSize();

    // slang
    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    VkShaderModule shaderModule{};
    VkShaderModule textShaderModule{};

    // swapchain
    VkImage swapchainImages[MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchainImageViews[MAX_SWAPCHAIN_IMAGES];
    //VkImage depthImage{ VK_NULL_HANDLE };
    AllocatedImage depthImage{ VK_NULL_HANDLE };
    VmaAllocation depthImageAllocation{ VK_NULL_HANDLE };
    AllocatedImage drawImage;
    VkFormat swapchainImageFormat;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkImageView depthImageView{ VK_NULL_HANDLE };

    // pipeline
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    // text rendering with Vulkan
    VkPipeline      textPipeline = VK_NULL_HANDLE;
    VkPipelineLayout textPipelineLayout = VK_NULL_HANDLE;

    // Font atlas
    FontAtlas       fontAtlas;

    // text rendering with OpenGL
    GameFont font;
    int pixelHeight = 24;
    VkBuffer  stagingBuffer = VK_NULL_HANDLE;
    VkBuffer  textStagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAlloc = VK_NULL_HANDLE;
    VmaAllocation textStagingAlloc = VK_NULL_HANDLE;
    int textVertexCountThisFrame = 0;
    
    // memory arena
    Arena *arena;
};

// singleton for pointer retrieval
VulkanEngine *getVulkanEngine(void);

struct GameState;
// init functions
void init_vulkan(VulkanEngine *engine);
void init_swapchain(VulkanEngine *engine);
bool init_mezzanine(VulkanEngine *engine, GameState *gameState);

// init_mezzanine
bool load_and_upload_gltf_models(VulkanEngine *engine, GameState *gameState);
bool create_per_frame_uniform_buffers(VulkanEngine *engine);
bool create_main_3d_descriptor_layout_and_set(VulkanEngine *engine);
bool create_main_graphics_pipeline(VulkanEngine *engine);
bool setup_font_atlas_and_text_pipeline(VulkanEngine *engine);

// main render loop
void mainRenderLoop(VulkanEngine *engine, GameState *gameState);
void update_shader_data(VulkanEngine *engine, GameState *gameState);
void begin_frame(VulkanEngine *engine);
void begin_rendering(VulkanEngine *engine, VkCommandBuffer cmd);
void draw_3d_scene(VulkanEngine *engine, GameState *gameState, VkCommandBuffer cmd);
void draw_ui_text(VulkanEngine *engine, VkCommandBuffer cmd);
void end_rendering(VulkanEngine *engine, VkCommandBuffer cmd);
void submit_and_present(VulkanEngine *engine);

void create_swapchain(VulkanEngine *engine, uint32_t width, uint32_t height);
void destroy_swapchain(VulkanEngine *engine);
void draw_imgui(VulkanEngine *engine, VkCommandBuffer cmd, VkImageView targetImageView);
void resize_swapchain(VulkanEngine *engine);

void initVulkanEngine(VulkanEngine *engine, GameState *gameState);
void cleanupVulkanEngine(VulkanEngine *engine);
void howtoCleanupVulkanEngine(VulkanEngine *engine);

void runVulkanEngine(VulkanEngine *engine, GameState *gameState);
FrameData *getCurrentFrame(VulkanEngine *engine);
void RenderText(VulkanEngine *engine, VkCommandBuffer cmd, FontAtlas *atlas,
const char *text, float x, float y, float red = 1.0f, float green = 0.0f, float blue = 0.0f);

// Textures
AllocatedImage create_image(VulkanEngine *engine, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
AllocatedImage create_image(VulkanEngine *engine, void *data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
void destroy_image(VulkanEngine *engine, const AllocatedImage &img);

// DeletionQueue
void deletion_queue_init(DeletionQueue *queue);
void deletion_queue_push(DeletionQueue *queue, DeletionFunc func, void* userdata);
void deletion_queue_flush(DeletionQueue *queue);
void deletion_queue_destroy(DeletionQueue *queue);
void destroy_allocated_image(void* userdata);
void deletion_queue_push_allocated_image(DeletionQueue *queue, VkDevice device, VkImageView view, VmaAllocator allocator, VkImage image, VmaAllocation allocation);

