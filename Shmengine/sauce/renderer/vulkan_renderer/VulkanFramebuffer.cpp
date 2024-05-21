#include "VulkanFramebuffer.hpp"

void vulkan_framebuffer_create(
	VulkanContext* context, 
	VulkanRenderpass* renderpass, 
	uint32 attachment_count, 
	VkImageView* attachments, 
	VulkanFramebuffer* out_framebuffer)
{

	out_framebuffer->attachments.init(attachment_count);
	for (uint32 i = 0; i < attachment_count; i++)
		out_framebuffer->attachments[i] = attachments[i];
	out_framebuffer->renderpass = renderpass;

	VkFramebufferCreateInfo create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	create_info.renderPass = renderpass->handle;
	create_info.attachmentCount = attachment_count;
	create_info.pAttachments = out_framebuffer->attachments.data;
	create_info.width = context->framebuffer_width;
	create_info.height = context->framebuffer_height;
	create_info.layers = 1;

	VK_CHECK(vkCreateFramebuffer(context->device.logical_device, &create_info, context->allocator_callbacks, &out_framebuffer->handle));

}

void vulkan_framebuffer_destroy(VulkanContext* context, VulkanFramebuffer* framebuffer)
{

	vkDestroyFramebuffer(context->device.logical_device, framebuffer->handle, context->allocator_callbacks);
	framebuffer->attachments.free_data();

	framebuffer->handle = 0;
	framebuffer->renderpass = 0;

}
