#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{
	void vulkan_command_buffer_allocate(
		VulkanContext* context,
		VkCommandPool pool,
		bool32 primary,
		VulkanCommandBuffer* out_buffer
	);
	void vulkan_command_buffer_free(
		VulkanContext* context,
		VkCommandPool pool,
		VulkanCommandBuffer* buffer
	);

	void vulkan_command_buffer_begin(
		VulkanCommandBuffer* buffer,
		bool32 single_use,
		bool32 renderpass_continue,
		bool32 simultaneous_use
	);
	void vulkan_command_buffer_end(VulkanCommandBuffer* buffer);

	void vulkan_command_update_submitted(VulkanCommandBuffer* buffer);

	void vulkan_command_reset(VulkanCommandBuffer* buffer);

	void vulkan_command_buffer_reserve_and_begin_single_use(
		VulkanContext* context,
		VkCommandPool pool,
		VulkanCommandBuffer* out_buffer
	);
	void vulkan_command_buffer_end_single_use(
		VulkanContext* context,
		VkCommandPool pool,
		VulkanCommandBuffer* buffer,
		VkQueue queue
	);
}