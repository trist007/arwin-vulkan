

#include "stb_image.h"
#include <iostream>
#include "vk_loader.h"

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"

#include "HandmadeMath.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"


std::vector<std::shared_ptr<MeshAsset>> loadGltfMeshes(VulkanEngine* engine, const char* path)
{
    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // Read the entire file into memory
    FILE* file = fopen(path, "rb");
    if (!file) {
        SDL_Log("Failed to open glb file: %s", path);
        return meshes;
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* fileData = malloc(fileSize);
    fread(fileData, 1, fileSize, file);
    fclose(file);

    // Parse with cgltf
    cgltf_options options = {};
    cgltf_data* data = NULL;

    cgltf_result result = cgltf_parse(&options, fileData, fileSize, &data);
    if (result != cgltf_result_success) {
        SDL_Log("cgltf_parse failed for %s", path);
        free(fileData);
        return meshes;
    }

    // Load buffers (important for .glb files)
    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        SDL_Log("cgltf_load_buffers failed");
        cgltf_free(data);
        free(fileData);
        return meshes;
    }

    // Process each mesh
    for (cgltf_size i = 0; i < data->meshes_count; ++i)
    {
        cgltf_mesh* gltfMesh = &data->meshes[i];

        auto meshAsset = std::make_shared<MeshAsset>();
        meshAsset->name = gltfMesh->name ? gltfMesh->name : "unnamed_mesh";

        for (cgltf_size p = 0; p < gltfMesh->primitives_count; ++p)
        {
            cgltf_primitive* prim = &gltfMesh->primitives[p];

            // Only support triangles for now
            if (prim->type != cgltf_primitive_type_triangles)
                continue;

            GeoSurface surface = {};

            // --- Indices ---
            if (prim->indices) {
                cgltf_accessor* accessor = prim->indices;
                surface.count = (uint32_t)accessor->count;
                surface.startIndex = 0; // we'll handle offset when uploading

                // For simplicity, assume uint32 indices
                std::vector<uint32_t> indices(accessor->count);
                cgltf_accessor_unpack_indices(accessor, indices.data(), sizeof(uint32_t), accessor->count);

                // You'll need to upload indices to GPU here (or collect them)
            }

            // --- Vertices (positions, normals, uv, etc.) ---
            // This is the more complex part — you need to extract attributes

            for (cgltf_size a = 0; a < prim->attributes_count; ++a)
            {
                cgltf_attribute* attr = &prim->attributes[a];

                if (attr->type == cgltf_attribute_type_position) {
                    // Extract positions
                } else if (attr->type == cgltf_attribute_type_normal) {
                    // Extract normals
                } else if (attr->type == cgltf_attribute_type_texcoord) {
                    // Extract UVs
                }
            }

            // TODO: Upload vertices + indices using your `uploadMesh` function
            // For now, just push a placeholder
            meshAsset->surfaces.push_back(surface);
        }

        meshes.push_back(meshAsset);
    }

    cgltf_free(data);
    free(fileData);

    return meshes;
}