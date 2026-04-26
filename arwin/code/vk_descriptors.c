#include "vk_descriptors.h"

/* DescriptorLayoutBuilder usage
    DescriptorLayoutBuilder builder = {0};   // important: zero initialize

    descriptor_layout_builder_add(&builder, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptor_layout_builder_add(&builder, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptor_layout_builder_add_binding(&builder, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);

    VkDescriptorSetLayout layout = descriptor_layout_builder_build(&builder, device, 0, NULL, 0);

    descriptor_layout_builder_clear(&builder);  // ready for next layout
*/

void descriptorLayoutBuilder_add_binding(struct DescriptorLayoutBuilder *builder,
    uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkShaderStageFlags stageFlags)
{
    if(builder->bindingCount >= DESCRIPTOR_LAYOUT_MAX_BINDINGS)
    {
        SDL_Log("ERROR: Too many descriptor bindings in DescriptorLayoutBuilder:add_binding");
        return;
    }

    VkDescriptorSetLayoutBinding* b = &builder->bindings[builder->bindingCount++];
    b->binding            = binding;
    b->descriptorType     = type;
    b->descriptorCount    = descriptorCount;
    b->stageFlags         = stageFlags;
    b->pImmutableSamplers = nullptr;
}

void descriptorLayoutBuilder_add(struct DescriptorLayoutBuilder* builder, uint32_t binding, VkDescriptorType type)
{
    descriptorLayoutBuilder_add_binding(builder, binding, type, 1, VK_SHADER_STAGE_ALL);
}

void descriptorLayoutBuilder_clear(struct DescriptorLayoutBuilder *builder)
{
    builder->bindingCount = 0;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(struct DescriptorLayoutBuilder* builder, VkDevice device, VkShaderStageFlags defaultStages,
        void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    // Apply default stage flags if none were set
    if (defaultStages != 0)
    {
        for (uint32_t i = 0; i < builder->bindingCount; ++i)
        {
            if (builder->bindings[i].stageFlags == 0)
                builder->bindings[i].stageFlags = defaultStages;
        }
    }

    VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext        = pNext,
        .flags        = flags,
        .bindingCount = builder->bindingCount,
        .pBindings    = builder->bindings
    };

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    vkCreateDescriptorSetLayout(device, &createInfo, NULL, &layout);

    return layout;
}

/*
void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, PoolSizeRatio *poolRatios, uint32_t ratioCount)
{
    VkDescriptorPoolSize poolSizes[16] = {};

    uint32_t actualCount = 0;

    for(uint32_t i = 0; i < ratioCount && actualCount < 16; ++i)
    {
        poolSizes[actualCount].type            = poolRatios[i].type;
        poolSizes[actualCount].descriptorCount = (uint32_t)(poolRatios[i].ratio * maxSets);
        actualCount++;
    }

	VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = maxSets,
        .poolSizeCount = actualCount,
        .pPoolSizes = poolSizes
    };

	vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &layout,
    };

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
} 
*/

/* DescriptorAllocatorGrowable usage
    DescriptorAllocatorGrowable allocator = {0};

    // Define your pool size ratios (this is the most common setup)
    PoolSizeRatio ratios[3] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         0.5f },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2.0f },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         0.2f }
    };

    descriptor_allocator_growable_init(&allocator, device, 100, ratios, 3);
    // 100 = initial number of descriptor sets per pool


    // ====================== ALLOCATING DESCRIPTOR SETS ======================
    VkDescriptorSetLayout layout = // your layout from DescriptorLayoutBuilder;

    VkDescriptorSet set1 = descriptor_allocator_growable_allocate(&allocator, device, layout, NULL);
    VkDescriptorSet set2 = descriptor_allocator_growable_allocate(&allocator, device, layout, NULL);

    // If you need pNext (e.g. for mutable descriptors or bindless)
    VkDescriptorSet set3 = descriptor_allocator_growable_allocate(&allocator, device, layout, &some_pnext_struct);


    // ====================== CLEANUP ======================
    descriptor_allocator_growable_clear(&allocator, device);        // reuse pools (fast)
    descriptor_allocator_growable_destroy(&allocator, device);      // full cleanup at shutdown
*/

VkDescriptorPool descriptorAllocatorGrowable_get_pool(struct DescriptorAllocatorGrowable *alloc, VkDevice device)
{
    // If we have a ready pool, return it
    if (alloc->readyPoolCount > 0)
    {
        return alloc->readyPools[alloc->readyPoolCount - 1];
    }

    // No ready pools → create a new one
    VkDescriptorPool newPool = descriptorAllocatorGrowable_create_pool(device,
                                                                         alloc->setsPerPool,
                                                                         alloc->ratios,
                                                                         alloc->ratioCount);

    if (newPool == VK_NULL_HANDLE)
    {
        fprintf(stderr, "ERROR: Failed to create new descriptor pool!\n");
        return VK_NULL_HANDLE;
    }

    // Add the new pool to ready pools
    if (alloc->readyPoolCount >= DESCRIPTOR_ALLOCATOR_MAX_POOLS)
    {
        fprintf(stderr, "ERROR: Too many descriptor pools! (max = %d)\n", 
                DESCRIPTOR_ALLOCATOR_MAX_POOLS);
        vkDestroyDescriptorPool(device, newPool, NULL);
        return VK_NULL_HANDLE;
    }

    alloc->readyPools[alloc->readyPoolCount++] = newPool;
    return newPool;
}

VkDescriptorPool descriptorAllocatorGrowable_create_pool(VkDevice device, uint32_t setCount, PoolSizeRatio *poolRatios, uint32_t ratioCount)
{
    VkDescriptorPoolSize poolSizes[DESCRIPTOR_ALLOCATOR_MAX_RATIOS];

    uint32_t actualPoolSizeCount = 0;
    for (uint32_t i = 0; i < ratioCount; ++i)
    {
        if (ratios[i].ratio > 0.0f)
        {
            poolSizes[actualPoolSizeCount].type = ratios[i].type;
            poolSizes[actualPoolSizeCount].descriptorCount = (uint32_t)(setCount * ratios[i].ratio);
            actualPoolSizeCount++;
        }
    }

    if (actualPoolSizeCount == 0)
        return VK_NULL_HANDLE;

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,  // important!
        .maxSets       = setCount,
        .poolSizeCount = actualPoolSizeCount,
        .pPoolSizes    = poolSizes
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, NULL, &pool) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }

    return pool;
}

void descriptorAllocatorGrowable_init_pool(struct DescriptorAllocatorGrowable *alloc, VkDevice device,
    uint32_t initialSets, PoolSizeRatio* poolRatios, uint32_t ratioCount)
{
    memset(alloc, 0, sizeof(*alloc));
    alloc->setsPerPool = initialSets;

    uint32_t copyCount = ratioCount < DESCRIPTOR_ALLOCATOR_MAX_RATIOS ? ratioCount : DESCRIPTOR_ALLOCATOR_MAX_RATIOS;
    memcpy(alloc->ratios, poolRatios, copyCount * sizeof(PoolSizeRatio));
    alloc->ratioCount = copyCount;

    // Create first pool
    VkDescriptorPool firstPool = descriptorAllocatorGrowable_create_pool(device, initialSets, poolRatios, ratioCount);
    if (firstPool != VK_NULL_HANDLE)
    {
        alloc->readyPools[0] = firstPool;
        alloc->readyPoolCount = 1;
    }
}

void descriptorAllocatorGrowable_clear_pools(struct DescriptorAllocatorGrowable *alloc, VkDevice device)
{ 
    // reset counters, move all pools back to ready, etc.
    alloc->fullPoolCount = 0;
    // ready pools stay ready
}

void descriptorAllocatorGrowable_destroy_pools(struct DescriptorAllocatorGrowable *alloc, VkDevice device)
{
    if (device == VK_NULL_HANDLE || alloc == NULL)
        return;

    // Destroy all full pools
    for (uint32_t i = 0; i < alloc->fullPoolCount; ++i)
    {
        if (alloc->fullPools[i] != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device, alloc->fullPools[i], NULL);
            alloc->fullPools[i] = VK_NULL_HANDLE;
        }
    }

    // Destroy all ready pools
    for (uint32_t i = 0; i < alloc->readyPoolCount; ++i)
    {
        if (alloc->readyPools[i] != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device, alloc->readyPools[i], NULL);
            alloc->readyPools[i] = VK_NULL_HANDLE;
        }
    }

    // Reset counters
    alloc->fullPoolCount = 0;
    alloc->readyPoolCount = 0;
    alloc->ratioCount = 0;
    alloc->setsPerPool = 0;

    // Optional: zero the whole struct for safety
    // memset(alloc, 0, sizeof(DescriptorAllocatorGrowable));
}

VkDescriptorSet descriptorAllocatorGrowable_allocate(struct DescriptorAllocatorGrowable *alloc, VkDevice device,
VkDescriptorSetLayout layout, void* pNext)
{
    // Try ready pools first
    for (uint32_t i = 0; i < alloc->readyPoolCount; ++i)
    {
        VkDescriptorPool pool = alloc->readyPools[i];
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = pNext,
            .descriptorPool     = pool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &layout
        };

        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(device, &allocInfo, &set) == VK_SUCCESS)
            return set;
    }

    // All current pools are full → create new one
    VkDescriptorPool newPool = descriptorAllocatorGrowable_create_pool(device,
                                                                         alloc->setsPerPool,
                                                                         alloc->ratios,
                                                                         alloc->ratioCount);

    if (newPool == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;

    // Move last ready pool to full and add new one
    if (alloc->readyPoolCount > 0)
    {
        alloc->fullPools[alloc->fullPoolCount++] = alloc->readyPools[--alloc->readyPoolCount];
    }
    alloc->readyPools[alloc->readyPoolCount++] = newPool;

    // Try allocate again from the new pool
    return descriptorAllocatorGrowable_allocate(alloc, device, layout, pNext);
}

// ! NOTE: trist007: VkDescriptorTypes
/*
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
*/

/* DescriptorWriter usage

    DescriptorWriter writer = {0};

    // ====================== WRITING TO A DESCRIPTOR SET ======================
    descriptor_writer_write_buffer(&writer, 0, myUniformBuffer, sizeof(MyUniformData), 0, 
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    descriptor_writer_write_image(&writer, 1, myTextureView, mySampler, 
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    descriptor_writer_write_buffer(&writer, 2, myStorageBuffer, myStorageSize, 0,
                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    // Update the actual descriptor set
    descriptor_writer_update_set(&writer, device, myDescriptorSet);


    // ====================== MULTIPLE UPDATES ======================
    // You can keep writing and calling update_set() multiple times
    descriptor_writer_write_image(&writer, 3, anotherTexture, anotherSampler, 
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    descriptor_writer_update_set(&writer, device, anotherSet);

    // ====================== RESET ======================
    descriptor_writer_clear(&writer);   // ready for next frame / next batch

*/
void descriptorWriter_write_buffer(struct DescriptorWriter* writer,
                                    uint32_t binding,
                                    VkBuffer buffer,
                                    VkDeviceSize size,
                                    VkDeviceSize offset,
                                    VkDescriptorType type)
{
    if (writer->bufferInfoCount >= DESCRIPTOR_WRITER_MAX_INFOS) return;

    VkDescriptorBufferInfo* info = &writer->bufferInfos[writer->bufferInfoCount++];
    info->buffer = buffer;
    info->offset = offset;
    info->range  = size;

    VkWriteDescriptorSet* w = &writer->writes[writer->writeCount++];
    w->sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w->dstSet           = VK_NULL_HANDLE;
    w->dstBinding       = binding;
    w->dstArrayElement  = 0;
    w->descriptorCount  = 1;
    w->descriptorType   = type;
    w->pBufferInfo      = info;
    w->pImageInfo       = NULL;
    w->pTexelBufferView = NULL;
}

void descriptorWriter_write_image(struct DescriptorWriter* writer,
                                   uint32_t binding,
                                   VkImageView imageView,
                                   VkSampler sampler,
                                   VkImageLayout layout,
                                   VkDescriptorType type)
{
    if (writer->imageInfoCount >= DESCRIPTOR_WRITER_MAX_INFOS) return;

    VkDescriptorImageInfo* info = &writer->imageInfos[writer->imageInfoCount++];
    info->imageLayout = layout;
    info->imageView   = imageView;
    info->sampler     = sampler;

    VkWriteDescriptorSet* w = &writer->writes[writer->writeCount++];
    w->sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w->dstSet           = VK_NULL_HANDLE;   // filled later
    w->dstBinding       = binding;
    w->dstArrayElement  = 0;
    w->descriptorCount  = 1;
    w->descriptorType   = type;
    w->pImageInfo       = info;
    w->pBufferInfo      = NULL;
    w->pTexelBufferView = NULL;
}


void descriptorWriter_clear(struct DescriptorWriter* writer)
{
    writer->imageInfoCount = 0;
    writer->bufferInfoCount = 0;
    writer->writeCount = 0;
}

void descriptorWriter_update_set(struct DescriptorWriter* writer,
                                  VkDevice device,
                                  VkDescriptorSet set)
{
    for (uint32_t i = 0; i < writer->writeCount; ++i)
    {
        writer->writes[i].dstSet = set;
    }

    if (writer->writeCount > 0)
        vkUpdateDescriptorSets(device, writer->writeCount, writer->writes, 0, NULL);

    descriptorWriter_clear(writer);
}