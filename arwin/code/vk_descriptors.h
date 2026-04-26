#pragma once

#include "vk_types.h"
#include <SDL3/SDL.h>

#define DESCRIPTOR_LAYOUT_MAX_BINDINGS 24
#define DESCRIPTOR_ALLOCATOR_MAX_RATIOS 16
#define DESCRIPTOR_ALLOCATOR_MAX_POOLS 32
#define DESCRIPTOR_WRITER_MAX_INFOS 64

struct DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding bindings[MAX_BINDINGS] = {};
    uint32_t bindingCount;

    void add_binding(DescriptorLayoutBuilder *builder, uint32_t binding,
        VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stageFlags);

    // simple if want VK_SHADER_STAGE_ALL
    void add(DescriptorLayoutBuilder* builder, uint32_t binding, VkDescriptorType type);

    void clear(DescriptorLayoutBuilder *builder);

    VkDescriptorSetLayout build(DescriptorLayoutBuilder* builder, VkDevice device, VkShaderStageFlags defaultStages,
        void* pNext, VkDescriptorSetLayoutCreateFlags flags);
};

/*
struct DescriptorAllocator
{
    struct PoolSizeRatio
    {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device, uint32_t maxSets, PoolSizeRatio *poolRatios, uint32_t ratioCount);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};
*/

struct DescriptorAllocatorGrowable
{
    struct PoolSizeRatio
    {
        VkDescriptorType type;
        float ratio;
    };

    void init_pool(DescriptorAllocatorGrowable *alloc, VkDevice device, uint32_t initialSets, PoolSizeRatio *poolRatios, uint32_t ratioCount);
    void clear_pools(DescriptorAllocatorGrowable *alloc, VkDevice device);
    void destroy_pools(DescriptorAllocatorGrowable *alloc, VkDevice device);

    VkDescriptorSet allocate(DescriptorAllocatorGrowable *alloc, VkDevice device, VkDescriptorSetLayout layout, void *pNext = nullptr);
    VkDescriptorPool get_pool(DescriptorAllocatorGrowable *alloc, VkDevice device);
    VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, PoolSizeRatio *poolRatios, uint32_t ratioCount);

    PoolSizeRatio ratios[DESCRIPTOR_ALLOCATOR_MAX_RATIOS] = {};
    uint32_t ratioCount;

    VkDescriptorPool fullPools[DESCRIPTOR_ALLOCATOR_MAX_POOLS] = {};
    uint32_t fullPoolCount;

    VkDescriptorPool readyPools[DESCRIPTOR_ALLOCATOR_MAX_POOLS] = {};
    uint32_t readyPoolCount;

    uint32_t setsPerPool;
};

struct DescriptorWriter {
    VkDescriptorImageInfo  imageInfos[DESCRIPTOR_WRITER_MAX_INFOS];
    uint32_t imageInfoCount;

    VkDescriptorBufferInfo bufferInfos[DESCRIPTOR_WRITER_MAX_INFOS];
    uint32_t bufferInfoCount;

    VkWriteDescriptorSet writes[DESCRIPTOR_WRITER_MAX_INFOS * 2];  // usually 1-2 writes per binding
    uint32_t writeCount;

    void write_image(DescriptorWriter *writer, uint32_t binding, VkImageView imageView, VkSampler sampler,
        VkImageLayout layout, VkDescriptorType type);

    void write_buffer(DescriptorWriter *writer, uint32_t binding, VkBuffer buffer, VkDeviceSize size,
        VkDeviceSize offset, VkDescriptorType type); 

    void clear(DescriptorWriter *writer);

    void update_set(DescriptorWriter *writer, VkDevice device, VkDescriptorSet set);
};