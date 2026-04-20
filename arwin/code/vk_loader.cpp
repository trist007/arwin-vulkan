#include "stb_image.h"
#include "vk_loader.h"

#include "HandmadeMath.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

extern Arena gArena;

int find_joint_index(cgltf_data *data, cgltf_node *node)
{
    if(!data->skins) return -1;
    cgltf_skin *skin = &data->skins[0];
    for(int i = 0; i < (int)skin->joints_count; i++)
        if(skin->joints[i] == node) return i;
    return -1;
}

/*
void extract_mesh(cgltf_data *data, Mesh *mesh)
{
    // first pass: count total verts and tris across all primitives
    int totalVerts = 0;
    int totalTris  = 0;
    for(int p = 0; p < (int)data->meshes[0].primitives_count; p++)
    {
        cgltf_primitive *prim = &data->meshes[0].primitives[p];
        totalVerts += (int)prim->attributes[0].data->count;
        totalTris  += (int)prim->indices->count / 3;
    }
    
    mesh->vertCount = totalVerts;
    mesh->triCount  = totalTris;
    mesh->verts = (Vertex*)arenaAlloc(&gArena, (totalVerts * sizeof(Vertex)));
    mesh->tris  = (Tri*)arenaAlloc(&gArena, (totalTris * sizeof(Tri)));
    memset(mesh->verts, 0, totalVerts * sizeof(Vertex));
    
    int vertOffset = 0;
    int triOffset  = 0;
    
    // second pass: extract each primitive
    for(int p = 0; p < (int)data->meshes[0].primitives_count; p++)
    {
        cgltf_primitive *prim = &data->meshes[0].primitives[p];
        
        cgltf_accessor *posAcc    = NULL;
        cgltf_accessor *normAcc   = NULL;
        cgltf_accessor *uvAcc     = NULL;
        cgltf_accessor *jointAcc  = NULL;
        cgltf_accessor *weightAcc = NULL;
        
        for(int i = 0; i < (int)prim->attributes_count; i++)
        {
            cgltf_attribute *attr = &prim->attributes[i];
            switch(attr->type)
            {
                case cgltf_attribute_type_position: posAcc    = attr->data; break;
                case cgltf_attribute_type_normal:   normAcc   = attr->data; break;
                case cgltf_attribute_type_texcoord: uvAcc     = attr->data; break;
                case cgltf_attribute_type_joints:   jointAcc  = attr->data; break;
                case cgltf_attribute_type_weights:  weightAcc = attr->data; break;
                default: break;
            }
        }
        
        int primVerts = (int)posAcc->count;
        
        for(int i = 0; i < primVerts; i++)
        {
            Vertex *v = &mesh->verts[vertOffset + i];
            if(posAcc)    read_float_n(posAcc,    i, &v->position.X,      3);
            if(normAcc)   read_float_n(normAcc,   i, &v->normal.X,   3);
            if(uvAcc)     read_float_n(uvAcc,     i, &v->texcoord.U,       2);
            if(weightAcc) read_float_n(weightAcc, i, &v->weights[0], 4);
            if(jointAcc)
            {
                unsigned int j[4] = {};
                cgltf_accessor_read_uint(jointAcc, i, j, 4);
                v->joints[0]=j[0]; v->joints[1]=j[1];
                v->joints[2]=j[2]; v->joints[3]=j[3];
            }
        }
        
        int primTris = (int)prim->indices->count / 3;
        for(int i = 0; i < primTris; i++)
        {
            unsigned int a, b, c;
            read_uint(prim->indices, i*3+0, &a);
            read_uint(prim->indices, i*3+1, &b);
            read_uint(prim->indices, i*3+2, &c);
            // offset indices by vertOffset so they point to the right verts
            mesh->tris[triOffset + i] = {{(int)a + vertOffset,
                    (int)b + vertOffset,
                    (int)c + vertOffset}};
        }
        unsigned int color = 0xffffffff;
        if(prim->material)
        {
            cgltf_pbr_metallic_roughness *pbr = &prim->material->pbr_metallic_roughness;
            int r = (int)(pbr->base_color_factor[0] * 255);
            int g = (int)(pbr->base_color_factor[1] * 255);
            int b = (int)(pbr->base_color_factor[2] * 255);
            color = (b << 16) | (g << 8) | r;
        }
        
        SDL_Log("primitive %d: r=%d g=%d b=%d", p,
                (color >> 16) & 0xff,
                (color >>  8) & 0xff,
                (color >>  0) & 0xff);
        
        mesh->primitives[p] = { vertOffset, triOffset, primTris, color };
        mesh->primitiveCount++;
        
        // Update 
        vertOffset += primVerts;
        triOffset  += primTris;
    }
}
*/

void extract_skeleton(cgltf_data *data, Skeleton *skel)
{
    if(!data->skins_count) return;
    cgltf_skin *skin = &data->skins[0];
    
    skel->jointCount = (int)skin->joints_count;
    skel->joints = (Joint*)arenaAlloc(&gArena, (skel->jointCount * sizeof(Joint)));
    
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
            j->defaultTranslation = {node->translation[0], node->translation[1], node->translation[2]};
        }
        else
        {
            j->defaultTranslation = {0,0,0};
        }
        
        if(node->has_rotation)
        {
            j->defaultRotation = {node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]};
        }
        else
        {
            j->defaultRotation = {0,0,0,1};
        }
        
        if(node->has_scale)
        {
            j->defaultScale = {node->scale[0], node->scale[1], node->scale[2]};
        }
        else
        {
            j->defaultScale = {1,1,1};
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

void extract_materials(cgltf_data* data, Mesh* mesh)
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

void extract_animations(cgltf_data *data, Model *m)
{
    m->animCount  = (int)data->animations_count;
    m->animations = (Animation*)arenaAlloc(&gArena, (m->animCount * sizeof(Animation)));
    
    for(int a = 0; a < m->animCount; a++)
    {
        cgltf_animation *src  = &data->animations[a];
        Animation       *anim = &m->animations[a];
        
        strncpy(anim->name, src->name ? src->name : "unnamed", 63);
        anim->channelCount = (int)src->channels_count;
        anim->channels     = (AnimChannel*)arenaAlloc(&gArena, (anim->channelCount * sizeof(AnimChannel)));
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
            ch->keyframes    = (Keyframe*)arenaAlloc(&gArena, (count * sizeof(Keyframe)));
            
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

Model load_gltf_model(Arena* arena, const char* path)
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
                model.mesh.primitives[model.mesh.primitiveCount++] = {
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
    extract_materials(data, &model.mesh);

    // Extract skeleton & animations
    extract_skeleton(data, &model.skeleton);
    extract_animations(data, &model);

    cgltf_free(data);

    SDL_Log("Successfully loaded glTF model: %d verts, %d tris, %d primitives, %d joints, %d animations",
            model.mesh.vertCount, model.mesh.triCount,
            model.mesh.primitiveCount, model.skeleton.jointCount, model.animCount);

    return model;
}

bool upload_model_to_gpu(VulkanEngine* engine, Model* model)
{
    if (model->mesh.vertCount == 0 || model->mesh.triCount == 0) {
        SDL_Log("No mesh data to upload");
        return false;
    }

    // Calculate sizes
    VkDeviceSize vertexBufferSize = model->mesh.vertCount * sizeof(Vertex);
    VkDeviceSize indexBufferSize  = model->mesh.triCount * 3 * sizeof(uint32_t);  // using uint32 for safety

    // Create combined buffer (vertex + index) - matching your current style
    VkDeviceSize totalSize = vertexBufferSize + indexBufferSize;

    VkBufferCreateInfo bufferCI = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size  = totalSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | 
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT | 
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };

    VmaAllocationCreateInfo allocCI = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    VmaAllocationInfo allocInfo = {};
    VK_CHECK(vmaCreateBuffer(engine->allocator, &bufferCI, &allocCI,
                             &engine->vBuffer, &engine->vBufferAllocation, &allocInfo));

    // Upload data
    void* mapped = allocInfo.pMappedData;

    // Copy vertices first
    memcpy(mapped, model->mesh.verts, vertexBufferSize);

    // Copy indices right after vertices
    memcpy((char*)mapped + vertexBufferSize, model->mesh.tris, indexBufferSize);

    // Save offsets and counts
    engine->vertexBufferSize = vertexBufferSize;
    engine->indexBufferOffset = vertexBufferSize;
    engine->indexCount = model->mesh.triCount * 3;   // total number of indices

    SDL_Log("Uploaded glTF mesh: %d vertices, %d indices to GPU", 
            model->mesh.vertCount, model->indexCount);

    return true;
}

/*
bool
load_model(Model *m, const char *path)
{
    cgltf_options options = {};
    cgltf_data   *data    = NULL;
    
    if(cgltf_parse_file(&options, path, &data) != cgltf_result_success)
    {
        SDL_Log("cgltf_parse_file failed: %s", path);
        return false;
    }
    if(cgltf_load_buffers(&options, data, path) != cgltf_result_success)
    {
        SDL_Log("cgltf_load_buffers failed");
        cgltf_free(data);
        return false;
    }
    
    extract_mesh(data, &m->mesh);
    extract_skeleton(data, &m->skeleton);
    extract_animations(data, m);
    
    SDL_Log("mesh count: %d", (int)data->meshes_count);
    for(int i = 0; i < (int)data->meshes_count; i++)
        SDL_Log("mesh %d: %d primitives", i, (int)data->meshes[i].primitives_count);
    
    cgltf_free(data);
    return true;
}

bool
load_room(Model *m, const char *path)
{
    cgltf_options options = {};
    cgltf_data   *data    = NULL;
    
    if(cgltf_parse_file(&options, path, &data) != cgltf_result_success)
    {
        SDL_Log("load_room: parse failed: %s", path);
        return false;
    }
    if(cgltf_load_buffers(&options, data, path) != cgltf_result_success)
    {
        SDL_Log("load_room: load buffers failed");
        cgltf_free(data);
        return false;
    }
    
    // count totals across ALL meshes
    int totalVerts = 0;
    int totalTris  = 0;
    for(int mi = 0; mi < (int)data->meshes_count; mi++)
    {
        cgltf_mesh *mesh = &data->meshes[mi];
        for(int p = 0; p < (int)mesh->primitives_count; p++)
        {
            cgltf_primitive *prim = &mesh->primitives[p];
            if(!prim->indices) continue;
            totalVerts += (int)prim->attributes[0].data->count;
            totalTris  += (int)prim->indices->count / 3;
        }
    }
    
    Mesh *out = &m->mesh;
    out->vertCount      = totalVerts;
    out->triCount       = totalTris;
    out->primitiveCount = 0;
    out->verts = (Vertex*)arenaAlloc(&gArena, totalVerts * sizeof(Vertex));
    out->tris  = (Tri*)arenaAlloc(&gArena, totalTris  * sizeof(Tri));
    memset(out->verts, 0, totalVerts * sizeof(Vertex));
    
    int vertOffset = 0;
    int triOffset  = 0;
    
    for(int mi = 0; mi < (int)data->meshes_count; mi++)
    {
        cgltf_mesh *mesh = &data->meshes[mi];
        for(int p = 0; p < (int)mesh->primitives_count; p++)
        {
            cgltf_primitive *prim = &mesh->primitives[p];
            if(!prim->indices) continue;
            
            cgltf_accessor *posAcc  = NULL;
            cgltf_accessor *normAcc = NULL;
            cgltf_accessor *uvAcc   = NULL;
            
            for(int i = 0; i < (int)prim->attributes_count; i++)
            {
                cgltf_attribute *attr = &prim->attributes[i];
                switch(attr->type)
                {
                    case cgltf_attribute_type_position: posAcc  = attr->data; break;
                    case cgltf_attribute_type_normal:   normAcc = attr->data; break;
                    case cgltf_attribute_type_texcoord: uvAcc   = attr->data; break;
                    default: break;
                }
            }
            
            if(!posAcc) continue;
            
            int primVerts = (int)posAcc->count;
            for(int i = 0; i < primVerts; i++)
            {
                Vertex *v = &out->verts[vertOffset + i];
                if(posAcc)  read_float_n(posAcc,  i, &v->position.X,    3);
                if(normAcc) read_float_n(normAcc, i, &v->normal.X, 3);
                if(uvAcc)   read_float_n(uvAcc,   i, &v->texcoord.U,     2);
                // no joints/weights for static room
            }
            
            int primTris = (int)prim->indices->count / 3;
            for(int i = 0; i < primTris; i++)
            {
                unsigned int a, b, c;
                read_uint(prim->indices, i*3+0, &a);
                read_uint(prim->indices, i*3+1, &b);
                read_uint(prim->indices, i*3+2, &c);
                out->tris[triOffset + i] = {{
                        (int)a + vertOffset,
                        (int)b + vertOffset,
                        (int)c + vertOffset
                    }};
            }
            
            unsigned int color = 0xffffffff;
            if(prim->material)
            {
                cgltf_pbr_metallic_roughness *pbr = &prim->material->pbr_metallic_roughness;
                int r = (int)(pbr->base_color_factor[0] * 255);
                int g = (int)(pbr->base_color_factor[1] * 255);
                int b = (int)(pbr->base_color_factor[2] * 255);
                color = (b << 16) | (g << 8) | r;
            }
            
            if(out->primitiveCount < 16)
                out->primitives[out->primitiveCount++] = { vertOffset, triOffset, primTris, color };
            
            vertOffset += primVerts;
            triOffset  += primTris;
        }
    }
    
    SDL_Log("load_room: %d verts, %d tris, %d primitives",
            totalVerts, totalTris, out->primitiveCount);
    //debug_model_bounds(out);
    
    cgltf_free(data);
    return true;
}
*/