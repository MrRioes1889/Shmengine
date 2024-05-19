#pragma once

#include "VulkanTypes.hpp"

void vulkan_fence_create(VulkanContext* context, bool32 create_signaled, VulkanFence* out_fence);
void vulkan_fence_destroy(VulkanContext* context, VulkanFence* fence);

bool32 vulkan_fence_wait(VulkanContext* context, VulkanFence* fence, uint64 timeout_ns);

void vulkan_fence_reset(VulkanContext* context, VulkanFence* fence);