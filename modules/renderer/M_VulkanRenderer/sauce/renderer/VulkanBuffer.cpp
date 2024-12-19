#include "VulkanBackend.hpp"
#include "VulkanInternal.hpp"

#include <core/Memory.hpp>
#include <core/Logging.hpp>
#include <core/Clock.hpp>
		 
#include <optick.h>

namespace Renderer::Vulkan
{

	extern VulkanContext* context;

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

	bool32 vk_buffer_create(RenderBuffer* buffer)
	{
		buffer->internal_data.init(sizeof(VulkanBuffer), 0, AllocationTag::VULKAN);
		if (!vk_buffer_create_internal((VulkanBuffer*)buffer->internal_data.data, buffer->type, buffer->size, buffer->name.c_str()))
		{
			buffer->internal_data.free_data();
			return false;
		}

		return true;
	}

	bool32 vk_buffer_create_internal(VulkanBuffer* buffer, RenderBufferType type, uint64 size, const char* name)
	{

		switch (type) {
		case RenderBufferType::VERTEX:
		{
			buffer->usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			//buffer->memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; //| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}		
		case RenderBufferType::INDEX:
		{
			buffer->usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			//buffer->memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; //| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			buffer->memory_property_flags =  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}		
		case RenderBufferType::UNIFORM:
		{
			uint32 device_local_bits = context->device.supports_device_local_host_visible ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
			buffer->usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | device_local_bits;
			break;
		}	
		case RenderBufferType::STAGING:
		{
			buffer->usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}
		case RenderBufferType::READ:
		{
			buffer->usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			buffer->memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		}
		case RenderBufferType::STORAGE:
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

		buffer->mapped_memory = 0;
		buffer->is_locked = false;

		VkBufferCreateInfo buffer_create_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		buffer_create_info.size = size;
		buffer_create_info.usage = buffer->usage;
		buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // NOTE: Only used in one queue

		VK_CHECK(vkCreateBuffer(context->device.logical_device, &buffer_create_info, context->allocator_callbacks, &buffer->handle));

		vkGetBufferMemoryRequirements(context->device.logical_device, buffer->handle, &buffer->memory_requirements);
		buffer->memory_index = context->find_memory_index(buffer->memory_requirements.memoryTypeBits, buffer->memory_property_flags);
		if (buffer->memory_index < 0)
		{
			SHMERROR("Unable to create vulkan buffer because the required memory type index was not found");		
			return false;
		}

		VkMemoryAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocate_info.allocationSize = buffer->memory_requirements.size;
		allocate_info.memoryTypeIndex = (uint32)buffer->memory_index;

		VkResult res = vkAllocateMemory(context->device.logical_device, &allocate_info, context->allocator_callbacks, &buffer->memory);
		if (res != VK_SUCCESS)
		{
			SHMERROR("Unable to create vulkan buffer. Failed to allocate memory.");
			return false;
		}
		VK_DEBUG_SET_OBJECT_NAME(context, VK_OBJECT_TYPE_DEVICE_MEMORY, buffer->memory, name);

		bool32 is_device_memory = buffer_is_device_local(buffer);
		Memory::track_external_allocation(buffer->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);

		if (!buffer_is_device_local(buffer) || buffer_is_host_visible(buffer))
			vk_buffer_map_memory_internal(buffer, 0, size);

		return true;

	}

	void vk_buffer_destroy(RenderBuffer* buffer)
	{
		if (!buffer->internal_data.data)
			return;

		VulkanBuffer* v_buffer = (VulkanBuffer*)buffer->internal_data.data;
		vk_buffer_destroy_internal(v_buffer);
		buffer->internal_data.free_data();
	}

	void vk_buffer_destroy_internal(VulkanBuffer* buffer)
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

		bool32 is_device_memory = (buffer->memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Memory::track_external_free(buffer->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);

		buffer->usage = 0;
		buffer->is_locked = false;

	}

	bool32 vk_buffer_resize(RenderBuffer* buffer, uint64 new_size)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_resize_internal(internal_buffer, buffer->size, new_size, buffer->name.c_str());
	}

	bool32 vk_buffer_resize_internal(VulkanBuffer* buffer, uint64 old_size, uint64 new_size, const char* name)
	{

		// Create new buffer.
		VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		buffer_info.size = new_size;
		buffer_info.usage = buffer->usage;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // NOTE: Only used in one queue.

		VkBuffer new_buffer;
		VK_CHECK(vkCreateBuffer(context->device.logical_device, &buffer_info, context->allocator_callbacks, &new_buffer));

		// Gather memory requirements.
		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(context->device.logical_device, new_buffer, &requirements);

		// Allocate memory info
		VkMemoryAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocate_info.allocationSize = requirements.size;
		allocate_info.memoryTypeIndex = (uint32)buffer->memory_index;

		// Allocate the memory.
		VkDeviceMemory new_memory;
		VkResult result = vkAllocateMemory(context->device.logical_device, &allocate_info, context->allocator_callbacks, &new_memory);
		if (result != VK_SUCCESS) {
			SHMERRORV("Unable to resize vulkan buffer because the required memory allocation failed. Error: %u", result);
			return false;
		}
		
		// Bind the new buffer's memory
		VK_CHECK(vkBindBufferMemory(context->device.logical_device, new_buffer, new_memory, 0));

		// Copy over the data.
		vk_buffer_copy_range_internal(buffer->handle, 0, new_buffer, 0, old_size);

		// Make sure anything potentially using these is finished.
		// NOTE: We could use vkQueueWaitIdle here if we knew what queue this buffer would be used with...
		vkDeviceWaitIdle(context->device.logical_device);
	
		// Destroy the old
		vk_buffer_unmap_memory_internal(buffer);
		if (buffer->memory) {
			vkFreeMemory(context->device.logical_device, buffer->memory, context->allocator_callbacks);
			buffer->memory = 0;
		}
		if (buffer->handle) {
			vkDestroyBuffer(context->device.logical_device, buffer->handle, context->allocator_callbacks);
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
		VK_DEBUG_SET_OBJECT_NAME(context, VK_OBJECT_TYPE_DEVICE_MEMORY, buffer->memory, name);
		vk_buffer_map_memory_internal(buffer, 0, new_size);

		return true;

	}

	bool32 vk_buffer_bind(RenderBuffer* buffer, uint64 offset)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_bind_internal(internal_buffer, offset);
	}

	bool32 vk_buffer_bind_internal(VulkanBuffer* buffer, uint64 offset)
	{
		VK_CHECK(vkBindBufferMemory(context->device.logical_device, buffer->handle, buffer->memory, offset));
		return true;
	}

	bool32 vk_buffer_unbind(RenderBuffer* buffer)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_unbind_internal(internal_buffer);
	}

	bool32 vk_buffer_unbind_internal(VulkanBuffer* buffer)
	{
		return true;
	}

	void* vk_buffer_map_memory(RenderBuffer* buffer, uint64 offset, uint64 size)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_map_memory_internal(internal_buffer, offset, size);
	}

	void* vk_buffer_map_memory_internal(VulkanBuffer* buffer, uint64 offset, uint64 size)
	{
		if (buffer->mapped_memory)
			return buffer->mapped_memory;

		VK_CHECK(vkMapMemory(context->device.logical_device, buffer->memory, offset, size, 0, &buffer->mapped_memory));
		return buffer->mapped_memory;
	}

	void vk_buffer_unmap_memory(RenderBuffer* buffer)
	{		
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		vk_buffer_unmap_memory_internal(internal_buffer);
	}

	void vk_buffer_unmap_memory_internal(VulkanBuffer* buffer)
	{
		if (!buffer->mapped_memory)
			return;

		vkUnmapMemory(context->device.logical_device, buffer->memory);
		buffer->mapped_memory = 0;
	}

	bool32 vk_buffer_flush(RenderBuffer* buffer, uint64 offset, uint64 size)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		if (!buffer_is_host_coherent(internal_buffer))
			return false;

		VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		range.memory = internal_buffer->memory;
		range.offset = offset;
		range.size = size;
		VK_CHECK(vkFlushMappedMemoryRanges(context->device.logical_device, 1, &range));

		return true;
	}

	bool32 vk_buffer_read(RenderBuffer* buffer, uint64 offset, uint64 size, void* out_memory)
	{
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_read_internal(internal_buffer, offset, size, out_memory);
	}

	bool32 vk_buffer_read_internal(VulkanBuffer* buffer, uint64 offset, uint64 size, void* out_memory)
	{
	
		if (!buffer_is_device_local(buffer) || buffer_is_host_visible(buffer))
		{

			uint8* ptr = (uint8*)buffer->mapped_memory + offset;
			Memory::copy_memory(ptr, out_memory, size);
			return true;
			
		}

		VulkanBuffer read;
		if (!vk_buffer_create_internal(&read, RenderBufferType::READ, size, "temp_read_buffer")) {
			SHMERROR("vk_buffer_read - Failed to create read buffer.");
			return false;
		}
		vk_buffer_bind_internal(&read, 0);

		vk_buffer_copy_range_internal(buffer->handle, offset, read.handle, 0, size);

		// Map/copy/unmap
		void* mapped_data;
		VK_CHECK(vkMapMemory(context->device.logical_device, read.memory, 0, size, 0, &mapped_data));
		Memory::copy_memory(mapped_data, out_memory, size);
		vkUnmapMemory(context->device.logical_device, read.memory);

		// Clean up the read buffer.
		vk_buffer_unbind_internal(&read);
		vk_buffer_destroy_internal(&read);

		return true;

	}

	bool32 vk_buffer_load_range(RenderBuffer* buffer, uint64 offset, uint64 size, const void* data)
	{

		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;
		return vk_buffer_load_range_internal(internal_buffer, offset, size, data);

	}

	// TODO: Overhaul for performance necessary. Creating/destruction of staging buffer and copy function seem to be major bottlenecks!
	bool32 vk_buffer_load_range_internal(VulkanBuffer* buffer, uint64 offset, uint64 size, const void* data)
	{

		OPTICK_EVENT();
		
		if (!buffer_is_device_local(buffer) || buffer_is_host_visible(buffer))
		{					
			uint8* ptr = (uint8*)buffer->mapped_memory + offset;
			Memory::copy_memory(data, ptr, size);
			return true;
		}
		
		VulkanBuffer staging;		
		if (!vk_buffer_create_internal(&staging, RenderBufferType::STAGING, size, "load_range_staging_buffer")) {
			SHMERROR("Failed to create staging buffer.");
			return false;
		}		
		vk_buffer_bind_internal(&staging, 0);

		vk_buffer_load_range_internal(&staging, 0, size, data);
		vk_buffer_copy_range_internal(staging.handle, 0, buffer->handle, offset, size);		

		vk_buffer_unbind_internal(&staging);
		vk_buffer_destroy_internal(&staging);			

		return true;

	}

	bool32 vk_buffer_copy_range(RenderBuffer* source, uint64 source_offset, RenderBuffer* dest, uint64 dest_offset, uint64 size)
	{
		VulkanBuffer* source_v_buffer = (VulkanBuffer*)source->internal_data.data;
		VulkanBuffer* dest_v_buffer = (VulkanBuffer*)dest->internal_data.data;
		return vk_buffer_copy_range_internal(source_v_buffer->handle, source_offset, dest_v_buffer->handle, dest_offset, size);
	}

	bool32 vk_buffer_draw(RenderBuffer* buffer, uint64 offset, uint32 element_count, bool32 bind_only)
	{

		VulkanCommandBuffer& command_buffer = context->graphics_command_buffers[context->image_index];
		VulkanBuffer* internal_buffer = (VulkanBuffer*)buffer->internal_data.data;

		if (buffer->type == RenderBufferType::VERTEX)
		{
			VkDeviceSize offsets[1] = { offset };
			vkCmdBindVertexBuffers(command_buffer.handle, 0, 1, &internal_buffer->handle, offsets);
			if (!bind_only)
				vkCmdDraw(command_buffer.handle, element_count, 1, 0, 0);
			return true;
		}

		if (buffer->type == RenderBufferType::INDEX)
		{
			vkCmdBindIndexBuffer(command_buffer.handle, internal_buffer->handle, offset, VK_INDEX_TYPE_UINT32);
			if (!bind_only)
				vkCmdDrawIndexed(command_buffer.handle, element_count, 1, 0, 0, 0);
			return true;
		}

		SHMERROR("vk_buffer_draw - Invalid buffer type for drawing!");
		return false;

	}

	bool32 vk_buffer_copy_range_internal(VkBuffer source, uint64 source_offset, VkBuffer dest, uint64 dest_offset, uint64 size)
	{

		VkQueue queue = context->device.graphics_queue;
		vkQueueWaitIdle(queue);
		// Create a one-time-use command buffer.
		VulkanCommandBuffer temp_command_buffer;
		vk_command_buffer_reserve_and_begin_single_use(context->device.graphics_command_pool, &temp_command_buffer);

		// Prepare the copy command and add it to the command buffer.
		VkBufferCopy copy_region;
		copy_region.srcOffset = source_offset;
		copy_region.dstOffset = dest_offset;
		copy_region.size = size;
		vkCmdCopyBuffer(temp_command_buffer.handle, source, dest, 1, &copy_region);

		// Submit the buffer for execution and wait for it to complete.
		vk_command_buffer_end_single_use(context->device.graphics_command_pool, &temp_command_buffer, queue);

		return true;

	}

}

