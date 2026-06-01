#pragma once
#include "vk_types.h"
#include "vk_initializers.h"
#include "VkBootstrap.h"
#include <SDL.h>
#include <SDL_vulkan.h>

class VulkanSwapchain {
public:
	void init_swapchain(Context context);
	
	void create_swapchain(uint32_t width, uint32_t height);

	void destroy_swapchain();

	void resize_swapchain(bool& resize_requested);

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;

	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	VkExtent2D _swapchain_extent;

private:
	Context _context;
};