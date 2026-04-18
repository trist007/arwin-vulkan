#pragma once
#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"
#include <SDL3/SDL.h>
#include "tiny_obj_loader.h"

#include "slang/slang.h"
#include "slang/slang-com-ptr.h"

#define MAX_SWAPCHAIN_IMAGES 8
#define MAX_TEXTURES 3

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
    HMM_Mat4 model[3];

    HMM_Vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
    uint32_t selected;
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
    VkSemaphore swapchainSemaphore;
    VkSemaphore renderSemaphore;
    VkFence renderFence;

    VkCommandBuffer mainCommandBuffer;

    DeletionQueue deletionQueue;
    DescriptorAllocatorGrowable frameDescriptors;

    // howtoVulkan
    ShaderDataBuffer shaderDataBuffers; // GPU buffer
    ShaderData shaderData;              // CPU data
    VkCommandBuffer commandBuffers{VK_NULL_HANDLE};
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

struct VulkanEngine;

struct GLTFMetallic_Roughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		HMM_Vec4 colorFactors;
		HMM_Vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		HMM_Vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine *engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

struct RenderObject
{
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance *material;

    HMM_Mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct MeshAsset;

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};

/*
struct HowtoVulkanEngine {
    // === Core Vulkan handles ===
    VkInstance instance{ VK_NULL_HANDLE };
    VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
    VkDevice device{ VK_NULL_HANDLE };
    VkQueue graphicsQueue{ VK_NULL_HANDLE };
    uint32_t graphicsQueueFamily{ 0 };

    VkSurfaceKHR surface{ VK_NULL_HANDLE };
    VkSwapchainKHR swapchain{ VK_NULL_HANDLE };

    // === VMA ===
    VmaAllocator allocator{ VK_NULL_HANDLE };

    // === Swapchain related ===
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat{ VK_FORMAT_UNDEFINED };
    VkExtent2D swapchainExtent{};

    // === Depth buffer ===
    VkImage depthImage{ VK_NULL_HANDLE };
    VmaAllocation depthImageAllocation{ VK_NULL_HANDLE };
    VkImageView depthImageView{ VK_NULL_HANDLE };
    VkFormat depthFormat{ VK_FORMAT_UNDEFINED };

    // === Command buffers & sync ===
    VkCommandPool commandPool{ VK_NULL_HANDLE };
    std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers{};
    std::array<VkFence, maxFramesInFlight> fences{};
    std::array<VkSemaphore, maxFramesInFlight> imageAvailableSemaphores{};   // renamed from presentSemaphores
    std::vector<VkSemaphore> renderFinishedSemaphores{};                     // one per swapchain image

    bool framebufferResized{ false };   // for resize handling

    // === Mesh data (your suzanne.obj) ===
    VkBuffer vertexBuffer{ VK_NULL_HANDLE };      // combined vertex + index buffer
    VmaAllocation vertexBufferAllocation{ VK_NULL_HANDLE };
    VkDeviceSize indexOffset{ 0 };                // byte offset where indices start in the buffer
    uint32_t indexCount{ 0 };

    // === Uniform / Shader data buffers (per-frame) ===
    struct ShaderDataBuffer {
        VkBuffer buffer{ VK_NULL_HANDLE };
        VmaAllocation allocation{ VK_NULL_HANDLE };
        VmaAllocationInfo allocationInfo{};
        VkDeviceAddress deviceAddress{ 0 };
    };
    std::array<ShaderDataBuffer, maxFramesInFlight> shaderDataBuffers{};

    // === Textures ===
    struct Texture {
        VkImage image{ VK_NULL_HANDLE };
        VmaAllocation allocation{ VK_NULL_HANDLE };
        VkImageView view{ VK_NULL_HANDLE };
        VkSampler sampler{ VK_NULL_HANDLE };
    };
    std::array<Texture, 3> textures{};

    // === Descriptors ===
    VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };   // renamed from descriptorSetLayoutTex
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };               // renamed from descriptorSetTex

    // === Pipeline ===
    VkPipeline pipeline{ VK_NULL_HANDLE };
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

    // === Shader compiler (Slang) ===
    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;

    // === Other useful state ===
    SDL_Window* window{ nullptr };
    glm::ivec2 windowSize{ 1280, 720 };
    glm::vec3 cameraPosition{ 0.0f, 0.0f, -6.0f };
    glm::vec3 objectRotations[3]{};
    uint32_t selectedObject{ 1 };

    bool isInitialized{ false };
};
*/
struct Texture
{
    VmaAllocation allocation;
    VkImage image;
    VkImageView view;
    VkSampler sampler;
};

struct VulkanEngine
{
    // general
    /*
    bool isInitialized;
    bool stop_rendering;
    int frameNumber;

    // vulkan core
    VkExtent2D windowExtent;
    SDL_Window *window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkDevice device;
    VkPhysicalDevice chosenGPU;
    VkFormat swapchainImageFormat;
    uint32_t swapchainImageCount;
    VkExtent2D swapchainExtent;

    // queues
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
    DeletionQueue deletionQueue;
    DeletionQueue mainDeletionQueue;
    VmaAllocator allocator;

    // draw resources
    AllocatedImage drawImage;
    AllocatedImage depthImage;
    VkExtent2D drawExtent;
    float renderScale = 1.0f;

    // VkDescriptor
    DescriptorAllocatorGrowable globalDescriptorAllocator;
    VkDescriptorSet drawImageDescriptors;
    VkDescriptorSetLayout drawImageDescriptorLayout;

    // VkPipeline
    VkPipeline gradientPipeline;
    VkPipelineLayout gradientPipelineLayout;

    // immediate submit structures
    VkFence immFence;
    VkCommandBuffer immCommandBuffer;
    VkCommandPool immCommandPool;

    // Compute Effects
    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect;

    // Graphics pipeline
    // VkPipelineLayout trianglePipelineLayout;
    // VkPipeline trianglePipeline;

    // Mesh
    VkPipelineLayout meshPipelineLayout;
    VkPipeline meshPipeline;

    // GPUMeshBuffers rectangle;
    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    bool resize_requested;

    GPUSceneData sceneData;
    VkDescriptorSetLayout gpuSceneDataDescriptorLayout;
    VkDescriptorSetLayout singleImageDescriptorLayout;

    // images
    AllocatedImage whiteImage;
    AllocatedImage blackImage;
    AllocatedImage greyImage;
    AllocatedImage errorCheckerboardImage;

    VkSampler defaultSamplerLinear;
    VkSampler defaultSamplerNearest;

    MaterialInstance defaultData;
    GLTFMetallic_Roughness metalRoughMaterial;

    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

    Camera mainCamera;
    */

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

    // queues
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
    DeletionQueue deletionQueue;
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
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    // textures
    Texture textures[MAX_TEXTURES];
    uint32_t textureCount = MAX_TEXTURES;
    
    // meshes
    uint32_t indexCount = 0;

    // descriptors
    VkDescriptorSetLayout descriptorSetLayoutTex = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSetTex = VK_NULL_HANDLE;

    // model
    HMM_Vec3 camPos{0.0f, 0.0f, -6.0f};
    HMM_Vec3 objectRotations[3]{};
    ivec2 windowSize();

    // slang
    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    VkShaderModule shaderModule{};

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

};

struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const HMM_Mat4& topMatrix, DrawContext& ctx) override;
};

// singleton for pointer retrieval
VulkanEngine *getVulkanEngine(void);

// VkBootstrap functions
/*
	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface
*/

// init functions
void init_vulkan(VulkanEngine *engine);
void init_swapchain(VulkanEngine *engine);
void init_commands(VulkanEngine *engine);
void init_sync_structures(VulkanEngine *engine);
void init_descriptors(VulkanEngine *engine);
void init_pipelines(VulkanEngine *engine);
void init_background_pipelines(VulkanEngine *engine);
void init_imgui(VulkanEngine *engine);
void init_triangle_pipeline(VulkanEngine *engine);
void init_mesh_pipeline(VulkanEngine *engine);

void create_swapchain(VulkanEngine *engine, uint32_t width, uint32_t height);
void destroy_swapchain(VulkanEngine *engine);
void draw_background(VulkanEngine *engine, VkCommandBuffer cmd);
void draw_geometry(VulkanEngine *engine, VkCommandBuffer cmd);
void immediate_submit(VulkanEngine *engine, std::function<void(VkCommandBuffer cmd)>&& function);
void draw_imgui(VulkanEngine *engine, VkCommandBuffer cmd, VkImageView targetImageView);
void resize_swapchain(VulkanEngine *engine);
void update_scene(VulkanEngine *engine);

void initVulkanEngine(VulkanEngine *engine);
void cleanupVulkanEngine(VulkanEngine *engine);
void howtoCleanupVulkanEngine(VulkanEngine *engine);

void drawVulkanEngine(VulkanEngine *engine);
void drawHowtoVulkanEngine(VulkanEngine *engine);
void runVulkanEngine(VulkanEngine *engine);
FrameData *getCurrentFrame(VulkanEngine *engine);

// Mesh buffers
AllocatedBuffer create_buffer(VulkanEngine *engine, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
void destroy_buffer(VulkanEngine *engine, const AllocatedBuffer &buffer);
GPUMeshBuffers uploadMesh(VulkanEngine *engine, std::span<uint32_t> indices, std::span<Vertex> vertices);
void init_default_data(VulkanEngine *engine);

// Textures
AllocatedImage create_image(VulkanEngine *engine, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
AllocatedImage create_image(VulkanEngine *engine, void *data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
void destroy_image(VulkanEngine *engine, const AllocatedImage &img);

bool howtoVulkan(VulkanEngine *engine);