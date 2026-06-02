#pragma once

#include "vk_types.h"
#include "vk_loader.h"
#include <filesystem>
#include "vk_images.h"

class VulkanMaterial {
public:
	void init_materials(const Context& context);

	void upload_material(std::filesystem::path filePath);

	void destroy_materials();
	std::vector<Material> _materials;

	bool materials_empty();
private:
	Context _context;
};
