#pragma once

#include "vk_types.h"
#include <SDL3/SDL.h>

#define DESCRIPTOR_LAYOUT_MAX_BINDINGS 24
#define DESCRIPTOR_ALLOCATOR_MAX_RATIOS 16
#define DESCRIPTOR_ALLOCATOR_MAX_POOLS 32
#define DESCRIPTOR_WRITER_MAX_INFOS 64

typedef struct DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding bindings[MAX_BINDINGS];
    uint32_t bindingCount;

} DescriptorLayoutBuilder;

void descriptorLayoutBuilder_add_binding(struct DescriptorLayoutBuilder *builder, uint32_t binding,
    VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stageFlags);

// simple if want VK_SHADER_STAGE_ALL
void descriptorLayoutBuilder_add(struct DescriptorLayoutBuilder* builder, uint32_t binding, VkDescriptorType type);

void descriptorLayoutBuilder_clear(struct DescriptorLayoutBuilder *builder);

VkDescriptorSetLayout descriptorLayoutBuilder_build(struct DescriptorLayoutBuilder* builder, VkDevice device, VkShaderStageFlags defaultStages,
    void* pNext, VkDescriptorSetLayoutCreateFlags flags);

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

typedef struct PoolSizeRatio
{
    VkDescriptorType type;
    float ratio;
} PoolSizeRatio;

typedef struct DescriptorAllocatorGrowable
{
    struct PoolSizeRatio ratios[DESCRIPTOR_ALLOCATOR_MAX_RATIOS];
    uint32_t ratioCount;

    VkDescriptorPool fullPools[DESCRIPTOR_ALLOCATOR_MAX_POOLS];
    uint32_t fullPoolCount;

    VkDescriptorPool readyPools[DESCRIPTOR_ALLOCATOR_MAX_POOLS];
    uint32_t readyPoolCount;

    uint32_t setsPerPool;
} DescriptorAllocatorGrowable;

void descriptorAllocatorGrowable_init_pool(struct DescriptorAllocatorGrowable *alloc, VkDevice device, uint32_t initialSets, PoolSizeRatio *poolRatios, uint32_t ratioCount);
void descriptorAllocatorGrowable_clear_pools(struct DescriptorAllocatorGrowable *alloc, VkDevice device);
void descriptorAllocatorGrowable_destroy_pools(struct DescriptorAllocatorGrowable *alloc, VkDevice device);

VkDescriptorSet descriptorAllocatorGrowable_allocate(struct DescriptorAllocatorGrowable *alloc, VkDevice device, VkDescriptorSetLayout layout, void *pNext);
VkDescriptorPool descriptorAllocatorGrowable_get_pool(struct DescriptorAllocatorGrowable *alloc, VkDevice device);
VkDescriptorPool descriptorAllocatorGrowable_create_pool(VkDevice device, uint32_t setCount, PoolSizeRatio *poolRatios, uint32_t ratioCount);

typedef struct DescriptorWriter {
    VkDescriptorImageInfo  imageInfos[DESCRIPTOR_WRITER_MAX_INFOS];
    uint32_t imageInfoCount;

    VkDescriptorBufferInfo bufferInfos[DESCRIPTOR_WRITER_MAX_INFOS];
    uint32_t bufferInfoCount;

    VkWriteDescriptorSet writes[DESCRIPTOR_WRITER_MAX_INFOS * 2];  // usually 1-2 writes per binding
    uint32_t writeCount;

} DescriptorWriter;

void descriptorWriter_write_image(struct DescriptorWriter *writer, uint32_t binding, VkImageView imageView, VkSampler sampler,
    VkImageLayout layout, VkDescriptorType type);

void descriptorWriter_write_buffer(struct DescriptorWriter *writer, uint32_t binding, VkBuffer buffer, VkDeviceSize size,
    VkDeviceSize offset, VkDescriptorType type); 

void descriptorWriter_clear(struct DescriptorWriter *writer);

void descriptorWriter_update_set(struct DescriptorWriter *writer, VkDevice device, VkDescriptorSet set);
