#include "VulkanSwapchain.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "VulkanDevice.hpp"
#include "VulkanImage.hpp"

#include "utility/Utility.hpp"

#include "systems/TextureSystem.hpp"

static bool32 create(VulkanContext* context, uint32 width, uint32 height, VulkanSwapchain* out_swapchain)
{

	VkExtent2D swapchain_extent = { width, height };

	bool32 found = false;
	for (uint32 i = 0; i < context->device.swapchain_support.format_count; i++)
	{
		VkSurfaceFormatKHR format = context->device.swapchain_support.formats[i];
		// NOTE: Choosing preferred formats
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			out_swapchain->image_format = format;
			found = true;
			break;
		}
	}

	if (!found)
		out_swapchain->image_format = context->device.swapchain_support.formats[0];

	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	for (uint32 i = 0; i < context->device.swapchain_support.present_mode_count; i++)
	{
		VkPresentModeKHR mode = context->device.swapchain_support.present_modes[i];
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			present_mode = mode;
			break;
		}
	}

	vulkan_device_query_swapchain_support(context->device.physical_device, context->surface, &context->device.swapchain_support);

	if (context->device.swapchain_support.capabilities.currentExtent.width != UINT32_MAX)
		swapchain_extent = context->device.swapchain_support.capabilities.currentExtent;

	VkExtent2D min = context->device.swapchain_support.capabilities.minImageExtent;
	VkExtent2D max = context->device.swapchain_support.capabilities.maxImageExtent;
	swapchain_extent.width = clamp(swapchain_extent.width, min.width, max.width);
	swapchain_extent.height = clamp(swapchain_extent.height, min.height, max.height);

	uint32 image_count = context->device.swapchain_support.capabilities.minImageCount + 1;
	if (context->device.swapchain_support.capabilities.maxImageCount > 0 && image_count > context->device.swapchain_support.capabilities.maxImageCount)
	{
		image_count = context->device.swapchain_support.capabilities.maxImageCount;
	}
	out_swapchain->max_frames_in_flight = image_count - 1;

	VkSwapchainCreateInfoKHR swapchain_create_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchain_create_info.surface = context->surface;
	swapchain_create_info.minImageCount = image_count;
	swapchain_create_info.imageFormat = out_swapchain->image_format.format;
	swapchain_create_info.imageColorSpace = out_swapchain->image_format.colorSpace;
	swapchain_create_info.imageExtent = swapchain_extent;
	swapchain_create_info.imageArrayLayers = 1;
	swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	static const uint32 queue_family_indices[] = { (uint32)context->device.graphics_queue_index, (uint32)context->device.present_queue_index };
	if (context->device.graphics_queue_index != context->device.present_queue_index)
	{	
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_create_info.queueFamilyIndexCount = 2;
		swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
	{
		swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_create_info.queueFamilyIndexCount = 0;
		swapchain_create_info.pQueueFamilyIndices = 0;
	}

	swapchain_create_info.preTransform = context->device.swapchain_support.capabilities.currentTransform;
	swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_create_info.presentMode = present_mode;
	swapchain_create_info.clipped = VK_TRUE;
	// TODO: Pass old swapchain when recreating it. Explicit destruction and recreation for now.
	swapchain_create_info.oldSwapchain = 0;

	VK_CHECK(vkCreateSwapchainKHR(context->device.logical_device, &swapchain_create_info, context->allocator_callbacks, &out_swapchain->handle));

	context->current_frame = 0;
	
	image_count = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device, out_swapchain->handle, &image_count, 0));
	if (!out_swapchain->render_images.data)
	{
		out_swapchain->render_images.init(image_count, 0, AllocationTag::MAIN);

		for (uint32 i = 0; i < out_swapchain->render_images.count; i++)
		{
			uint64 allocation_size = sizeof(VulkanImage);
			void* internal_data = Memory::allocate(allocation_size, 0, AllocationTag::MAIN);

			char tex_name[] = "__internal_vulkan_swapchain_image_0__";
			tex_name[34] = (char)('0' + i);

			out_swapchain->render_images[i] =
				TextureSystem::wrap_internal(tex_name, swapchain_extent.width, swapchain_extent.height, 4, false, true, false, internal_data, allocation_size);
			if (!out_swapchain->render_images[i])
			{
				SHMFATAL("Failed to generate new swapchain image texture!");
				return false;
			}

		}
	}
	else
	{
		for (uint32 i = 0; i < out_swapchain->render_images.count; i++)
		{
			TextureSystem::resize(out_swapchain->render_images[i], swapchain_extent.width, swapchain_extent.height, false);
		}
	}
		
	VkImage swapchain_images[32];
	VK_CHECK(vkGetSwapchainImagesKHR(context->device.logical_device, out_swapchain->handle, &image_count, swapchain_images));
	for (uint32 i = 0; i < out_swapchain->render_images.count; i++)
	{
		VulkanImage* image = (VulkanImage*)out_swapchain->render_images[i]->internal_data.data;
		image->handle = swapchain_images[i];
		image->width = swapchain_extent.width;
		image->height = swapchain_extent.height;
	}


	for (uint32 i = 0; i < out_swapchain->render_images.count; i++)
	{
		VulkanImage* image = (VulkanImage*)out_swapchain->render_images[i]->internal_data.data;

		VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_info.image = image->handle;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = out_swapchain->image_format.format;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(context->device.logical_device, &view_info, context->allocator_callbacks, &image->view));
	}

	if (!vulkan_device_detect_depth_format(&context->device))
		SHMFATAL("Failed to find a supported depth buffer format!");

	VulkanImage* depth_image = (VulkanImage*)Memory::allocate(sizeof(VulkanImage), true, AllocationTag::MAIN);
	vulkan_image_create(
		context,
		TextureType::TYPE_2D,
		swapchain_extent.width,
		swapchain_extent.height,
		context->device.depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		true,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		depth_image);

	context->swapchain.depth_texture =
		TextureSystem::wrap_internal(
			"default_depth_texture",
			swapchain_extent.width,
			swapchain_extent.height,
			context->device.depth_channel_count,
			false,
			true,
			false,
			depth_image,
			sizeof(VulkanImage)
		);

	SHMINFO("Swapchain created successfully!");

	return true;

}

static void destroy(VulkanContext* context, VulkanSwapchain* swapchain)
{
	vkDeviceWaitIdle(context->device.logical_device);

	VulkanImage* depth_image = (VulkanImage*)swapchain->depth_texture->internal_data.data;
	vulkan_image_destroy(context, depth_image);
	Memory::free_memory(depth_image, true, AllocationTag::MAIN);
	Memory::free_memory(swapchain->depth_texture, true, AllocationTag::MAIN);

	for (uint32 i = 0; i < swapchain->render_images.count; i++)
	{
		VulkanImage* image = (VulkanImage*)swapchain->render_images[i]->internal_data.data;
		vkDestroyImageView(context->device.logical_device, image->view, context->allocator_callbacks);
	}

	// NOTE: Image data gets destroyed within this call, therefore does not have to be destroyed seperately
	vkDestroySwapchainKHR(context->device.logical_device, swapchain->handle, context->allocator_callbacks);
}

bool32 vulkan_swapchain_create(VulkanContext* context, uint32 width, uint32 height, VulkanSwapchain* out_swapchain)
{
	return create(context, width, height, out_swapchain);
}

bool32 vulkan_swapchain_recreate(VulkanContext* context, uint32 width, uint32 height, VulkanSwapchain* swapchain)
{
	destroy(context, swapchain);
	return create(context, width, height, swapchain);
}

void vulkan_swapchain_destroy(VulkanContext* context, VulkanSwapchain* swapchain)
{
	destroy(context, swapchain);
}

bool32 vulkan_swapchain_acquire_next_image_index(VulkanContext* context, VulkanSwapchain* swapchain, uint64 timeout_ns, VkSemaphore image_available_semaphore, VkFence fence, uint32* out_image_index)
{
	
	VkResult result = vkAcquireNextImageKHR(context->device.logical_device, swapchain->handle, timeout_ns, image_available_semaphore, fence, out_image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		vulkan_swapchain_recreate(context, context->framebuffer_width, context->framebuffer_height, swapchain);
		return false;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		SHMFATAL("Failed to acquire swapchain image!");
		return false;
	}

	return true;

}

void vulkan_swapchain_present(VulkanContext* context, VulkanSwapchain* swapchain, VkQueue graphics_queue, VkQueue present_queue, VkSemaphore render_complete_semaphore, uint32 present_image_index)
{

	VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render_complete_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain->handle;
	present_info.pImageIndices = &present_image_index;
	present_info.pResults = 0;

	VkResult result = vkQueuePresentKHR(present_queue, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		vulkan_swapchain_recreate(context, context->framebuffer_width, context->framebuffer_height, swapchain);
	else if (result != VK_SUCCESS)
		SHMFATAL("Failed to present swap chain image!");

	context->current_frame = (context->current_frame + 1) % swapchain->max_frames_in_flight;
}


