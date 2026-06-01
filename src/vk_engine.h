#pragma once

#include "vk_types.h"
#include "vk_images.h"
#include "vk_buffers.h"
#include "vk_initializers.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_loader.h"
#include "vk_swapchain.h"

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

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	Context _context;

	VulkanSwapchain _vk_swapchain;

	FrameData _frames[FRAME_OVERLAP];

	FrameData& get_current_frame() { return _frames[_frame_number % FRAME_OVERLAP]; };

	VkQueue _graphics_queue;
	uint32_t _graphics_queue_family;

	DeletionQueue _main_deletion_queue;

	AllocatedImage _draw_image;
	AllocatedImage _depth_image;
	VkExtent2D _draw_extent;

	float render_scale = 1.f;
	float cam_move_test = 4.0f;
	float rotation_angle = 0.0f;

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

	std::vector<Material> _materials;

	VkSampler _default_sampler_linear;
	VkSampler _defaultSamplerNearest;

	VkDescriptorSetLayout _single_image_descriptor_layout;

	VkClearColorValue clear_color;

private:

	void init_vulkan();
	void init_images();
	void init_commands();
	void init_sync_structures();

	void draw_background(VkCommandBuffer cmd);
	void init_descriptors();

	void init_pipelines();

	void init_imgui();

	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	void draw_geometry(VkCommandBuffer cmd);

	void init_mesh_pipeline();

	void init_default_data();

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
};
