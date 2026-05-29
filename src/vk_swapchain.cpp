#include "vk_swapchain.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

Swapchain::Swapchain(const VkPhysicalDevice& chosen_GPU, 
	const VkSurfaceKHR& surface,
	const VmaAllocator& allocator, 
	const VkDevice& device, 
	const DeletionQueue& deletion_queue, 
	uint32_t width, uint32_t height) : 
	_deletion_queue(deletion_queue), 
	_device(device), 
	_allocator(allocator),
	_chosen_GPU(chosen_GPU),
	_surface(surface)
{
	create_swapchain(width, height);
}

void Swapchain::create_swapchain(uint32_t width, uint32_t height) {
	vkb::SwapchainBuilder swapchainBuilder{ _chosen_GPU, _device, _surface };

	_swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

	VkSurfaceFormatKHR desiredFormat{};
	desiredFormat.format = _swapchain_image_format;
	desiredFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainBuilder.set_desired_format(desiredFormat);

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(desiredFormat)
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // important, vsync
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	_swapchain_extent = vkbSwapchain.extent;
	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchain_images = vkbSwapchain.get_images().value();
	_swapchain_image_views = vkbSwapchain.get_image_views().value();
}

void Swapchain::destroy_swapchain() {
	vkDestroySwapchainKHR(_device, _swapchain, nullptr);

	for (size_t i = 0; i < _swapchain_images.size(); i++) {
		vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);//dont need to destroy _swapchain_images as vkDestroySwapchainKHR already does
	}
}

void Swapchain::resize_swapchain() {
	/*
	vkDeviceWaitIdle(_device);

	destroy_swapchain();

	int w, h;
	SDL_GetWindowSize(_window, &w, &h);
	_window_extent.width = w;
	_window_extent.height = h;

	create_swapchain(_window_extent.width, _window_extent.height);

	resize_requested = false;*/
}
