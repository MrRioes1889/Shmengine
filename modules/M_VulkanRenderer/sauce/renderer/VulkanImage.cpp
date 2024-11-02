#include "VulkanInternal.hpp"

#include <core/Logging.hpp>


namespace Renderer::Vulkan
{
	extern VulkanContext context;

	void vk_image_create(
		TextureType type,
		uint32 width,
		uint32 height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags memory_flags,
		bool32 create_view,
		VkImageAspectFlags view_aspect_flags,
		VulkanImage* out_image)
	{

		out_image->width = width;
		out_image->height = height;
		out_image->memory_flags = memory_flags;

		VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		switch (type)
		{
		default:
		case TextureType::TYPE_CUBE:
		case TextureType::TYPE_2D:
		{
			image_create_info.imageType = VK_IMAGE_TYPE_2D;
			break;
		}
		}
		image_create_info.extent.width = out_image->width;
		image_create_info.extent.height = out_image->height;
		image_create_info.extent.depth = 1;	// TODO: make configurable
		image_create_info.mipLevels = 4;	// TODO: make configurable
		image_create_info.arrayLayers = type == TextureType::TYPE_CUBE ? 6 : 1;	// TODO: make configurable
		image_create_info.format = format;
		image_create_info.tiling = tiling;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_create_info.usage = usage;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;				// TODO: make configurable
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// TODO: make configurable
		if (type == TextureType::TYPE_CUBE)
			image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		VK_CHECK(vkCreateImage(context.device.logical_device, &image_create_info, context.allocator_callbacks, &out_image->handle));

		vkGetImageMemoryRequirements(context.device.logical_device, out_image->handle, &out_image->memory_requirements);

		int32 memory_type = context.find_memory_index(out_image->memory_requirements.memoryTypeBits, memory_flags);
		if (memory_type < 0)
			SHMERROR("Required memory type not found. Image not valid");

		VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		memory_allocate_info.allocationSize = out_image->memory_requirements.size;
		memory_allocate_info.memoryTypeIndex = memory_type;
		VK_CHECK(vkAllocateMemory(context.device.logical_device, &memory_allocate_info, context.allocator_callbacks, &out_image->memory));
		VK_CHECK(vkBindImageMemory(context.device.logical_device, out_image->handle, out_image->memory, 0)); // TODO: Add configurable memory offset

		bool32 is_device_memory = (out_image->memory_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Memory::track_external_allocation(out_image->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);

		if (create_view)
		{
			out_image->view = 0;
			vk_image_view_create(type, format, out_image, view_aspect_flags);
		}
	}



	void vk_image_view_create(TextureType type, VkFormat format, VulkanImage* image, VkImageAspectFlags aspect_flags)
	{

		VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_create_info.image = image->handle;
		switch (type)
		{
		case TextureType::TYPE_CUBE:
		{
			view_create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			break;
		}
		default:
		case TextureType::TYPE_2D:
		{
			view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			break;
		}
		}
		view_create_info.format = format;
		view_create_info.subresourceRange.aspectMask = aspect_flags;

		// TODO: make configurable
		view_create_info.subresourceRange.baseMipLevel = 0;
		view_create_info.subresourceRange.levelCount = 1;
		view_create_info.subresourceRange.baseArrayLayer = 0;
		view_create_info.subresourceRange.layerCount = type == TextureType::TYPE_CUBE ? 6 : 1;

		VK_CHECK(vkCreateImageView(context.device.logical_device, &view_create_info, context.allocator_callbacks, &image->view));

	}

	void vk_image_transition_layout(TextureType type, VulkanCommandBuffer* command_buffer, VulkanImage* image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
	{

		VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.oldLayout = old_layout;
		barrier.newLayout = new_layout;
		barrier.srcQueueFamilyIndex = context.device.graphics_queue_index;
		barrier.dstQueueFamilyIndex = context.device.graphics_queue_index;
		barrier.image = image->handle;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = type == TextureType::TYPE_CUBE ? 6 : 1;

		VkPipelineStageFlags source_stage;
		VkPipelineStageFlags dest_stage;

		if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else
		{
			SHMFATAL("Unsupported layout transition!");
			return;
		}

		vkCmdPipelineBarrier(command_buffer->handle, source_stage, dest_stage, 0, 0, 0, 0, 0, 1, &barrier);

	}

	void vk_image_copy_from_buffer(TextureType type, VulkanImage* image, VkBuffer buffer, VulkanCommandBuffer* command_buffer)
	{

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = type == TextureType::TYPE_CUBE ? 6 : 1;

		region.imageExtent.width = image->width;
		region.imageExtent.height = image->height;
		region.imageExtent.depth = 1;

		vkCmdCopyBufferToImage(command_buffer->handle, buffer, image->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	}

	void vk_image_destroy(VulkanImage* image)
	{

		if (image->view)
			vkDestroyImageView(context.device.logical_device, image->view, context.allocator_callbacks);

		if (image->memory)
			vkFreeMemory(context.device.logical_device, image->memory, context.allocator_callbacks);

		if (image->handle)
			vkDestroyImage(context.device.logical_device, image->handle, context.allocator_callbacks);

		bool32 is_device_memory = (image->memory_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Memory::track_external_free(image->memory_requirements.size, is_device_memory ? AllocationTag::GPU_LOCAL : AllocationTag::VULKAN);

		image->view = 0;
		image->memory = 0;
		image->handle = 0;

	}

	void vk_image_copy_to_buffer(TextureType type, VulkanImage* image, VkBuffer buffer, VulkanCommandBuffer* command_buffer)
	{
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = type == TextureType::TYPE_CUBE ? 6 : 1;

		region.imageExtent.width = image->width;
		region.imageExtent.height = image->height;
		region.imageExtent.depth = 1;

		vkCmdCopyImageToBuffer(command_buffer->handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
	}

	void vk_image_copy_pixel_to_buffer(TextureType type, VulkanImage* image, VkBuffer buffer, uint32 x, uint32 y, VulkanCommandBuffer* command_buffer)
	{
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = type == TextureType::TYPE_CUBE ? 6 : 1;

		region.imageOffset.x = x;
		region.imageOffset.y = y;
		region.imageExtent.width = 1;
		region.imageExtent.height = 1;
		region.imageExtent.depth = 1;

		vkCmdCopyImageToBuffer(command_buffer->handle, image->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
	}

}
