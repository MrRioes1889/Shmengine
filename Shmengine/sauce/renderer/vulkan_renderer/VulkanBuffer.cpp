#include "VulkanBuffer.hpp"

#include "VulkanDevice.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanUtils.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"

namespace Renderer::Vulkan
{
	bool32 buffer_create(
		VulkanContext* context,
		uint64 size,
		VkBufferUsageFlagBits usage,
		uint32 memory_property_flags,
		bool32 bind_on_create,
		VulkanBuffer* out_buffer)
	{

		Memory::zero_memory(out_buffer, sizeof(*out_buffer));
		out_buffer->size = size;
		out_buffer->usage = usage;
		out_buffer->memory_property_flags = memory_property_flags;

		uint64 freelist_nodes_size = Freelist::get_required_nodes_array_memory_size_by_node_count(10000, AllocatorPageSize::SMALL);
		out_buffer->freelist_data.init(freelist_nodes_size, AllocationTag::MAIN);
		out_buffer->freelist.init(size, freelist_nodes_size, out_buffer->freelist_data.data, AllocatorPageSize::SMALL, 10000);

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
			out_buffer->freelist.destroy();
			out_buffer->freelist_data.free_data();
			return false;
		}

		VkMemoryAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocate_info.allocationSize = requirements.size;
		allocate_info.memoryTypeIndex = (uint32)out_buffer->memory_index;

		VkResult res = vkAllocateMemory(context->device.logical_device, &allocate_info, context->allocator_callbacks, &out_buffer->memory);
		if (res != VK_SUCCESS)
		{
			SHMERROR("Unable to create vulkan buffer. Failed to allocate memory.");
			out_buffer->freelist.destroy();
			out_buffer->freelist_data.free_data();
			return false;
		}

		if (bind_on_create)
			buffer_bind(context, out_buffer, 0);

		return true;

	}

	void buffer_destroy(VulkanContext* context, VulkanBuffer* buffer)
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

		buffer->freelist.destroy();
		buffer->freelist_data.free_data();

		buffer->size = 0;
		buffer->is_locked = false;

	}

	void buffer_resize(VulkanContext* context, uint64 new_size, VulkanBuffer* buffer, VkQueue queue, VkCommandPool pool)
	{

		SHMASSERT(new_size >= buffer->size);

		uint64 freelist_nodes_size = Freelist::get_required_nodes_array_memory_size_by_node_count(10000, AllocatorPageSize::SMALL);
		buffer->freelist_data.resize(new_size);
		buffer->freelist.resize(buffer->freelist_data.data, new_size);

		VulkanBuffer old_buffer = *buffer;
		if (!buffer_create(context, new_size, buffer->usage, buffer->memory_property_flags, true, buffer))
		{
			SHMERROR("Failed to create new buffer for resizing operation.");
			return;
		}

		buffer_copy_to(context, pool, 0, queue, old_buffer.handle, 0, buffer->handle, 0, buffer->size);
		buffer->freelist_data.steal(old_buffer.freelist_data);
		buffer_destroy(context, &old_buffer);		

	}

	bool32 buffer_allocate(VulkanBuffer* buffer, uint64 size, uint64* out_offset)
	{
		return buffer->freelist.allocate(size, out_offset);
	}

	bool32 buffer_free(VulkanBuffer* buffer, uint64 offset)
	{
		return buffer->freelist.free(offset);
	}

	void buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64 offset)
	{
		VK_CHECK(vkBindBufferMemory(context->device.logical_device, buffer->handle, buffer->memory, offset));
	}

	void* buffer_lock_memory(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags)
	{
		void* data;
		VK_CHECK(vkMapMemory(context->device.logical_device, buffer->memory, offset, size, flags, &data));
		return data;
	}

	void buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer)
	{
		vkUnmapMemory(context->device.logical_device, buffer->memory);
	}

	void buffer_load_data(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags, const void* data)
	{
		void* buffer_data = buffer_lock_memory(context, buffer, offset, size, flags);
		Memory::copy_memory(data, buffer_data, size);
		buffer_unlock_memory(context, buffer);
	}

	void buffer_copy_to(
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
}

