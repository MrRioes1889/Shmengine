#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{	

	const char* vk_result_string(VkResult result, bool32 get_extended);
	bool32 vk_result_is_success(VkResult result);

	bool32 vk_device_create();
	void vk_device_destroy();
	void vk_device_query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanSwapchainSupportInfo* out_support_info);
	bool32 vk_device_detect_depth_format(VulkanDevice* device);

	void vk_command_buffer_allocate(VkCommandPool pool, bool32 primary, VulkanCommandBuffer* out_buffer);
	void vk_command_buffer_free(VkCommandPool pool, VulkanCommandBuffer* buffer);
	void vk_command_buffer_begin(VulkanCommandBuffer* buffer, bool32 single_use, bool32 renderpass_continue, bool32 simultaneous_use);
	void vk_command_buffer_end(VulkanCommandBuffer* buffer);
	void vk_command_buffer_update_submitted(VulkanCommandBuffer* buffer);
	void vk_command_buffer_reset(VulkanCommandBuffer* buffer);
	void vk_command_buffer_reserve_and_begin_single_use(VkCommandPool pool, VulkanCommandBuffer* out_buffer);
	void vk_command_buffer_end_single_use(VkCommandPool pool, VulkanCommandBuffer* buffer, VkQueue queue);

	bool32 vk_pipeline_create(const VulkanPipelineConfig* config, VulkanPipeline* out_pipeline);
	void vk_pipeline_destroy(VulkanPipeline* pipeline);
	void vk_pipeline_bind(VulkanCommandBuffer* command_buffer, VkPipelineBindPoint bind_point, VulkanPipeline* pipeline);

	bool32 vk_swapchain_create(uint32 width, uint32 height, VulkanSwapchain* out_swapchain);
	bool32 vk_swapchain_recreate(uint32 width, uint32 height, VulkanSwapchain* swapchain);
	void vk_swapchain_destroy(VulkanSwapchain* swapchain);
	bool32 vk_swapchain_acquire_next_image_index(VulkanSwapchain* swapchain, uint64 timeout_ns, VkSemaphore image_available_semaphore, VkFence fence, uint32* out_image_index);
	void vk_swapchain_present(VulkanSwapchain* swapchain, VkQueue present_queue, VkSemaphore render_complete_semaphore, uint32 present_image_index);

	bool32 vk_buffer_create_internal(VulkanBuffer* buffer, RenderbufferType type, uint64 size);
	void vk_buffer_destroy_internal(VulkanBuffer* buffer);
	bool32 vk_buffer_resize_internal(VulkanBuffer* buffer, uint64 old_size, uint64 new_size);
	void* vk_buffer_map_memory_internal(VulkanBuffer* buffer, uint64 offset, uint64 size);
	void vk_buffer_unmap_memory_internal(VulkanBuffer* buffer);
	bool32 vk_buffer_bind_internal(VulkanBuffer* buffer, uint64 offset);
	bool32 vk_buffer_unbind_internal(VulkanBuffer* buffer);
	bool32 vk_buffer_load_range_internal(VulkanBuffer* buffer, uint64 offset, uint64 size, const void* data);
	bool32 vk_buffer_copy_range_internal(VkBuffer source, uint64 source_offset, VkBuffer dest, uint64 dest_offset, uint64 size);
	bool32 vk_buffer_read_internal(VulkanBuffer* buffer, uint64 offset, uint64 size, void* out_memory);

	void vk_image_create(
		TextureType type,
		uint32 width,
		uint32 height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags memory_flags,
		bool32 create_view,
		VkImageAspectFlags view_aspect_flags,
		VulkanImage* out_image
	);
	void vk_image_destroy(VulkanImage* image);
	void vk_image_view_create(TextureType type, VkFormat format, VulkanImage* image, VkImageAspectFlags aspect_flags);
	void vk_image_transition_layout(TextureType type, VulkanCommandBuffer* command_buffer, VulkanImage* image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
	void vk_image_copy_from_buffer(TextureType type, VulkanImage* image, VkBuffer buffer, VulkanCommandBuffer* command_buffer);
	void vk_image_copy_to_buffer(TextureType type, VulkanImage* image, VkBuffer buffer, VulkanCommandBuffer* command_buffer);
	void vk_image_copy_pixel_to_buffer(TextureType type, VulkanImage* image, VkBuffer buffer, uint32 x, uint32 y, VulkanCommandBuffer* command_buffer);
	
}


