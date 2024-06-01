#include "VulkanBuffer.hpp"

#include "VulkanDevice.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanUtils.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"

bool32 vulkan_buffer_create(
	VulkanContext* context,
	uint64 size,
	VkBufferUsageFlagBits usage,
	uint32 memory_property_flags,
	bool32 bind_on_create,
	VulkanBuffer* out_buffer)
{

	*out_buffer = {};
	out_buffer->size = size;
	out_buffer->usage = usage;
	out_buffer->memory_property_flags = memory_property_flags;

	VkBufferCreateInfo buffer_create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_create_info.size = size;
	buffer_create_info.usage = usage;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // NOTE: Only used in one queue

	VK_CHECK(vkCreateBuffer(context->device.logical_device, &buffer_create_info, context->allocator_callbacks, &out_buffer->handle));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(context->device.logical_device, out_buffer->handle, &requirements);
	out_buffer->memory_index = context->find_memory_index(requirements.memoryTypeBits, out_buffer->memory_property_flags);
	if (out_buffer->memory_index < 0)
	{
		SHMERROR("Unable to create vulkan buffer because the required memory type index was not found");
		return false;
	}

	VkMemoryAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocate_info.allocationSize = requirements.size;
	allocate_info.memoryTypeIndex = (uint32)out_buffer->memory_index;

	VkResult res = vkAllocateMemory(context->device.logical_device, &allocate_info, context->allocator_callbacks, &out_buffer->memory);
	if (res != VK_SUCCESS)
	{
		SHMERROR("Unable to create vulkan buffer. Failed to allocate memory.");
		return false;
	}

	if (bind_on_create)
		vulkan_buffer_bind(context, out_buffer, 0);

	return true;

}

void vulkan_buffer_destroy(VulkanContext* context, VulkanBuffer* buffer)
{

	vkDeviceWaitIdle(context->device.logical_device);
	if (buffer->memory)
	{
		vkFreeMemory(context->device.logical_device, buffer->memory, context->allocator_callbacks);
		buffer->memory = 0;
	}
	if (buffer->handle)
	{
		vkDestroyBuffer(context->device.logical_device, buffer->handle, context->allocator_callbacks);
		buffer->handle = 0;
	}
	buffer->size = 0;
	buffer->is_locked = false;

}

void vulkan_buffer_resize(VulkanContext* context, uint64 new_size, VulkanBuffer* buffer, VkQueue queue, VkCommandPool pool)
{

	VulkanBuffer new_buffer;
	if (!vulkan_buffer_create(context, new_size, buffer->usage, buffer->memory_property_flags, true, &new_buffer))
	{
		SHMERROR("Failed to create new buffer for resizing operation.");
		return;
	}

	vulkan_buffer_copy_to(context, pool, 0, queue, buffer->handle, 0, new_buffer.handle, 0, buffer->size);

	vulkan_buffer_destroy(context, buffer);
	*buffer = new_buffer;

}

void vulkan_buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64 offset)
{
	VK_CHECK(vkBindBufferMemory(context->device.logical_device, buffer->handle, buffer->memory, offset));
}

void* vulkan_buffer_lock_memory(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags)
{
	void* data;
	VK_CHECK(vkMapMemory(context->device.logical_device, buffer->memory, offset, size, flags, &data));
	return data;
}

void vulkan_buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer)
{
	vkUnmapMemory(context->device.logical_device, buffer->memory);
}

void vulkan_buffer_load_data(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags, const void* data)
{
	void* buffer_data = vulkan_buffer_lock_memory(context, buffer, offset, size, flags);
	Memory::copy_memory(data, buffer_data, size);
	vulkan_buffer_unlock_memory(context, buffer);
}

void vulkan_buffer_copy_to(
	VulkanContext* context,
	VkCommandPool pool,
	VkFence fence,
	VkQueue queue,
	VkBuffer source,
	uint64 source_offset,
	VkBuffer dest,
	uint64 dest_offset,
	uint64 size)
{

	vkQueueWaitIdle(queue);

	VulkanCommandBuffer tmp_command_buffer;
	vulkan_command_buffer_reserve_and_begin_single_use(context, pool, &tmp_command_buffer);

	VkBufferCopy copy_region;
	copy_region.srcOffset = source_offset;
	copy_region.dstOffset = dest_offset;
	copy_region.size = size;

	vkCmdCopyBuffer(tmp_command_buffer.handle, source, dest, 1, &copy_region);

	vulkan_command_buffer_end_single_use(context, pool, &tmp_command_buffer, queue);

}