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

AllocatedImage load_image_from_gltf(const Context& context, fastgltf::Asset& asset, fastgltf::Image& image);

std::optional<std::vector<Material>> loadGltfTextures(const Context& context, std::filesystem::path filePath);

GPUMeshBuffers upload_mesh(const Context& context, std::span<uint32_t> indices, std::span<Vertex> vertices);

AllocatedImage uploadTexture(const Context& context, std::filesystem::path filename);

void calculate_tangents(std::vector<uint32_t>& indices, std::vector<Vertex>& vertices);