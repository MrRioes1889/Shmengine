#include "VulkanFence.hpp"

#include "core/Logging.hpp"

void vulkan_fence_create(VulkanContext* context, bool32 create_signaled, VulkanFence* out_fence)
{

	out_fence->signaled = create_signaled;

	VkFenceCreateInfo create_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	if (out_fence->signaled)
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(context->device.logical_device, &create_info, context->allocator_callbacks, &out_fence->handle));

}

void vulkan_fence_destroy(VulkanContext* context, VulkanFence* fence)
{
	if (fence->handle)
		vkDestroyFence(context->device.logical_device, fence->handle, context->allocator_callbacks);
	fence->handle = 0;
	fence->signaled = false;
}

bool32 vulkan_fence_wait(VulkanContext* context, VulkanFence* fence, uint64 timeout_ns)
{
	if (!fence->signaled)
	{
		VkResult res = vkWaitForFences(context->device.logical_device, 1, &fence->handle, true, timeout_ns);

		switch (res)
		{
		case VK_SUCCESS:
			fence->signaled = true;
			return true;
		case VK_TIMEOUT:
			SHMWARN("vulkan_fence_wait - Timed out!");
			break;
		case VK_ERROR_DEVICE_LOST:
			SHMWARN("vulkan_fence_wait - VK_ERROR_DEVICE_LOST");
			break;
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			SHMWARN("vulkan_fence_wait - VK_ERROR_OUT_OF_HOST_MEMORY");
			break;
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			SHMWARN("vulkan_fence_wait - VK_ERROR_OUT_OF_DEVICE_MEMORY");
			break;
		default:
			SHMWARN("vulkan_fence_wait - An unknown error has occured.");
			break;
		}
	}
	else
		return true;

	return false;
}

void vulkan_fence_reset(VulkanContext* context, VulkanFence* fence)
{
	if (fence->signaled)
		VK_CHECK(vkResetFences(context->device.logical_device, 1, &fence->handle));
	fence->signaled = false;
}
