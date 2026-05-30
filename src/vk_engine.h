#pragma once

#include "vk_types.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_loader.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

constexpr unsigned int FRAME_OVERLAP = 2;

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

struct FrameData {
	VkSemaphore _swapchain_semaphore, _render_semaphore;
	VkFence _render_fence;

	VkCommandPool _command_pool;
	VkCommandBuffer _main_command_buffer;

	DeletionQueue _deletion_queue;
	DescriptorAllocatorGrowable _frame_descriptors;
};


class VulkanEngine {
public:

	bool _is_initialized{ false };
	int _frame_number{ 0 };

	VkExtent2D _window_extent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosen_GPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;

	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	VkExtent2D _swapchain_extent;

	FrameData _frames[FRAME_OVERLAP];

	FrameData& get_current_frame() { return _frames[_frame_number % FRAME_OVERLAP]; };

	VkQueue _graphics_queue;
	uint32_t _graphics_queue_family;

	DeletionQueue _main_deletion_queue;

	VmaAllocator _allocator;

	AllocatedImage _draw_image;
	AllocatedImage _depth_image;
	VkExtent2D _draw_extent;
	float render_scale = 1.f;
	float cam_move_test = 4.0f;
	float orbit_angle = 0.0f;

	std::chrono::steady_clock::time_point prev_time = std::chrono::steady_clock::now();;
	std::chrono::steady_clock::time_point curr_time;
	int frame_time = 0;

	bool stop_rendering{ false };

	DescriptorAllocator global_descriptor_allocator;

	VkDescriptorSet _draw_image_descriptors_allocator;
	VkDescriptorSetLayout _draw_image_descriptor_layout;

	VkPipeline _compute_pipeline;
	VkPipelineLayout _compute_pipeline_layout;

	VkFence _imm_fence;
	VkCommandBuffer _imm_command_buffer;
	VkCommandPool _imm_command_pool;

	VkPipelineLayout _mesh_pipeline_layout;
	VkPipeline _mesh_pipeline;

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	AllocatedImage uploadTexture(std::filesystem::path filename);

	std::vector<std::shared_ptr<MeshAsset>> test_meshes;

	std::vector<std::shared_ptr<MeshAsset>> sphere_mesh;

	bool resize_requested;

	GPUSceneData scene_data;

	VkDescriptorSetLayout _gpu_scene_data_descriptor_layout;

	AllocatedImage _error_checkerboard_image;
	AllocatedImage _init_texture;

	VkSampler _default_sampler_linear;
	VkSampler _defaultSamplerNearest;

	VkDescriptorSetLayout _single_image_descriptor_layout;

	VkClearColorValue clear_color;

private:

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();

	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	void draw_background(VkCommandBuffer cmd);
	void init_descriptors();

	void init_pipelines();

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	void init_imgui();

	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	void draw_geometry(VkCommandBuffer cmd);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	void destroy_buffer(const AllocatedBuffer& buffer);

	void init_mesh_pipeline();

	void init_default_data();

	void resize_swapchain();

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

	void destroy_image(const AllocatedImage& img);
};
