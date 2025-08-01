#include "VulkanInternal.hpp"

namespace Renderer::Vulkan
{

	extern VulkanContext* context;

	void vk_command_buffer_allocate(VulkanCommandPool pool, bool8 primary, VulkanCommandBuffer* out_buffer)
	{

		*out_buffer = {};

		VkCommandBufferAllocateInfo all_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		all_info.commandPool = pool.handle;
		all_info.level = primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		all_info.commandBufferCount = 1;
		all_info.pNext = 0;

		out_buffer->state = VulkanCommandBufferState::NOT_ALLOCATED;
		VK_CHECK(vkAllocateCommandBuffers(context->device.logical_device, &all_info, &out_buffer->handle));
		out_buffer->state = VulkanCommandBufferState::READY;

	}

	void vk_command_buffer_free(VulkanCommandPool pool, VulkanCommandBuffer* buffer)
	{
		vkFreeCommandBuffers(context->device.logical_device, pool.handle, 1, &buffer->handle);
		buffer->handle = 0;
		buffer->state = VulkanCommandBufferState::NOT_ALLOCATED;
	}

	void vk_command_buffer_begin(VulkanCommandBuffer* buffer, bool8 single_use, bool8 renderpass_continue, bool8 simultaneous_use)
	{
		VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		begin_info.flags = 0;
		if (single_use)
			begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (renderpass_continue)
			begin_info.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		if (simultaneous_use)
			begin_info.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VK_CHECK(vkBeginCommandBuffer(buffer->handle, &begin_info));
		buffer->state = VulkanCommandBufferState::RECORDING;
	}

	void vk_command_buffer_end(VulkanCommandBuffer* buffer)
	{
		VK_CHECK(vkEndCommandBuffer(buffer->handle));
		buffer->state = VulkanCommandBufferState::RECORDING_ENDED;
	}

	void vk_command_buffer_update_submitted(VulkanCommandBuffer* buffer)
	{
		buffer->state = VulkanCommandBufferState::SUBMITTED;
	}

	void vk_command_buffer_reset(VulkanCommandBuffer* buffer)
	{
		buffer->state = VulkanCommandBufferState::READY;
	}

	void vk_command_buffer_reserve_and_begin_single_use(VulkanCommandPool pool, VulkanCommandBuffer* out_buffer)
	{
		vk_command_buffer_allocate(pool, true, out_buffer);
		vk_command_buffer_begin(out_buffer, true, false, false);
	}

	void vk_command_buffer_end_single_use(VulkanCommandPool pool, VulkanCommandBuffer* buffer, VkQueue queue)
	{
		vk_command_buffer_end(buffer);

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &buffer->handle;
		VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, 0));
		VK_CHECK(vkQueueWaitIdle(queue));

		vk_command_buffer_free(pool, buffer);
	}
}
