#pragma once

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

//forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);

AllocatedImage load_image_from_gltf(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);

std::optional<std::vector<AllocatedImage>> loadGltfTextures(VulkanEngine* engine, std::filesystem::path filePath);