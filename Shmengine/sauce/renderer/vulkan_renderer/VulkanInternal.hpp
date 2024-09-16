#pragma once

#include "VulkanTypes.hpp"

namespace Renderer::Vulkan
{
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
}


