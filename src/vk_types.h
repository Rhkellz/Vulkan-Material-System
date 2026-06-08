#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <thread>
#include <chrono>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>

#include "vk_mem_alloc.h"


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
             fmt::print("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)


struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct Context {
	VkInstance instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT debug_messenger; // Vulkan debug output handle
	VkPhysicalDevice chosen_GPU; // GPU chosen as the default device
	VkDevice device; // Vulkan device for commands
	VkSurfaceKHR surface; // Vulkan window surface
	VmaAllocator allocator; // allocator
	std::function<void(const std::function<void(VkCommandBuffer)>&)> immediate_submit;

	VkExtent2D window_extent;
	struct SDL_Window* window;
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
	VkExtent3D imageExtent = {};
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
};

struct AllocatedBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
};

struct Vertex {
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
	glm::vec4 tangent;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
	glm::mat4 worldMatrix;
	glm::mat4 model;
	uint32_t flags;
	VkDeviceAddress vertexBuffer;
	float displacement_amount;
};

struct GPUSkyboxPushConstants {
	glm::mat4 worldMatrix;
	glm::mat4 model;
};

struct GPUSceneData {
	glm::vec4 light_dir; // w for sun power
	glm::vec4 light_col;
	glm::vec4 camera_pos;
};

struct Material {
	AllocatedImage albedo = {};
	AllocatedImage normal_map = {};
	AllocatedImage roughness = {};
	AllocatedImage metalness = {};
	AllocatedImage displacement = {};
	AllocatedImage AO = {};
	std::string name;
	//VkDescriptorSet material_descriptor;
};