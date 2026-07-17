#define VMA_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <stb_image.h>

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"
#include <glm/gtx/transform.hpp>


constexpr bool bUseValidationLayers = true;


void VulkanEngine::init() {
	init_vulkan();

	_vk_swapchain.init_swapchain(_context);

	init_images();

	init_commands();

	init_sync_structures();

	init_descriptors();

	init_pipelines();

	init_default_data();

	init_imgui();

	//everything went fine
	_is_initialized = true;

}

void VulkanEngine::init_vulkan() {
	Context context;

	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	context.window_extent = { 1700 , 900 };

	context.window = SDL_CreateWindow(
		"Material System",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		context.window_extent.width,
		context.window_extent.height,
		window_flags
	);

	prev_time = std::chrono::steady_clock::now();

	vkb::InstanceBuilder builder;

	//make the vulkan instance, with basic debug feature
	auto inst_ret = builder.set_app_name("vkguide")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	context.instance = vkb_inst.instance;
	context.debug_messenger = vkb_inst.debug_messenger;
	context.immediate_submit = [this](const std::function<void(VkCommandBuffer)>& func) {
		auto mutable_func = func;
		this->immediate_submit(std::move(mutable_func));
		};

	SDL_Vulkan_CreateSurface(context.window, context.instance, &context.surface);

	// now we activate needed features

	//vulkan 1.3 features
	VkPhysicalDeviceVulkan13Features features{};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features.dynamicRendering = true;
	features.synchronization2 = true;

	//vulkan 1.2 features
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	//use vkbootstrap to select a gpu.
	//We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physical_device = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features)
		.set_required_features_12(features12)
		.set_surface(context.surface)
		.select()
		.value();

	//create the final vulkan device
	vkb::DeviceBuilder device_builder{ physical_device };

	vkb::Device vkbDevice = device_builder.build().value();

	// Get the VkDevice handle used in the rest of a vulkan application
	context.device = vkbDevice.device;
	context.chosen_GPU = physical_device.physical_device;

	_context = context;

	_graphics_queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _context.chosen_GPU;
	allocatorInfo.device = _context.device;
	allocatorInfo.instance = _context.instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &_context.allocator);

}

void VulkanEngine::init_images() {
	//hardcoding the draw format to 32 bit float
	VkFormat draw_image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkExtent3D draw_image_extent = { _context.window_extent.width, _context.window_extent.height, 1 };

	VkImageUsageFlags draw_image_flags{}; // revisit
	draw_image_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
	draw_image_flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	_draw_image = vkutil::create_image(_context, draw_image_extent, draw_image_format, draw_image_flags, false, VK_SAMPLE_COUNT_4_BIT);
	
	// this image resolves the MSAA sampling, allowing us to blit the drawn image into the swapchain image
	_resolve_image = vkutil::create_image(_context, draw_image_extent, draw_image_format, draw_image_flags, false, VK_SAMPLE_COUNT_1_BIT);


	VkFormat depth_image_format = VK_FORMAT_D32_SFLOAT;
	VkImageUsageFlags depth_image_flags = {};
	depth_image_flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	_depth_image = vkutil::create_image(_context, draw_image_extent, depth_image_format, depth_image_flags, false, VK_SAMPLE_COUNT_4_BIT);

	// add to deletion queues
	_main_deletion_queue.push_function([=]() {
		vkutil::destroy_image(_context, _draw_image);
		vkutil::destroy_image(_context, _resolve_image);
		vkutil::destroy_image(_context, _depth_image);
		});
}

void VulkanEngine::init_commands() {
	// create a command pool for commands submitted to the graphics queue.
	// we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo command_pool_info = vkinit::command_pool_create_info(_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {// create a command pool / command buffers per frame
		VK_CHECK(vkCreateCommandPool(_context.device, &command_pool_info, nullptr, &_frames[i]._command_pool));

		_main_deletion_queue.push_function([=]() {
			vkDestroyCommandPool(_context.device, _frames[i]._command_pool, nullptr);
			});

		// allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmd_alloc_info = vkinit::command_buffer_allocate_info(_frames[i]._command_pool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_context.device, &cmd_alloc_info, &_frames[i]._main_command_buffer));
	}

	VK_CHECK(vkCreateCommandPool(_context.device, &command_pool_info, nullptr, &_imm_command_pool));

	// allocate the command buffer for immediate submits
	VkCommandBufferAllocateInfo cmd_alloc_info = vkinit::command_buffer_allocate_info(_imm_command_pool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_context.device, &cmd_alloc_info, &_imm_command_buffer));// may not currently need

	_main_deletion_queue.push_function([=]() {
		vkDestroyCommandPool(_context.device, _imm_command_pool, nullptr);
		});
}

void VulkanEngine::init_sync_structures() {
	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	VkFenceCreateInfo fence_create_info = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphore_create_info = vkinit::semaphore_create_info();

	// we seperate the present semaphores to be tied to the amount of swapchain images, as vkQueuePresentKHR cannot signal a semaphore
	// https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
	_vk_swapchain.init_present_semaphores(semaphore_create_info);


	for (int i = 0; i < FRAME_OVERLAP; i++) {
		VK_CHECK(vkCreateFence(_context.device, &fence_create_info, nullptr, &_frames[i]._render_fence));

		VK_CHECK(vkCreateSemaphore(_context.device, &semaphore_create_info, nullptr, &_frames[i]._swapchain_semaphore));

		_main_deletion_queue.push_function([=]() { vkDestroyFence(_context.device, _frames[i]._render_fence, nullptr); });

		_main_deletion_queue.push_function([=]() { vkDestroySemaphore(_context.device, _frames[i]._swapchain_semaphore, nullptr); });
	}

	VK_CHECK(vkCreateFence(_context.device, &fence_create_info, nullptr, &_imm_fence));
	_main_deletion_queue.push_function([=]() { 
		vkDestroyFence(_context.device, _imm_fence, nullptr);
		_vk_swapchain.destroy_present_semaphores(); 
		});
}


void VulkanEngine::cleanup() {
	if (_is_initialized) {

		vkDeviceWaitIdle(_context.device);

		for (int i = 0; i < FRAME_OVERLAP; i++) {
			_frames[i]._deletion_queue.flush();
		}

		_main_deletion_queue.flush();

		_vk_materials.destroy_materials();

		_vk_swapchain.destroy_swapchain();

		vmaDestroyAllocator(_context.allocator);

		vkDestroySurfaceKHR(_context.instance, _context.surface, nullptr);
		vkDestroyDevice(_context.device, nullptr);

		vkb::destroy_debug_utils_messenger(_context.instance, _context.debug_messenger);
		vkDestroyInstance(_context.instance, nullptr);

		SDL_DestroyWindow(_context.window);
	}
}

void VulkanEngine::draw_background(VkCommandBuffer cmd) {

}

void VulkanEngine::draw() {
	// wait for sync
	VK_CHECK(vkWaitForFences(_context.device, 1, &get_current_frame()._render_fence, true, 1000000000));
	VK_CHECK(vkResetFences(_context.device, 1, &get_current_frame()._render_fence));

	get_current_frame()._deletion_queue.flush();
	get_current_frame()._frame_descriptors.clear_pools(_context.device);

	uint32_t swapchain_image_idx;
	// _swapchain_semaphore will be signaled when the image is ready
	VkResult e = vkAcquireNextImageKHR(_context.device, _vk_swapchain._swapchain, 1000000000, get_current_frame()._swapchain_semaphore, nullptr, &swapchain_image_idx);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
		return;
	}

	VkCommandBuffer cmd = get_current_frame()._main_command_buffer;

	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	// begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// start the command buffer recording
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	_draw_extent.height = std::min(_vk_swapchain._swapchain_extent.height, _draw_image.imageExtent.height);
	_draw_extent.width = std::min(_vk_swapchain._swapchain_extent.width, _draw_image.imageExtent.width);

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, _draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _resolve_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::transition_image(cmd, _depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	start_rendering(cmd);
	draw_geometry(cmd);
	draw_skybox(cmd);
	end_rendering(cmd);

	draw_imgui(cmd, _draw_image.imageView);

	// transition the resolve image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, _resolve_image.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _vk_swapchain._swapchain_images[swapchain_image_idx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// also draw imgui now to save a transition

	// execute a copy from the resolve image into the swapchain
	vkutil::copy_image_to_image(cmd, _resolve_image.image, _vk_swapchain._swapchain_images[swapchain_image_idx], _draw_extent, _vk_swapchain._swapchain_extent);

	// switch to present
	vkutil::transition_image(cmd, _vk_swapchain._swapchain_images[swapchain_image_idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	// finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	// prepare the submission to the queue. 
	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	// we will signal the _render_semaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	// gpu will wait when it needs to write colors for swapchain semaphore so we dont write onto an image currently being used
	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchain_semaphore);
	// signal render semaphore when done rendering
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, _vk_swapchain._present_semaphores[swapchain_image_idx]);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _render_fence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphics_queue, 1, &submit, get_current_frame()._render_fence));

	// prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _render_semaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &_vk_swapchain._swapchain;
	presentInfo.swapchainCount = 1;

	// Do not show the image until render semaphore is signaled
	presentInfo.pWaitSemaphores = &_vk_swapchain._present_semaphores[swapchain_image_idx];
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchain_image_idx;

	VkResult presentResult = vkQueuePresentKHR(_graphics_queue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		resize_requested = true;
	}

	//increase the number of frames drawn
	_frame_number++;
	curr_time = std::chrono::steady_clock::now();

	frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - prev_time).count();

	prev_time = curr_time;
}

void VulkanEngine::start_rendering(VkCommandBuffer cmd) {
	VkClearValue clear_value;
	clear_value.color = clear_color;
	VkRenderingAttachmentInfo color_attachment = vkinit::attachment_info(_draw_image.imageView, &clear_value, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
	color_attachment.resolveImageView = _resolve_image.imageView;
	color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkRenderingAttachmentInfo depth_attachment = vkinit::depth_attachment_info(_depth_image.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo render_info = vkinit::rendering_info(_draw_extent, &color_attachment, &depth_attachment);
	vkCmdBeginRendering(cmd, &render_info);

	// set dynamic viewport and scissor
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _draw_extent.width;
	viewport.height = _draw_extent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd) {
	// get the current uniform buffer
	AllocatedBuffer uniform_buffer = get_current_frame()._GPU_scene_data_buffer;

	// write into the buffer
	GPUSceneData* scene_uniform_data = (GPUSceneData*)uniform_buffer.allocation->GetMappedData();
	scene_data.camera_pos = glm::vec4(0, 0, cam_move_test, 0);//TODO: temp
	*scene_uniform_data = scene_data;

	// create a descriptor set that binds that buffer and update it
	VkDescriptorSet global_descriptor = get_current_frame()._frame_descriptors.allocate(_context.device, _gpu_scene_data_descriptor_layout);

	DescriptorWriter writer;
	writer.write_buffer(0, uniform_buffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_context.device, global_descriptor);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _mesh_pipeline);

	VkDescriptorSet image_set = get_current_frame()._frame_descriptors.allocate(_context.device, _material_descriptor_layout);

	// write textures
	DescriptorWriter image_writer;
	if (!_vk_materials.materials_empty()) {
		_vk_materials.write_materials(_selected_material_idx, image_writer, _defaultSamplerNearest);

		image_writer.update_set(_context.device, image_set);
	} else {
		for (uint32_t i = 0; i < _vk_materials.num_tex; i++) {
			writer.write_image(0, _error_checkerboard_image.imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		}

		image_writer.update_set(_context.device, image_set);
	}

	VkDescriptorSet skybox_set = get_current_frame()._frame_descriptors.allocate(_context.device, _skybox_descriptor_layout);
	{
		DescriptorWriter writer;
		writer.write_image(0, _skybox.imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		writer.update_set(_context.device, skybox_set);
	}

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _mesh_pipeline_layout, 0, 1, &image_set, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _mesh_pipeline_layout, 1, 1, &global_descriptor, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _mesh_pipeline_layout, 2, 1, &skybox_set, 0, nullptr);

	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3{ 0,0,-cam_move_test });
	view = glm::rotate(view, glm::radians(rotation_angle), glm::vec3(0, 1, 0));

	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_draw_extent.width / (float)_draw_extent.height, 10000.f, 0.1f);

	glm::mat4 model = glm::mat4(1.f);// glm::rotate(glm::mat4(1.f), glm::radians(rotation_angle), glm::vec3(0, 1, 0));

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	projection[1][1] *= -1;

	GPUDrawPushConstants push_constants;
	push_constants.worldMatrix = projection * view;
	push_constants.vertexBuffer = sphere_mesh[0]->meshBuffers.vertexBufferAddress;
	push_constants.model = model;
	push_constants.flags = shader_flags;
	push_constants.displacement_amount = displacement_amount;

	if (!sphere_mesh.empty() && !sphere_mesh[0]->surfaces.empty()) {
		push_constants.vertexBuffer = sphere_mesh[0]->meshBuffers.vertexBufferAddress;
		vkCmdPushConstants(cmd, _mesh_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
		vkCmdBindIndexBuffer(cmd, sphere_mesh[0]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, sphere_mesh[0]->surfaces[0].count, 1, sphere_mesh[0]->surfaces[0].startIndex, 0, 0);
	}
}

void VulkanEngine::draw_skybox(VkCommandBuffer cmd) {

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skybox_pipeline);

	VkDescriptorSet skybox_set = get_current_frame()._frame_descriptors.allocate(_context.device, _skybox_descriptor_layout);
	{
		DescriptorWriter writer;
		writer.write_image(0, _skybox.imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		writer.update_set(_context.device, skybox_set);
	}

	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_draw_extent.width / (float)_draw_extent.height, 10000.f, 0.1f);

	glm::mat4 model = glm::rotate(glm::mat4(1.f), glm::radians(rotation_angle), glm::vec3(0, 1, 0));

	projection[1][1] *= -1;

	GPUSkyboxPushConstants push_constants;
	push_constants.worldMatrix = projection;
	push_constants.model = model;

	vkCmdPushConstants(cmd, _skybox_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUSkyboxPushConstants), &push_constants);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _skybox_pipeline_layout, 0, 1, &skybox_set, 0, nullptr);

	vkCmdDraw(cmd, 36, 1, 0, 0);
}

void VulkanEngine::end_rendering(VkCommandBuffer cmd) {
	vkCmdEndRendering(cmd);
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	while (!bQuit) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT)
				bQuit = true;

			if (e.type == SDL_WINDOWEVENT) {
				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
					stop_rendering = true;
				}
				if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
					stop_rendering = false;
				}
			}
			ImGui_ImplSDL2_ProcessEvent(&e);
		}


		if (stop_rendering) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (resize_requested) {
			_vk_swapchain.resize_swapchain(resize_requested);
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		if (ImGui::Begin("background")) {
			ImGui::SliderFloat("Move Cam", &cam_move_test, 0.0f, 10.0f);
			ImGui::SliderFloat("Rotate Cam", &rotation_angle, -720.0f, 720.0f);
			ImGui::Text("Frame Time: %d", frame_time);

			ImGui::Checkbox("Albedo", &shader_flags_bools[0]);
			ImGui::Checkbox("Normals", &shader_flags_bools[1]);
			ImGui::Checkbox("Roughness", &shader_flags_bools[2]);
			ImGui::Checkbox("Metalness", &shader_flags_bools[3]);
			ImGui::Checkbox("Heights", &shader_flags_bools[4]);
			ImGui::Checkbox("AO", &shader_flags_bools[5]);
			
			ImGui::SliderFloat("Displacement", &displacement_amount, -0.5f, 0.5f);

			shader_flags = 0;

			for (int i = 5; i >= 0; i--) {
				shader_flags |= shader_flags_bools[i];
				if (i > 0) {
					shader_flags <<= 1;
				}
			}

		}
		  
		if (ImGui::TreeNodeEx("Select Material", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (size_t i = 0; i < _vk_materials._materials.size(); i++) {
				ImGui::PushID(i);
				if (ImGui::Selectable(_vk_materials._materials[i].name.c_str(), _selected_material_idx == i)) {
					_selected_material_idx = i;
				}

				ImGui::PopID();
			}
			ImGui::TreePop();
		}

		ImGui::End();

		ImGui::Render();

		draw();
	}
}

void VulkanEngine::init_descriptors()
{
	// create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};

	global_descriptor_allocator.init_pool(_context.device, 10, sizes);

	// make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		_draw_image_descriptor_layout = builder.build(_context.device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	// allocate a descriptor set for our draw image
	_draw_image_descriptors_allocator = global_descriptor_allocator.allocate(_context.device, _draw_image_descriptor_layout);

	DescriptorWriter writer;
	writer.write_image(0, _draw_image.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	writer.update_set(_context.device, _draw_image_descriptors_allocator);

	// make sure both the descriptor allocator and the new layout get cleaned up properly
	_main_deletion_queue.push_function([&]() {
		global_descriptor_allocator.destroy_pool(_context.device);

		vkDestroyDescriptorSetLayout(_context.device, _draw_image_descriptor_layout, nullptr);
		});

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		// create a descriptor pool
		std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		_frames[i]._frame_descriptors = DescriptorAllocatorGrowable{};
		_frames[i]._frame_descriptors.init(_context.device, 1000, frame_sizes);

		// also allocate a uniform buffer per frame so we can reuse until exit
		_frames[i]._GPU_scene_data_buffer = vkutil::create_buffer(_context, sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		_main_deletion_queue.push_function([&, i]() {
			_frames[i]._frame_descriptors.destroy_pools(_context.device);
			vkutil::destroy_buffer(_context, _frames[i]._GPU_scene_data_buffer);
			});
	}

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		_gpu_scene_data_descriptor_layout = builder.build(_context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	_main_deletion_queue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_context.device, _gpu_scene_data_descriptor_layout, nullptr);
		});

	{
		DescriptorLayoutBuilder builder;
		for (uint32_t i = 0; i < _vk_materials.num_tex; i++) {
			builder.add_binding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		}
		_material_descriptor_layout = builder.build(_context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	_main_deletion_queue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_context.device, _material_descriptor_layout, nullptr);
		});


	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		_skybox_descriptor_layout = builder.build(_context.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	_main_deletion_queue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_context.device, _skybox_descriptor_layout, nullptr);
		});
}

void VulkanEngine::init_pipelines()
{
	init_mesh_pipeline();
	init_skybox_pipeline();
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(_context.device, 1, &_imm_fence));
	VK_CHECK(vkResetCommandBuffer(_imm_command_buffer, 0));

	VkCommandBuffer cmd = _imm_command_buffer;

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _render_fence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphics_queue, 1, &submit, _imm_fence));

	VK_CHECK(vkWaitForFences(_context.device, 1, &_imm_fence, true, 9999999999));
}

void VulkanEngine::init_imgui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_context.device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();

	// this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_context.window);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _context.instance;
	init_info.PhysicalDevice = _context.chosen_GPU;
	init_info.Device = _context.device;
	init_info.Queue = _graphics_queue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	// dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_draw_image.imageFormat;


	init_info.MSAASamples = VK_SAMPLE_COUNT_4_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	_main_deletion_queue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(_context.device, imguiPool, nullptr);
		});
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo color_attachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
	color_attachment.resolveImageView = _resolve_image.imageView;
	color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkRenderingInfo renderInfo = vkinit::rendering_info(_draw_extent, &color_attachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void VulkanEngine::init_mesh_pipeline() {
	std::string frag_path = "shaders/main_frag.frag.spv";
	std::string vert_path = "shaders/main_vert.vert.spv";

	VkShaderModule frag_shader;
	VkShaderModule vert_shader;

	if (!vkutil::load_shader_module(frag_path.c_str(), _context.device, &frag_shader)) {
		throw std::runtime_error("Failed to load fragment shader");
	}

	if (!vkutil::load_shader_module(vert_path.c_str(), _context.device, &vert_shader)) {
		throw std::runtime_error("Failed to load vertex shader");
	}

	VkPushConstantRange buffer_range{};
	buffer_range.offset = 0;
	buffer_range.size = sizeof(GPUDrawPushConstants);
	buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	VkDescriptorSetLayout layouts[] = { _material_descriptor_layout, _gpu_scene_data_descriptor_layout, _skybox_descriptor_layout };

	pipeline_layout_info.pPushConstantRanges = &buffer_range;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = layouts;
	pipeline_layout_info.setLayoutCount = 3;
	VK_CHECK(vkCreatePipelineLayout(_context.device, &pipeline_layout_info, nullptr, &_mesh_pipeline_layout));

	PipelineBuilder pipeline_builder;

	//use the triangle layout we created
	pipeline_builder._pipelineLayout = _mesh_pipeline_layout;
	//connecting the vertex and pixel shaders to the pipeline
	pipeline_builder.set_shaders(vert_shader, frag_shader);
	//it will draw triangles
	pipeline_builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipeline_builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipeline_builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//no multisampling
	pipeline_builder.set_multisampling_MSAA();

	pipeline_builder.disable_blending();

	//pipeline_builder.disable_depthtest();
	pipeline_builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipeline_builder.set_color_attachment_format(_draw_image.imageFormat);
	pipeline_builder.set_depth_format(_depth_image.imageFormat);

	//finally build the pipeline
	_mesh_pipeline = pipeline_builder.build_pipeline(_context.device);

	//clean structures
	vkDestroyShaderModule(_context.device, frag_shader, nullptr);
	vkDestroyShaderModule(_context.device, vert_shader, nullptr);

	_main_deletion_queue.push_function([&]() {
		vkDestroyPipelineLayout(_context.device, _mesh_pipeline_layout, nullptr);
		vkDestroyPipeline(_context.device, _mesh_pipeline, nullptr);
		});
}

void VulkanEngine::init_skybox_pipeline() {
	std::string frag_path = "shaders/skybox_frag.frag.spv";
	std::string vert_path = "shaders/skybox_vert.vert.spv";

	VkShaderModule frag_shader;
	VkShaderModule vert_shader;

	if (!vkutil::load_shader_module(frag_path.c_str(), _context.device, &frag_shader)) {
		throw std::runtime_error("Failed to load fragment shader");
	}

	if (!vkutil::load_shader_module(vert_path.c_str(), _context.device, &vert_shader)) {
		throw std::runtime_error("Failed to load vertex shader");
	}

	VkPushConstantRange buffer_range{};
	buffer_range.offset = 0;
	buffer_range.size = sizeof(GPUSkyboxPushConstants);
	buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	VkDescriptorSetLayout layouts[] = { _skybox_descriptor_layout };

	pipeline_layout_info.pPushConstantRanges = &buffer_range;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pSetLayouts = layouts;
	pipeline_layout_info.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(_context.device, &pipeline_layout_info, nullptr, &_skybox_pipeline_layout));

	PipelineBuilder pipeline_builder;

	//use the triangle layout we created
	pipeline_builder._pipelineLayout = _skybox_pipeline_layout;
	//connecting the vertex and pixel shaders to the pipeline
	pipeline_builder.set_shaders(vert_shader, frag_shader);
	//it will draw triangles
	pipeline_builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//filled triangles
	pipeline_builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	//no backface culling
	pipeline_builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	//ms
	pipeline_builder.set_multisampling_MSAA();

	pipeline_builder.disable_blending();

	//pipeline_builder.disable_depthtest();
	pipeline_builder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//connect the image format we will draw into, from draw image
	pipeline_builder.set_color_attachment_format(_draw_image.imageFormat);
	pipeline_builder.set_depth_format(_depth_image.imageFormat);

	//finally build the pipeline
	_skybox_pipeline = pipeline_builder.build_pipeline(_context.device);

	//clean structures
	vkDestroyShaderModule(_context.device, frag_shader, nullptr);
	vkDestroyShaderModule(_context.device, vert_shader, nullptr);

	_main_deletion_queue.push_function([&]() {
		vkDestroyPipelineLayout(_context.device, _skybox_pipeline_layout, nullptr);
		vkDestroyPipeline(_context.device, _skybox_pipeline, nullptr);
		});
}

void VulkanEngine::init_default_data() {
	clear_color = { {0.1, 0.1, 0.1, 1.0} };

	sphere_mesh = loadGltfMeshes(this, "assets/icosphere.glb").value();

	_main_deletion_queue.push_function([&]() {
		for (auto& mesh : sphere_mesh) {
			vkutil::destroy_buffer(_context, mesh->meshBuffers.indexBuffer);
			vkutil::destroy_buffer(_context, mesh->meshBuffers.vertexBuffer);
		}
		});

	auto pack = [](glm::vec4 v) -> uint32_t {
		uint8_t r = (uint8_t)(v.x * 255.0f);
		uint8_t g = (uint8_t)(v.y * 255.0f);
		uint8_t b = (uint8_t)(v.z * 255.0f);
		uint8_t a = (uint8_t)(v.w * 255.0f);
		return r | (g << 8) | (b << 16) | (a << 24);
		};

	//checkerboard image
	uint32_t magenta = pack(glm::vec4(1, 0, 1, 1));
	uint32_t black = pack(glm::vec4(0, 0, 0, 0));
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? black : magenta;
		}
	}
	_error_checkerboard_image = vkutil::create_image(_context, (void*)pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_SAMPLE_COUNT_1_BIT);

	std::array<uint32_t, 1> black_pix;//TODO: temp
	black_pix[0] = 0;
	AllocatedImage black_image = vkutil::create_image(_context, (void*)black_pix.data(), VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT, false, VK_SAMPLE_COUNT_1_BIT);

	_vk_materials.init_materials(_context, black_image);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampl.minLod = 0.0f;
	sampl.maxLod = VK_LOD_CLAMP_NONE;
	sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_context.device, &sampl, nullptr, &_defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_context.device, &sampl, nullptr, &_default_sampler_linear);

	_main_deletion_queue.push_function([&]() {
		vkDestroySampler(_context.device, _defaultSamplerNearest, nullptr);
		vkDestroySampler(_context.device, _default_sampler_linear, nullptr);

		vkutil::destroy_image(_context, _error_checkerboard_image);
		});

	GPUSceneData default_data;
	default_data.light_dir = glm::vec4(1.0, -3.0, -1.0, 1.0);
	default_data.light_col = glm::vec4(1.0, 1.0, 1.0, 1.0);
	scene_data = default_data;

	_vk_materials.upload_material("assets/brick");
	_vk_materials.upload_material("assets/steel");
	_vk_materials.upload_material("assets/rock");

	shader_flags_bools[0] = true; // on albedo
	shader_flags_bools[1] = true; // on normal maps
	shader_flags_bools[2] = true; // on roughness
	shader_flags_bools[3] = true; // on metalness
	shader_flags_bools[4] = true; // on heightfield
	shader_flags_bools[5] = true; // on AO

	uint32_t shader_flags = 0;
	shader_flags |= shader_flags_bools[5];
	shader_flags <<= 1;
	shader_flags |= shader_flags_bools[4];
	shader_flags <<= 1;
	shader_flags |= shader_flags_bools[3];
	shader_flags <<= 1;
	shader_flags |= shader_flags_bools[2];
	shader_flags <<= 1;
	shader_flags |= shader_flags_bools[1];
	shader_flags <<= 1;
	shader_flags |= shader_flags_bools[0];

	VkImageUsageFlags skybox_usage_flags{};
	skybox_usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	skybox_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	skybox_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;


	// load skybox

	int img_width = 0;
	int img_height = 0;
	int img_channels = 0;

	std::vector<void*> skybox_data;
	stbi_uc* skybox_pixels;

	skybox_pixels = stbi_load("assets/skybox/posx.jpg", &img_width, &img_height, &img_channels, STBI_rgb_alpha);
	skybox_data.push_back(skybox_pixels);
	skybox_pixels = stbi_load("assets/skybox/negx.jpg", &img_width, &img_height, &img_channels, STBI_rgb_alpha);
	skybox_data.push_back(skybox_pixels);
	skybox_pixels = stbi_load("assets/skybox/posy.jpg", &img_width, &img_height, &img_channels, STBI_rgb_alpha);
	skybox_data.push_back(skybox_pixels);
	skybox_pixels = stbi_load("assets/skybox/negy.jpg", &img_width, &img_height, &img_channels, STBI_rgb_alpha);
	skybox_data.push_back(skybox_pixels);
	skybox_pixels = stbi_load("assets/skybox/posz.jpg", &img_width, &img_height, &img_channels, STBI_rgb_alpha);
	skybox_data.push_back(skybox_pixels);
	skybox_pixels = stbi_load("assets/skybox/negz.jpg", &img_width, &img_height, &img_channels, STBI_rgb_alpha);
	skybox_data.push_back(skybox_pixels);

	VkExtent3D skybox_extent = { img_width, img_height, 1 };

	_skybox = vkutil::create_skybox(_context, skybox_data, VK_FORMAT_R8G8B8A8_UNORM, skybox_usage_flags, skybox_extent);

	_main_deletion_queue.push_function([&]() {
		vkutil::destroy_image(_context, _skybox);
		});
}