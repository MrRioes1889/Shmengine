#pragma once

#include "VulkanTypes.hpp"

bool32 vulkan_buffer_create(
	VulkanContext* context,
	uint64 size,
	VkBufferUsageFlagBits usage,
	uint32 memory_property_flags,
	bool32 bind_on_create,
	VulkanBuffer* out_buffer);
void vulkan_buffer_destroy(VulkanContext* context, VulkanBuffer* buffer);
void vulkan_buffer_resize(VulkanContext* context, uint64 new_size, VulkanBuffer* buffer, VkQueue queue, VkCommandPool pool);

void vulkan_buffer_bind(VulkanContext* context, VulkanBuffer* buffer, uint64 offset);

void* vulkan_buffer_lock_memory(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags);
void vulkan_buffer_unlock_memory(VulkanContext* context, VulkanBuffer* buffer);

void vulkan_buffer_load_data(VulkanContext* context, VulkanBuffer* buffer, uint64 offset, uint64 size, uint32 flags, const void* data);

void vulkan_buffer_copy_to(
	VulkanContext* context,
	VkCommandPool pool,
	VkFence fence,
	VkQueue queue,
	VkBuffer source,
	uint64 source_offset,
	VkBuffer dest,
	uint64 dest_offset,
	uint64 size);