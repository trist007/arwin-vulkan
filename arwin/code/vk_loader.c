#include "vk_loader.h"

#include "HandmadeMath.h"
#include <SDL3/SDL_pixels.h>
#include <vulkan/vulkan_core.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Allocate and begin a one-time command buffer
static VkCommandBuffer begin_single_time_commands(struct VulkanEngine* engine)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = engine->commandPool,          // Use your main command pool
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(engine->device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT   // Important for one-time use
    };

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
}

// End, submit, and wait for the one-time command buffer to finish
static void end_single_time_commands(struct VulkanEngine* engine, VkCommandBuffer commandBuffer)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &commandBuffer
    };

    // Submit to graphics queue and wait for it to finish
    VK_CHECK(vkQueueSubmit(engine->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(engine->graphicsQueue));   // Wait for completion (simple but safe for init)

    // Clean up
    vkFreeCommandBuffers(engine->device, engine->commandPool, 1, &commandBuffer);
}

int find_joint_index(cgltf_data *data, cgltf_node *node)
{
    if(!data->skins) return -1;
    cgltf_skin *skin = &data->skins[0];
    for(int i = 0; i < (int)skin->joints_count; i++)
        if(skin->joints[i] == node) return i;
    return -1;
}

void extract_skeleton(Arena *arena, cgltf_data *data, Skeleton *skel)
{
    if(!data->skins_count) return;
    cgltf_skin *skin = &data->skins[0];
    
    skel->jointCount = (int)skin->joints_count;
    skel->joints = (Joint*)arenaAlloc(arena, (skel->jointCount * sizeof(Joint)));
    
    for(int i = 0; i < skel->jointCount; i++)
    {
        Joint *j = &skel->joints[i];
        cgltf_node *node = skin->joints[i];
        
        strncpy(j->name, node->name ? node->name : "unnamed", 63);
        
        // inverse bind matrix
        if(skin->inverse_bind_matrices)
        {
            float temp[16];
            cgltf_accessor_read_float(skin->inverse_bind_matrices, i, temp, 16);
            j->inverseBindMatrix = mat4_from_float16(temp);
        }
        else
        {
            (j->inverseBindMatrix) = HMM_M4D(1.0f);
        }
        
        // store default local transform from node
        if(node->has_translation)
        {
            j->defaultTranslation = (HMM_Vec3){node->translation[0], node->translation[1], node->translation[2]};
        }
        else
        {
            j->defaultTranslation = (HMM_Vec3){0,0,0};
        }
        
        if(node->has_rotation)
        {
            j->defaultRotation = (HMM_Quat){node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]};
        }
        else
        {
            j->defaultRotation = (HMM_Quat){0,0,0,1};
        }
        
        if(node->has_scale)
        {
            j->defaultScale = (HMM_Vec3){node->scale[0], node->scale[1], node->scale[2]};
        }
        else
        {
            j->defaultScale = (HMM_Vec3){1,1,1};
        }
        
        // find parent
        j->parent = -1;
        if(node->parent)
            j->parent = find_joint_index(data, node->parent);
    }
}

int path_to_type(cgltf_animation_path_type path)
{
    switch(path)
    {
        case cgltf_animation_path_type_translation: return 0;
        case cgltf_animation_path_type_rotation:    return 1;
        case cgltf_animation_path_type_scale:       return 2;
        default: return -1;
    }
}

void extract_materials(Arena *arena, cgltf_data* data, Mesh* mesh)
{
    if (data->materials_count == 0)
    {
        mesh->materialCount = 1;
        mesh->materials[0].baseColorFactor = HMM_V4(1,1,1,1);
        return;
    }

    mesh->materialCount = (int)data->materials_count;
    if (mesh->materialCount > 8) mesh->materialCount = 8;

    for (int i = 0; i < mesh->materialCount; ++i)
    {
        cgltf_material* mat = &data->materials[i];

        if (mat->has_pbr_metallic_roughness)
        {
            cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;
            mesh->materials[i].baseColorFactor = HMM_V4(
                pbr->base_color_factor[0],
                pbr->base_color_factor[1],
                pbr->base_color_factor[2],
                pbr->base_color_factor[3]
            );
        }
        else
        {
            mesh->materials[i].baseColorFactor = HMM_V4(1,1,1,1);
        }
    }
}

void extract_animations(Arena *arena, cgltf_data *data, Model *m)
{
    m->animCount  = (int)data->animations_count;
    m->animations = (Animation*)arenaAlloc(arena, (m->animCount * sizeof(Animation)));
    
    for(int a = 0; a < m->animCount; a++)
    {
        cgltf_animation *src  = &data->animations[a];
        Animation       *anim = &m->animations[a];
        
        strncpy(anim->name, src->name ? src->name : "unnamed", 63);
        anim->channelCount = (int)src->channels_count;
        anim->channels     = (AnimChannel*)arenaAlloc(arena, (anim->channelCount * sizeof(AnimChannel)));
        anim->duration     = 0.0f;
        
        for(int c = 0; c < anim->channelCount; c++)
        {
            cgltf_animation_channel *src_ch = &src->channels[c];
            cgltf_animation_sampler *samp   = src_ch->sampler;
            AnimChannel             *ch     = &anim->channels[c];
            
            ch->type       = path_to_type(src_ch->target_path);
            ch->jointIndex = find_joint_index(data, src_ch->target_node);
            
            int count        = (int)samp->input->count;
            ch->keyframeCount = count;
            ch->keyframes    = (Keyframe*)arenaAlloc(arena, (count * sizeof(Keyframe)));
            
            for(int k = 0; k < count; k++)
            {
                cgltf_accessor_read_float(samp->input,  k, &ch->keyframes[k].time,    1);
                cgltf_accessor_read_float(samp->output, k, &ch->keyframes[k].value.X,
                                          ch->type == 1 ? 4 : 3);  // rotation is vec4, others vec3
                
                if(ch->keyframes[k].time > anim->duration)
                    anim->duration = ch->keyframes[k].time;
            }
        }
    }
}

Model
load_gltf_model(Arena *arena, const char* path)
{
    Model model = {0};

    // === SDL3 file loading ===
    size_t fileSize = 0;
    void* fileData = SDL_LoadFile(path, &fileSize);
    if (!fileData) {
        SDL_Log("Failed to load glb/glTF file: %s (%s)", path, SDL_GetError());
        return model;
    }

    void* arenaData = arenaAlloc(arena, fileSize);
    if (!arenaData) {
        SDL_free(fileData);
        SDL_Log("Arena allocation failed for model data");
        return model;
    }
    memcpy(arenaData, fileData, fileSize);
    SDL_free(fileData);

    // === cgltf parsing ===
    cgltf_options options = {};
    cgltf_data* data = NULL;

    if (cgltf_parse(&options, arenaData, fileSize, &data) != cgltf_result_success) {
        SDL_Log("cgltf_parse failed for: %s", path);
        return model;
    }

    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        SDL_Log("cgltf_load_buffers failed for: %s", path);
        cgltf_free(data);
        return model;
    }

    SDL_Log("Loaded glTF: %zu meshes, %zu skins, %zu animations", 
            data->meshes_count, data->skins_count, data->animations_count);

    // === Extract Mesh with per-primitive colors ===
    if (data->meshes_count > 0) {
        cgltf_mesh* gltfMesh = &data->meshes[0];

        int totalVerts = 0;
        int totalTris = 0;

        for (cgltf_size p = 0; p < gltfMesh->primitives_count; ++p) {
            cgltf_primitive* prim = &gltfMesh->primitives[p];
            if (prim->type != cgltf_primitive_type_triangles || !prim->indices) 
                continue;

            totalVerts += (int)prim->attributes[0].data->count;
            totalTris += (int)prim->indices->count / 3;
        }

        model.mesh.vertCount = totalVerts;
        model.mesh.triCount  = totalTris;
        model.mesh.verts = (Vertex*)arenaAlloc(arena, totalVerts * sizeof(Vertex));
        model.mesh.tris  = (Tri*)arenaAlloc(arena, totalTris * sizeof(Tri));

        if (!model.mesh.verts || !model.mesh.tris) {
            cgltf_free(data);
            return model;
        }

        int vertOffset = 0;
        int triOffset  = 0;
        model.mesh.primitiveCount = 0;

        for (cgltf_size p = 0; p < gltfMesh->primitives_count; ++p) {
            cgltf_primitive* prim = &gltfMesh->primitives[p];
            if (prim->type != cgltf_primitive_type_triangles || !prim->indices) 
                continue;

            cgltf_accessor* posAcc = NULL;
            cgltf_accessor* normAcc = NULL;
            cgltf_accessor* uvAcc = NULL;
            cgltf_accessor* jointAcc = NULL;
            cgltf_accessor* weightAcc = NULL;

            for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
                cgltf_attribute* attr = &prim->attributes[a];
                if (attr->type == cgltf_attribute_type_position) posAcc = attr->data;
                else if (attr->type == cgltf_attribute_type_normal)   normAcc = attr->data;
                else if (attr->type == cgltf_attribute_type_texcoord) uvAcc   = attr->data;
                else if (attr->type == cgltf_attribute_type_joints)   jointAcc = attr->data;
                else if (attr->type == cgltf_attribute_type_weights)  weightAcc = attr->data;
            }

            int primVerts = (int)posAcc->count;

            for (int i = 0; i < primVerts; ++i) {
                Vertex* v = &model.mesh.verts[vertOffset + i];

                cgltf_accessor_read_float(posAcc, i, &v->position.X, 3);
                if (normAcc) cgltf_accessor_read_float(normAcc, i, &v->normal.X, 3);
                else v->normal = HMM_V3(0, 1, 0);

                if (uvAcc) {
                    float uv[2] = {0};
                    cgltf_accessor_read_float(uvAcc, i, uv, 2);
                    v->texcoord = HMM_V2(uv[0], 1.0f - uv[1]);
                }

                if (jointAcc) {
                    unsigned int j[4] = {0};
                    cgltf_accessor_read_uint(jointAcc, i, j, 4);
                    v->joints[0] = (uint8_t)j[0];
                    v->joints[1] = (uint8_t)j[1];
                    v->joints[2] = (uint8_t)j[2];
                    v->joints[3] = (uint8_t)j[3];
                }

                if (weightAcc)
                    cgltf_accessor_read_float(weightAcc, i, v->weights, 4);
            }

            // Indices
            int primTris = 0;
            if (prim->indices) {
                primTris = (int)prim->indices->count / 3;
                for (int i = 0; i < primTris; ++i) {
                    unsigned int a, b, c;
                    cgltf_accessor_read_uint(prim->indices, i*3, &a, 1);
                    cgltf_accessor_read_uint(prim->indices, i*3+1, &b, 1);
                    cgltf_accessor_read_uint(prim->indices, i*3+2, &c, 1);

                    model.mesh.tris[triOffset + i] = (Tri){
                        (int)a + vertOffset,
                        (int)b + vertOffset,
                        (int)c + vertOffset
                    };
                }
            }

            // === Material Index + Color ===
            int materialIndex = 0;
            if (prim->material) {
                materialIndex = (int)(prim->material - data->materials);  // calculate index
            }

            // Material Color - packed like your working renderer
            unsigned int color = 0xFFFFFFFF;
            if (prim->material && prim->material->has_pbr_metallic_roughness)
            {
                cgltf_pbr_metallic_roughness* pbr = &prim->material->pbr_metallic_roughness;
                int r = (int)(pbr->base_color_factor[0] * 255.0f);
                int g = (int)(pbr->base_color_factor[1] * 255.0f);
                int b = (int)(pbr->base_color_factor[2] * 255.0f);
                color = (b << 16) | (g << 8) | r;
            }

            if (model.mesh.primitiveCount < 16) {
                model.mesh.primitives[model.mesh.primitiveCount++] = (Primitive){
                    vertOffset, 
                    triOffset, 
                    primTris, 
                    materialIndex,
                    color
                };
            }

            vertOffset += primVerts;
            triOffset  += primTris;
        }
    }

    // Extract materials for shader baseColorFactor
    extract_materials(arena, data, &model.mesh);

    // Extract skeleton & animations
    extract_skeleton(arena, data, &model.skeleton);
    extract_animations(arena, data, &model);

    cgltf_free(data);

    SDL_Log("Successfully loaded glTF model: %d verts, %d tris, %d primitives, %d joints, %d animations",
            model.mesh.vertCount, model.mesh.triCount,
            model.mesh.primitiveCount, model.skeleton.jointCount, model.animCount);

    return model;
}

bool upload_model_to_gpu(struct VulkanEngine* engine, Model* model)
{
    if (model->mesh.vertCount == 0 || model->mesh.triCount == 0) {
        SDL_Log("No mesh data to upload");
        return false;
    }

    VkDeviceSize vertexBufferSize = model->mesh.vertCount * sizeof(Vertex);
    VkDeviceSize indexBufferSize  = model->mesh.triCount * 3 * sizeof(uint32_t);
    VkDeviceSize totalSize        = vertexBufferSize + indexBufferSize;

    // 1. Create the GPU buffer (in device local memory)
    VkBufferCreateInfo bufferCI = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = totalSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    VK_CHECK(vkCreateBuffer(engine->device, &bufferCI, NULL, &model->vertexBuffer));

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(engine->device, model->vertexBuffer, &reqs);

    Allocation gpuAlloc = arena_alloc(engine->deviceLocalArena, reqs.size, reqs.alignment);
    if (!allocation_valid(gpuAlloc)) {
        SDL_Log("ERROR: Out of device local memory for model!");
        vkDestroyBuffer(engine->device, model->vertexBuffer, NULL);
        return false;
    }

    VK_CHECK(vkBindBufferMemory(engine->device, model->vertexBuffer, 
                                gpuAlloc.memory, gpuAlloc.offset));

    model->vertexBufferOffset = gpuAlloc.offset;   // save for later use if needed

    // 2. Create staging buffer and upload data
    VkBuffer stagingBuffer;

    VkBufferCreateInfo stagingCI = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = totalSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    VK_CHECK(vkCreateBuffer(engine->device, &stagingCI, NULL, &stagingBuffer));

    Allocation stagingAlloc = arena_alloc(engine->stagingArena, totalSize, 16);
    if (!allocation_valid(stagingAlloc)) {
        SDL_Log("ERROR: Out of staging memory!");
        vkDestroyBuffer(engine->device, stagingBuffer, NULL);
        vkDestroyBuffer(engine->device, model->vertexBuffer, NULL);
        return false;
    }

    VK_CHECK(vkBindBufferMemory(engine->device, stagingBuffer, 
                                stagingAlloc.memory, stagingAlloc.offset));

    // Copy data to staging
    void* mapped = (char*)engine->stagingArena->mapped + stagingAlloc.offset;
    memcpy(mapped, model->mesh.verts, vertexBufferSize);
    memcpy((char*)mapped + vertexBufferSize, model->mesh.tris, indexBufferSize);

    // 3. Copy from staging → GPU buffer
    VkCommandBuffer cmd = begin_single_time_commands(engine);

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = totalSize
    };
    vkCmdCopyBuffer(cmd, stagingBuffer, model->vertexBuffer, 1, &copyRegion);

    end_single_time_commands(engine, cmd);

    // Cleanup staging buffer
    vkDestroyBuffer(engine->device, stagingBuffer, NULL);

    // Save info in model
    model->vertexBufferSize = vertexBufferSize;
    model->indexBufferOffset = vertexBufferSize;
    model->indexCount = model->mesh.triCount * 3;

    SDL_Log("Uploaded model: %d verts, %d indices", 
            model->mesh.vertCount, model->indexCount);

    return true;
}

bool LoadFontAtlas(struct VulkanEngine* engine, FontAtlas* atlas)
{
    if (!atlas) return false;

    // Load font
    size_t fileSize = 0;
    unsigned char* fontData = (unsigned char*)SDL_LoadFile("../fonts/liberation-mono.ttf", &fileSize);
    if (!fontData) {
        SDL_Log("Failed to load font file");
        return false;
    }

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fontData, 0)) {
        SDL_Log("stbtt_InitFont failed");
        SDL_free(fontData);
        return false;
    }

    const int atlasSize = 2048;
    const int padding = 12;
    const float fontPixelHeight = 48.0f;

    stbtt_pack_context packContext;
    stbtt_packedchar packedChars[96];

    unsigned char* atlasBitmap = (unsigned char*)malloc(atlasSize * atlasSize);
    if (!atlasBitmap) {
        SDL_free(fontData);
        return false;
    }

    stbtt_PackBegin(&packContext, atlasBitmap, atlasSize, atlasSize, 0, padding, NULL);
    stbtt_PackSetOversampling(&packContext, 2, 2);
    stbtt_PackFontRange(&packContext, fontData, 0, fontPixelHeight, 32, 96, packedChars);
    stbtt_PackEnd(&packContext);

    // Get font metrics
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    float scale = stbtt_ScaleForPixelHeight(&font, fontPixelHeight);

    atlas->baseline = (float)ascent * scale;

    SDL_Log("Font metrics - Ascent: %d, Descent: %d, Baseline: %.2f", ascent, descent, atlas->baseline);

    // Debug PNG
    stbi_write_png("font_atlas_debug.png", atlasSize, atlasSize, 1, atlasBitmap, atlasSize);

    // ====================== CREATE FONT ATLAS IMAGE ======================
    VkImageCreateInfo imageCI = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .extent = { (uint32_t)atlasSize, (uint32_t)atlasSize, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VK_CHECK(vkCreateImage(engine->device, &imageCI, NULL, &atlas->image));

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(engine->device, atlas->image, &reqs);

    Allocation alloc = arena_alloc(engine->deviceLocalArena, reqs.size, reqs.alignment);
    if (!allocation_valid(alloc)) {
        SDL_Log("ERROR: Out of memory for font atlas");
        free(atlasBitmap);
        SDL_free(fontData);
        vkDestroyImage(engine->device, atlas->image, NULL);
        return false;
    }

    VK_CHECK(vkBindImageMemory(engine->device, atlas->image, alloc.memory, alloc.offset));

    // ====================== UPLOAD DATA (Staging) ======================
    VkBuffer stagingBuffer;
    VkDeviceSize bufferSize = (VkDeviceSize)atlasSize * atlasSize;

    VkBufferCreateInfo stagingCI = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    VK_CHECK(vkCreateBuffer(engine->device, &stagingCI, NULL, &stagingBuffer));

    Allocation stagingAlloc = arena_alloc(engine->stagingArena, bufferSize, 16); // 16-byte alignment is safe

    if (!allocation_valid(stagingAlloc)) {
        SDL_Log("ERROR: Out of staging memory for font atlas");
        vkDestroyBuffer(engine->device, stagingBuffer, NULL);
        free(atlasBitmap);
        SDL_free(fontData);
        return false;
    }

    VK_CHECK(vkBindBufferMemory(engine->device, stagingBuffer, stagingAlloc.memory, stagingAlloc.offset));

    // Copy data to mapped memory
    void* mapped = (char*)engine->stagingArena->mapped + stagingAlloc.offset;
    memcpy(mapped, atlasBitmap, bufferSize);

    // ====================== COMMAND BUFFER UPLOAD ======================
    VkCommandBuffer cmd = begin_single_time_commands(engine);

    // Barrier: Undefined -> Transfer DST
    VkImageMemoryBarrier2 barrier1 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = atlas->image,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VkDependencyInfo dep1 = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier1 };
    vkCmdPipelineBarrier2(cmd, &dep1);

    // Copy
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .layerCount = 1 },
        .imageExtent = { (uint32_t)atlasSize, (uint32_t)atlasSize, 1 }
    };
    vkCmdCopyBufferToImage(cmd, stagingBuffer, atlas->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Barrier: Transfer DST -> Shader Read Only
    VkImageMemoryBarrier2 barrier2 = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = atlas->image,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VkDependencyInfo dep2 = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier2 };
    vkCmdPipelineBarrier2(cmd, &dep2);

    end_single_time_commands(engine, cmd);

    // Cleanup staging buffer
    vkDestroyBuffer(engine->device, stagingBuffer, NULL);

    // ====================== CREATE IMAGE VIEW & SAMPLER ======================
    VkImageViewCreateInfo viewCI = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = atlas->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };
    VK_CHECK(vkCreateImageView(engine->device, &viewCI, NULL, &atlas->imageView));

    VkSamplerCreateInfo samplerCI = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    };
    VK_CHECK(vkCreateSampler(engine->device, &samplerCI, NULL, &atlas->sampler));

    // Fill glyph data
    const float inset = 2.0f / (float)atlasSize;

    for (int i = 32; i < 128; ++i) {
        stbtt_packedchar* pc = &packedChars[i - 32];
        Glyph* g = &atlas->glyphs[i];

        g->u0 = (pc->x0 + inset) / (float)atlasSize;
        g->v0 = (pc->y0 + inset) / (float)atlasSize;
        g->u1 = (pc->x1 - inset) / (float)atlasSize;
        g->v1 = (pc->y1 - inset) / (float)atlasSize;

        g->width  = pc->x1 - pc->x0;
        g->height = pc->y1 - pc->y0;
        g->xoff   = pc->xoff;
        g->yoff   = pc->yoff;
        g->xoff2  = pc->xoff2;
        g->yoff2  = pc->yoff2;
    }

    atlas->atlasWidth = atlasSize;
    atlas->atlasHeight = atlasSize;

    // Cleanup CPU memory
    SDL_free(fontData);
    free(atlasBitmap);

    SDL_Log("Font atlas loaded successfully (%dx%d)", atlasSize, atlasSize);
    return true;
}