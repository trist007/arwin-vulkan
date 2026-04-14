#pragma once

#include "vk_types.h"
#include "vk_engine.h"
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <vector>
#include <memory>    // for std::shared_ptr
#include <string>

struct GLTFMaterial
{
    MaterialInstance data;
};

struct GeoSurface
{
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

struct VulkanEngine;
std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine *engine, std::filesystem::path filePath);