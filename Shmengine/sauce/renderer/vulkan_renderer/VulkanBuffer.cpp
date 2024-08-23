#include "VulkanBackend.hpp"
#include "VulkanDevice.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanUtils.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"


namespace Renderer::Vulkan
{

	extern VulkanContext context;

	static SHMINLINE bool32 buffer_is_device_local(const VulkanBuffer* buffer)
	{
		return (buffer->memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	static SHMINLINE bool32 buffer_is_host_visible(const VulkanBuffer* buffer)
	{
		return (buffer->memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}

	static SHMINLINE bool32 buffer_is_host_coherent(const VulkanBuffer* buffer)
	{
		return (buffer->memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	static bool32 vk_buffer_create_internal(VulkanBuffer* buffer, RenderbufferType type, uint64 size);
	static void vk_buffer_destroy_internal(VulkanBuffer* buffer);
	static bool32 vk_buffer_resize_internal(VulkanBuffer* buffer, uint64 old_size, uint64 new_size);
	static bool32 vk_buffer_bind_internal(VulkanBuffer* buffer, uint64 offset);
	static bool32 vk_buffer_unbind_internal(VulkanBuffer* buffer);
	static bool32 vk_buffer_load_range_internal(VulkanBuffer* buffer, uint64 offset, uint64 size, const void* data);
	static bool32 vk_buffer_copy_range_internal(VkBuffer source, uint64 source_offset, VkBuffer dest, uint64 dest_offset, uint64 size);

	bool32 vk_buffer_create(Renderbuffer* buffer)
	{
		buffer->internal_data.init(sizeof(VulkanBuffer), 0, AllocationTag::VULKAN);
		if (!vk_buffer_create_internal((VulkanBuffer*)buffer->internal_data.data, buffer->type, buffer->size))
		{
			buffer->internal_data.free_data();
			return false;
		}

		return true;
	}

	static bool32 vk_buffer_create_internal(VulkanBuffer* buffer, RenderbufferType type, uint64 size)
	{

		switch (type) {
		case RenderbufferType::VERTEX:
		{
			buffer->usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		}		
		case RenderbufferType::INDEX:
		{
			buffer->usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		}		
		case RenderbufferType::UNIFORM:
		{
			uint32 device_local_bits = context.device.supports_device_local_host_visible ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
			buffer->usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | device_local_bits;
			break;
		}	
		case RenderbufferType::STAGING:
		{
			buffer->usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}
		case RenderbufferType::READ:
		{
			buffer->usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}
		case RenderbufferType::STORAGE:
		{
			SHMERROR("Storage buffer not yet supported.");
			return false;
		}
		default:
		{
			SHMERRORV("Unsupported buffer type: %u", type);
			return false;
		}
		}

		VkBufferCreateInfo buffer_create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		buffer_create_info.size = size;
		buffer_create_info.usage = buffer->usage;
		buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // NOTE: Only used in one queue

		VK_CHECK(vkCreateBuffer(context.device.logical_device, &buffer_create_info, context.allocator_callbacks, &buffer->handle));

		vkGetBufferMemoryRequirements(context.device.logical_device, buffer->handle, &buffer->memory_requirements);
		buffer->memory_index = context.find_memory_index(buffer->memory_requirements.memoryTypeBits, buffer->memory_property_flags);
		if (buffer->memory_index < 0)
		{
			SHMERROR("Unable to create vulkan buffer because the required memory type index was not found");		
			return false;
		}

		VkMemoryAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocate_info.allocationSize = buffer->memory_requirements.size;
		allocate_info.memoryTypeIndex = (uint32)buffer->memory_index;

		VkResult res = vkAllocateMemory(context.device.logical_device, &allocate_info, context.allocator_callbacks, &buffer->memory);
		if (res != VK_SUCCESS)
		{
			SHMERROR("Unable to create vulkan buffer. Failed to allocate memory.");
			return false;
		}

		bool32 is_device_memory = buffer_is_device_local(buffer);
		Memory::track_external_allocation(buffer->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);

		return true;

	}

	void vk_buffer_destroy(Renderbuffer* buffer)
	{
		if (!buffer->internal_data.data)
			return;

		VulkanBuffer* v_buffer = (VulkanBuffer*)buffer->internal_data.data;
		vk_buffer_destroy_internal(v_buffer);
		buffer->internal_data.free_data();
	}

	static void vk_buffer_destroy_internal(VulkanBuffer* buffer)
	{
	
		vkDeviceWaitIdle(context.device.logical_device);

		if (buffer->memory)
		{
			vkFreeMemory(context.device.logical_device, buffer->memory, context.allocator_callbacks);
			buffer->memory = 0;
		}
		if (buffer->handle)
		{
			vkDestroyBuffer(context.device.logical_device, buffer->handle, context.allocator_callbacks);
			buffer->handle = 0;
		}

		bool32 is_device_memory = (buffer->memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Memory::track_external_free(buffer->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);

		buffer->usage = 0;
		buffer->is_locked = false;

	}

	bool32 vk_buffer_resize(Renderbuffer* buffer, uint64 new_size)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_resize_internal(internal_buffer, buffer->size, new_size);
	}

	static bool32 vk_buffer_resize_internal(VulkanBuffer* buffer, uint64 old_size, uint64 new_size)
	{

		// Create new buffer.
		VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		buffer_info.size = new_size;
		buffer_info.usage = buffer->usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // NOTE: Only used in one queue.

		VkBuffer new_buffer;
		VK_CHECK(vkCreateBuffer(context.device.logical_device, &buffer_info, context.allocator_callbacks, &new_buffer));

		// Gather memory requirements.
		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(context.device.logical_device, new_buffer, &requirements);

		// Allocate memory info
		VkMemoryAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocate_info.allocationSize = requirements.size;
		allocate_info.memoryTypeIndex = (uint32)buffer->memory_index;

		// Allocate the memory.
		VkDeviceMemory new_memory;
		VkResult result = vkAllocateMemory(context.device.logical_device, &allocate_info, context.allocator_callbacks, &new_memory);
		if (result != VK_SUCCESS) {
			SHMERRORV("Unable to resize vulkan buffer because the required memory allocation failed. Error: %u", result);
			return false;
		}

		// Bind the new buffer's memory
		VK_CHECK(vkBindBufferMemory(context.device.logical_device, new_buffer, new_memory, 0));

		// Copy over the data.
		vk_buffer_copy_range_internal(buffer->handle, 0, new_buffer, 0, old_size);

		// Make sure anything potentially using these is finished.
		// NOTE: We could use vkQueueWaitIdle here if we knew what queue this buffer would be used with...
		vkDeviceWaitIdle(context.device.logical_device);

		// Destroy the old
		if (buffer->memory) {
			vkFreeMemory(context.device.logical_device, buffer->memory, context.allocator_callbacks);
			buffer->memory = 0;
		}
		if (buffer->handle) {
			vkDestroyBuffer(context.device.logical_device, buffer->handle, context.allocator_callbacks);
			buffer->handle = 0;
		}

		// Report free of the old, allocate of the new.
		bool32 is_device_memory = buffer_is_device_local(buffer);

		Memory::track_external_free(buffer->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);
		buffer->memory_requirements = requirements;
		Memory::track_external_allocation(buffer->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);

		// Set new properties
		buffer->memory = new_memory;
		buffer->handle = new_buffer;

		return true;

	}

	bool32 vk_buffer_bind(Renderbuffer* buffer, uint64 offset)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_bind_internal(internal_buffer, offset);
	}

	static bool32 vk_buffer_bind_internal(VulkanBuffer* buffer, uint64 offset)
	{
		VK_CHECK(vkBindBufferMemory(context.device.logical_device, buffer->handle, buffer->memory, offset));
		return true;
	}

	bool32 vk_buffer_unbind(Renderbuffer* buffer)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_unbind_internal(internal_buffer);
	}

	static bool32 vk_buffer_unbind_internal(VulkanBuffer* buffer)
	{
		return true;
	}

	void* vk_buffer_map_memory(Renderbuffer* buffer, uint64 offset, uint64 size)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		void* data;
		VK_CHECK(vkMapMemory(context.device.logical_device, internal_buffer->memory, offset, size, 0, &data));
		return data;
	}

	void vk_buffer_unmap_memory(Renderbuffer* buffer, uint64 offset, uint64 size)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		vkUnmapMemory(context.device.logical_device, internal_buffer->memory);
	}

	bool32 vk_buffer_flush(Renderbuffer* buffer, uint64 offset, uint64 size)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		if (!buffer_is_host_coherent(internal_buffer))
			return false;

		VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		range.memory = internal_buffer->memory;
		range.offset = offset;
		range.size = size;
		VK_CHECK(vkFlushMappedMemoryRanges(context.device.logical_device, 1, &range));

		return true;
	}

	bool32 vk_buffer_read(Renderbuffer* buffer, uint64 offset, uint64 size, void** out_memory)
	{

		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		if (!buffer_is_device_local(internal_buffer) || buffer_is_host_visible(internal_buffer)) 
		{

			void* data_ptr;
			VK_CHECK(vkMapMemory(context.device.logical_device, internal_buffer->memory, offset, size, 0, &data_ptr));
			Memory::copy_memory(data_ptr, *out_memory, size);
			vkUnmapMemory(context.device.logical_device, internal_buffer->memory);
			return true;
			
		}

		VulkanBuffer read;
		if (!vk_buffer_create_internal(&read, RenderbufferType::READ, size)) {
			SHMERROR("vk_buffer_read - Failed to create read buffer.");
			return false;
		}
		vk_buffer_bind_internal(&read, 0);

		vk_buffer_copy_range_internal(internal_buffer->handle, offset, read.handle, 0, size);

		// Map/copy/unmap
		void* mapped_data;
		VK_CHECK(vkMapMemory(context.device.logical_device, read.memory, 0, size, 0, &mapped_data));
		Memory::copy_memory(mapped_data, *out_memory, size);
		vkUnmapMemory(context.device.logical_device, read.memory);

		// Clean up the read buffer.
		vk_buffer_unbind_internal(&read);
		vk_buffer_destroy_internal(&read);

		return true;

	}

	bool32 vk_buffer_load_range(Renderbuffer* buffer, uint64 offset, uint64 size, const void* data)
	{

		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_load_range_internal(internal_buffer, offset, size, data);

	}

	static bool32 vk_buffer_load_range_internal(VulkanBuffer* buffer, uint64 offset, uint64 size, const void* data)
	{

		if (!buffer_is_device_local(buffer) || buffer_is_host_visible(buffer))
		{

			void* data_ptr;
			VK_CHECK(vkMapMemory(context.device.logical_device, buffer->memory, offset, size, 0, &data_ptr));
			Memory::copy_memory(data, data_ptr, size);
			vkUnmapMemory(context.device.logical_device, buffer->memory);
			return true;

		}

		VulkanBuffer staging;
		if (!vk_buffer_create_internal(&staging, RenderbufferType::STAGING, size)) {
			SHMERROR("vk_buffer_read - Failed to create read buffer.");
			return false;
		}
		vk_buffer_bind_internal(&staging, 0);

		vk_buffer_load_range_internal(&staging, 0, size, data);
		vk_buffer_copy_range_internal(staging.handle, 0, buffer->handle, offset, size);

		vk_buffer_unbind_internal(&staging);
		vk_buffer_destroy_internal(&staging);

		return true;

	}

	bool32 vk_buffer_copy_range(Renderbuffer* source, uint64 source_offset, Renderbuffer* dest, uint64 dest_offset, uint64 size)
	{
		VulkanBuffer* source_v_buffer = (VulkanBuffer*)source->internal_data.data;
		VulkanBuffer* dest_v_buffer = (VulkanBuffer*)dest->internal_data.data;
		return vk_buffer_copy_range_internal(source_v_buffer->handle, source_offset, dest_v_buffer->handle, dest_offset, size);
	}

	bool32 vk_buffer_draw(Renderbuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only)
	{

		VulkanCommandBuffer& command_buffer = context.graphics_command_buffers[context.image_index];
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;

		if (buffer->type == RenderbufferType::VERTEX)
		{
			VkDeviceSize offsets[1] = { offset };
			vkCmdBindVertexBuffers(command_buffer.handle, 0, 1, &internal_buffer->handle, offsets);
			if (!bind_only)
				vkCmdDraw(command_buffer.handle, element_count, 1, 0, 0);
			return true;
		}

		if (buffer->type == RenderbufferType::INDEX)
		{
			vkCmdBindIndexBuffer(command_buffer.handle, internal_buffer->handle, offset, VK_INDEX_TYPE_UINT32);
			if (!bind_only)
				vkCmdDrawIndexed(command_buffer.handle, element_count, 1, 0, 0, 0);
			return true;
		}

		SHMERROR("vk_buffer_draw - Invalid buffer type for drawing!");
		return false;

	}

	static bool32 vk_buffer_copy_range_internal(VkBuffer source, uint64 source_offset, VkBuffer dest, uint64 dest_offset, uint64 size)
	{

		VkQueue queue = context.device.graphics_queue;
		vkQueueWaitIdle(queue);
		// Create a one-time-use command buffer.
		VulkanCommandBuffer temp_command_buffer;
		vulkan_command_buffer_reserve_and_begin_single_use(&context, context.device.graphics_command_pool, &temp_command_buffer);

		// Prepare the copy command and add it to the command buffer.
		VkBufferCopy copy_region;
		copy_region.srcOffset = source_offset;
		copy_region.dstOffset = dest_offset;
		copy_region.size = size;
		vkCmdCopyBuffer(temp_command_buffer.handle, source, dest, 1, &copy_region);

		// Submit the buffer for execution and wait for it to complete.
		vulkan_command_buffer_end_single_use(&context, context.device.graphics_command_pool, &temp_command_buffer, queue);

		return true;

	}

}

