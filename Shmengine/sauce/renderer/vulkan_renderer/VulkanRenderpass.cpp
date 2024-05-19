#include "VulkanRenderpass.hpp"

void vulkan_renderpass_create(VulkanContext* context, VulkanRenderpass* out_renderpass, Math::Vec2i offset, Math::Vec2ui dim, Math::Vec4f clear_color, float32 depth, uint32 stencil)
{

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	// TODO: Make configurable
	const uint32 attachment_description_count = 2;
	VkAttachmentDescription attachment_descriptions[attachment_description_count];

	VkAttachmentDescription color_att = {};
	color_att.format = context->swapchain.image_format.format; // TODO: Make configurable
	color_att.samples = VK_SAMPLE_COUNT_1_BIT;
	color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	color_att.flags = 0;

	attachment_descriptions[0] = color_att;

	VkAttachmentReference color_att_ref = {};
	color_att_ref.attachment = 0;
	color_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_att_ref;

	VkAttachmentDescription depth_att = {};
	depth_att.format = context->device.depth_format; // TODO: Make configurable
	depth_att.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_att.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_att.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_att.flags = 0;

	attachment_descriptions[1] = depth_att;

	VkAttachmentReference depth_att_ref = {};
	depth_att_ref.attachment = 1;
	depth_att_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpass.pDepthStencilAttachment = &depth_att_ref;

	// TODO: Add other attachment types (input, resolve, preserve)
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = 0;

	subpass.pResolveAttachments = 0;

	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = 0;

	VkSubpassDependency dependency;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dependencyFlags = 0;
	
	VkRenderPassCreateInfo create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	create_info.attachmentCount = attachment_description_count;
	create_info.pAttachments = attachment_descriptions;
	create_info.subpassCount = 1;
	create_info.pSubpasses = &subpass;
	create_info.dependencyCount = 1;
	create_info.pDependencies = &dependency;
	create_info.pNext = 0;
	create_info.flags = 0;

	VK_CHECK(vkCreateRenderPass(context->device.logical_device, &create_info, context->allocator_callbacks, &out_renderpass->handle));

}

void vulkan_renderpass_destroy(VulkanContext* context, VulkanRenderpass* renderpass)
{
	if (renderpass && renderpass->handle)
	{
		vkDestroyRenderPass(context->device.logical_device, renderpass->handle, context->allocator_callbacks);
		renderpass->handle = 0;
	}
}

void vulkan_renderpass_begin(VulkanCommandBuffer* command_buffer, VulkanRenderpass* renderpass, VkFramebuffer framebuffer)
{
	VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	begin_info.renderPass = renderpass->handle;
	begin_info.framebuffer = framebuffer;
	begin_info.renderArea.offset.x = renderpass->offset.x;
	begin_info.renderArea.offset.y = renderpass->offset.y;
	begin_info.renderArea.extent.width = renderpass->dim.width;
	begin_info.renderArea.extent.height = renderpass->dim.height;

	const uint32 clear_value_count = 2;
	VkClearValue clear_values[clear_value_count] = {};
	clear_values[0].color.float32[0] = renderpass->clear_color.r;
	clear_values[0].color.float32[1] = renderpass->clear_color.g;
	clear_values[0].color.float32[2] = renderpass->clear_color.b;
	clear_values[0].color.float32[3] = renderpass->clear_color.a;

	clear_values[1].depthStencil.depth = renderpass->depth;
	clear_values[1].depthStencil.stencil = renderpass->stencil;

	begin_info.clearValueCount = clear_value_count;
	begin_info.pClearValues = clear_values;

	vkCmdBeginRenderPass(command_buffer->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
	command_buffer->state = VulkanCommandBufferState::IN_RENDER_PASS;
}

void vulkan_renderpass_end(VulkanCommandBuffer* command_buffer, VulkanRenderpass* renderpass)
{
	vkCmdEndRenderPass(command_buffer->handle);
	command_buffer->state = VulkanCommandBufferState::RECORDING;
}
