#include "VulkanImage.hpp"

#include "VulkanDevice.hpp"
#include "core/Logging.hpp"



void vulkan_image_create(
	VulkanContext* context, 
	VkImageType image_type, 
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

	VkImageCreateInfo image_create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.extent.width = out_image->width;
	image_create_info.extent.height = out_image->height;
	image_create_info.extent.depth = 1;	// TODO: make configurable
	image_create_info.mipLevels = 4;	// TODO: make configurable
	image_create_info.arrayLayers = 1;	// TODO: make configurable
	image_create_info.format = format;	
	image_create_info.tiling = tiling;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_create_info.usage = usage;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;				// TODO: make configurable
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// TODO: make configurable

	VK_CHECK(vkCreateImage(context->device.logical_device, &image_create_info, context->allocator_callbacks, &out_image->handle));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(context->device.logical_device, out_image->handle, &memory_requirements);

	int32 memory_type = context->find_memory_index(memory_requirements.memoryTypeBits, memory_flags);
	if (memory_type < 0)
		SHMERROR("Required memory type not found. Image not valid");

	VkMemoryAllocateInfo memory_allocate_info = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memory_allocate_info.allocationSize = memory_requirements.size;
	memory_allocate_info.memoryTypeIndex = memory_type;
	VK_CHECK(vkAllocateMemory(context->device.logical_device, &memory_allocate_info, context->allocator_callbacks, &out_image->memory));
	VK_CHECK(vkBindImageMemory(context->device.logical_device, out_image->handle, out_image->memory, 0)); // TODO: Add configurable memory offset

	if (create_view)
	{
		out_image->view = 0;
		vulkan_image_view_create(context, format, out_image, view_aspect_flags);
	}
}

void vulkan_image_view_create(VulkanContext* context, VkFormat format, VulkanImage* image, VkImageAspectFlags aspect_flags)
{

	VkImageViewCreateInfo view_create_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_create_info.image = image->handle;
	view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_create_info.format = format;
	view_create_info.subresourceRange.aspectMask = aspect_flags;

	// TODO: make configurable
	view_create_info.subresourceRange.baseMipLevel = 0;
	view_create_info.subresourceRange.levelCount = 1;
	view_create_info.subresourceRange.baseArrayLayer = 0;
	view_create_info.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(context->device.logical_device, &view_create_info, context->allocator_callbacks, &image->view));

}

void vulkan_image_destroy(VulkanContext* context, VulkanImage* image)
{

	if (image->view)
		vkDestroyImageView(context->device.logical_device, image->view, context->allocator_callbacks);

	if (image->memory)
		vkFreeMemory(context->device.logical_device, image->memory, context->allocator_callbacks);

	if (image->handle)
		vkDestroyImage(context->device.logical_device, image->handle, context->allocator_callbacks);

	image->view = 0;
	image->memory = 0;
	image->handle = 0;

}
