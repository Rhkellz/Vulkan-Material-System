#include "vk_materials.h"

void VulkanMaterial::init_materials(const Context& context, const AllocatedImage& black_image) {
	_context = context;
    _black_image = black_image;
}

void VulkanMaterial::upload_material(std::filesystem::path filePath) {
    bool has_metal = false;
    Material new_mat;
    new_mat.name = filePath.filename().string();

    if (std::filesystem::is_directory(filePath)) {
        for (const auto& entry : std::filesystem::directory_iterator(filePath)) {
            std::string curr_path_string = entry.path().filename().string();
            if (curr_path_string == "albedo.png") {
                new_mat.albedo = uploadTexture(_context, entry.path());
            } else if (curr_path_string == "normal_map.png") {
                new_mat.normal_map = uploadTexture(_context, entry.path());
            } else if (curr_path_string == "roughness.png") {
                new_mat.roughness = uploadTexture(_context, entry.path());
            } else if (curr_path_string == "metalness.png") {
                has_metal = true;
                new_mat.metalness = uploadTexture(_context, entry.path());
            } else if (curr_path_string == "displacement.png") {
                new_mat.displacement = uploadTexture(_context, entry.path());
            } else if (curr_path_string == "AO.png") {
                new_mat.AO = uploadTexture(_context, entry.path());
            }
        }

        if (!has_metal) {
            new_mat.metalness = _black_image;
        }
    }
    else {
        fmt::println("invalid material directory");
        return;
    }

    _materials.push_back(new_mat);
}

void VulkanMaterial::destroy_materials() {
    for (auto& mat : _materials) {
        if (mat.albedo.allocation != VK_NULL_HANDLE) {
            vkutil::destroy_image(_context, mat.albedo);
            mat.albedo.allocation = VK_NULL_HANDLE;
        }

        if (mat.normal_map.allocation != VK_NULL_HANDLE) {
            vkutil::destroy_image(_context, mat.normal_map);
            mat.normal_map.allocation = VK_NULL_HANDLE;
        }

        if (mat.roughness.allocation != VK_NULL_HANDLE) {
            vkutil::destroy_image(_context, mat.roughness);
            mat.roughness.allocation = VK_NULL_HANDLE;
        }

        if (mat.metalness.image != VK_NULL_HANDLE && mat.metalness.image != _black_image.image) {
            vkutil::destroy_image(_context, mat.metalness);
            mat.metalness.allocation = VK_NULL_HANDLE;
        }

        if (mat.displacement.allocation != VK_NULL_HANDLE) {
            vkutil::destroy_image(_context, mat.displacement);
            mat.displacement.allocation = VK_NULL_HANDLE;
        }

        if (mat.AO.allocation != VK_NULL_HANDLE) {
            vkutil::destroy_image(_context, mat.AO);
            mat.AO.allocation = VK_NULL_HANDLE;
        }

    }

    if (_black_image.image != VK_NULL_HANDLE) {
        vkutil::destroy_image(_context, _black_image);
        _black_image.image = VK_NULL_HANDLE;
        _black_image.allocation = nullptr;
    }

}

bool VulkanMaterial::materials_empty() {
    return _materials.empty();
}