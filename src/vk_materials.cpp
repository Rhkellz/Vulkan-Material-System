#include "vk_materials.h"

void VulkanMaterial::init_materials(const Context& context) {
	_context = context;
}

void VulkanMaterial::upload_material(std::filesystem::path filePath) {
    Material new_mat;
    new_mat.name = filePath.filename().string();

    if (std::filesystem::is_directory(filePath)) {
        for (const auto& entry : std::filesystem::directory_iterator(filePath)) {
            std::string curr_path_string = entry.path().filename().string();
            if (curr_path_string == "albedo.png") {
                new_mat.albedo = uploadTexture(_context, entry.path());
            } else if (curr_path_string == "normal_map.png") {
                new_mat.normal_map = uploadTexture(_context, entry.path());
            }
        }
    }
    else {
        fmt::println("invalid material directory");
        return;
    }

    _materials.push_back(new_mat);
}

void VulkanMaterial::destroy_materials() {
    AllocatedImage empty{};
    for (auto& mat : _materials) {
        if (memcmp(&mat.albedo, &empty, sizeof(AllocatedImage)) != 0) {
            vkutil::destroy_image(_context, mat.albedo);
        }

        if (memcmp(&mat.normal_map, &empty, sizeof(AllocatedImage)) != 0) {
            vkutil::destroy_image(_context, mat.normal_map);
        }
    }
}

bool VulkanMaterial::materials_empty() {
    return _materials.empty();
}