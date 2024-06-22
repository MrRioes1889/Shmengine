#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{
	bool32 buffer_create(
		VulkanContext* context,
		uint64 size,
		VkBufferUsageFlagBits usage,
		uint32 memory_property_flags,
		bool32 bind_on_create,
		bool32 use_freelist,
		VulkanBuffer* out_buffer);
	void buffer_destroy(VulkanContext* context, VulkanBuffer* buffer);
	void buffer_resize(VulkanContext* context, uint64 new_size, VulkanBuffer* buffer, VkQueue queue, VkCommandPool pool);

	bool32 buffer_allocate(VulkanBuffer* buffer, uint64 size, uint64* out_offset);
	bool32 buffer_free(VulkanBuffer* buffer, uint64 offset);

	void buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64 offset);

	void* buffer_lock_memory(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags);
	void buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer);

	void buffer_load_data(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags, const void* data);

	void buffer_copy_to(
		VulkanContext* context,
		VkCommandPool pool,
		VkFence fence,
		VkQueue queue,
		VkBuffer source,
		uint64 source_offset,
		VkBuffer dest,
		uint64 dest_offset,
		uint64 size);
}

