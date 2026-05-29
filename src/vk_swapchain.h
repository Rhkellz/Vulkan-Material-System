#pragma once
#include "vk_types.h"
#include "vk_initializers.h"
#include "VkBootstrap.h"

class Swapchain {
public:
	Swapchain(const VkPhysicalDevice& chosenGPU,
		const VkSurfaceKHR& surface, 
		const VmaAllocator& allocator,
		const VkDevice& _device,
		const DeletionQueue& deletion_queue,
		uint32_t width, uint32_t height);

	void resize_swapchain();

	void destroy_swapchain();

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;

	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	VkExtent2D _swapchain_extent;

	VkDevice _device;
	VkPhysicalDevice _chosen_GPU;
	VkSurfaceKHR _surface;
	VmaAllocator _allocator;
	DeletionQueue _deletion_queue;
private:
	void create_swapchain(uint32_t width, uint32_t height);
};