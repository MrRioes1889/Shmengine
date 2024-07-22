#include "VulkanBackend.hpp"
#include "VulkanCommon.hpp"

static VulkanContext* context = 0;

namespace Renderer::Vulkan
{

	void renderpass_init_context(VulkanContext* c)
	{
		context = c;
	}

	void renderpass_create(Renderpass* out_renderpass, float32 depth, uint32 stencil, bool32 has_prev_pass, bool32 has_next_pass)
	{

		out_renderpass->internal_data.init(sizeof(VulkanRenderpass), 0, AllocationTag::MAIN);
		VulkanRenderpass* v_renderpass = (VulkanRenderpass*)out_renderpass->internal_data.data;

		v_renderpass->depth = depth;
		v_renderpass->stencil = stencil;
		v_renderpass->has_prev_pass = has_prev_pass;
		v_renderpass->has_next_pass = has_next_pass;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		// TODO: Make configurable
		uint32 attachment_description_count = 0;
		VkAttachmentDescription attachment_descriptions[2];

		bool32 do_clear_color = (out_renderpass->clear_flags & RenderpassClearFlags::COLOR_BUFFER);
		VkAttachmentDescription color_att = {};
		color_att.format = context->swapchain.image_format.format; // TODO: Make configurable
		color_att.samples = VK_SAMPLE_COUNT_1_BIT;
		color_att.loadOp = do_clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_att.initialLayout = has_prev_pass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
		color_att.finalLayout = has_next_pass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		color_att.flags = 0;

		attachment_descriptions[attachment_description_count++] = color_att;

		VkAttachmentReference color_att_ref = {};
		color_att_ref.attachment = 0;
		color_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_att_ref;

		bool32 do_clear_depth = (out_renderpass->clear_flags & RenderpassClearFlags::DEPTH_BUFFER);
		VkAttachmentDescription depth_att = {};
		VkAttachmentReference depth_att_ref = {};
		if (do_clear_depth)
		{		
			depth_att.format = context->device.depth_format; // TODO: Make configurable
			depth_att.samples = VK_SAMPLE_COUNT_1_BIT;
			if (has_prev_pass)
				depth_att.loadOp = do_clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
			else 
				depth_att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depth_att.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depth_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depth_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depth_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depth_att.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depth_att.flags = 0;

			attachment_descriptions[attachment_description_count++] = depth_att;
		
			depth_att_ref.attachment = 1;
			depth_att_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			subpass.pDepthStencilAttachment = &depth_att_ref;
		}
		else
		{
			attachment_descriptions[attachment_description_count] = {};
			subpass.pDepthStencilAttachment = 0;
		}


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

		VK_CHECK(vkCreateRenderPass(context->device.logical_device, &create_info, context->allocator_callbacks, &v_renderpass->handle));

	}

	void renderpass_destroy(Renderpass* renderpass)
	{
		if (renderpass->internal_data.data)
		{
			VulkanRenderpass* v_renderpass = (VulkanRenderpass*)renderpass->internal_data.data;
			vkDestroyRenderPass(context->device.logical_device, v_renderpass->handle, context->allocator_callbacks);
			v_renderpass->handle = 0;
			renderpass->internal_data.free_data();
		}
	}

	Renderpass* renderpass_get(const char* name)
	{
		if (!name[0])
		{
			SHMERROR("renderpass_get - empty name. Returning 0.");
			return 0;
		}

		uint32 id = context->renderpass_table.get_value(name);
		if (id == INVALID_ID)
		{
			SHMERRORV("renderpass_get - No renderpass called '%s' registered. Returning 0.", name);
			return 0;
		}

		return &context->registered_renderpasses[id];
	}

	bool32 renderpass_begin(Renderpass* renderpass, RenderTarget* render_target)
	{
		VulkanCommandBuffer* command_buffer = &context->graphics_command_buffers[context->image_index];
		VulkanRenderpass* v_renderpass = (VulkanRenderpass*)renderpass->internal_data.data;

		VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		begin_info.renderPass = v_renderpass->handle;
		begin_info.framebuffer = (VkFramebuffer)render_target->internal_framebuffer;
		begin_info.renderArea.offset.x = renderpass->offset.x;
		begin_info.renderArea.offset.y = renderpass->offset.y;
		begin_info.renderArea.extent.width = renderpass->dim.width;
		begin_info.renderArea.extent.height = renderpass->dim.height;

		begin_info.clearValueCount = 0;
		begin_info.pClearValues = 0;

		VkClearValue clear_values[2] = {};
		if (renderpass->clear_flags & RenderpassClearFlags::COLOR_BUFFER)
		{
			clear_values[begin_info.clearValueCount].color.float32[0] = renderpass->clear_color.r;
			clear_values[begin_info.clearValueCount].color.float32[1] = renderpass->clear_color.g;
			clear_values[begin_info.clearValueCount].color.float32[2] = renderpass->clear_color.b;
			clear_values[begin_info.clearValueCount].color.float32[3] = renderpass->clear_color.a;			
		}
		begin_info.clearValueCount++;

		if (renderpass->clear_flags & RenderpassClearFlags::DEPTH_BUFFER)
		{
			clear_values[begin_info.clearValueCount].color.float32[0] = renderpass->clear_color.r;
			clear_values[begin_info.clearValueCount].color.float32[1] = renderpass->clear_color.g;
			clear_values[begin_info.clearValueCount].color.float32[2] = renderpass->clear_color.b;
			clear_values[begin_info.clearValueCount].color.float32[3] = renderpass->clear_color.a;
			clear_values[begin_info.clearValueCount].depthStencil.depth = v_renderpass->depth;

			clear_values[begin_info.clearValueCount].depthStencil.stencil = (renderpass->clear_flags & RenderpassClearFlags::STENCIL_BUFFER) ? v_renderpass->stencil : 0;

			begin_info.clearValueCount++;
		}


		begin_info.pClearValues = (begin_info.clearValueCount > 0) ? clear_values : 0;

		vkCmdBeginRenderPass(command_buffer->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
		command_buffer->state = VulkanCommandBufferState::IN_RENDER_PASS;

		return true;
	}

	bool32 renderpass_end(Renderpass* renderpass)
	{
		VulkanCommandBuffer* command_buffer = &context->graphics_command_buffers[context->image_index];
		vkCmdEndRenderPass(command_buffer->handle);
		command_buffer->state = VulkanCommandBufferState::RECORDING;
		return true;
	}

	void render_target_create(uint32 attachment_count, Texture* const * attachments, Renderpass* pass, uint32 width, uint32 height, RenderTarget* out_target)
	{
		SHMASSERT(attachment_count <= 32);
		VkImageView attachment_views[32];
		for (uint32 i = 0; i < attachment_count; i++) 
			attachment_views[i] = ((VulkanImage*)attachments[i]->internal_data.data)->view;
		

		// Take a copy of the attachments and count.
		if (!out_target->attachments.data)
		{
			out_target->attachments.init(attachment_count, 0, AllocationTag::MAIN);
		}		
		else if (out_target->attachments.capacity < attachment_count)
		{
			out_target->attachments.free_data();
			out_target->attachments.init(attachment_count, 0, AllocationTag::MAIN);
		}

		for (uint32 i = 0; i < attachment_count; i++)
			out_target->attachments[i] = attachments[i];	

		VkFramebufferCreateInfo framebuffer_create_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebuffer_create_info.renderPass = ((VulkanRenderpass*)pass->internal_data.data)->handle;
		framebuffer_create_info.attachmentCount = attachment_count;
		framebuffer_create_info.pAttachments = attachment_views;
		framebuffer_create_info.width = width;
		framebuffer_create_info.height = height;
		framebuffer_create_info.layers = 1;

		VK_CHECK(vkCreateFramebuffer(context->device.logical_device, &framebuffer_create_info, context->allocator_callbacks, (VkFramebuffer*)&out_target->internal_framebuffer));
	}

	void render_target_destroy(RenderTarget* target, bool32 free_internal_memory)
	{
		if (!target->internal_framebuffer)
			return;

		vkDestroyFramebuffer(context->device.logical_device, (VkFramebuffer)target->internal_framebuffer, context->allocator_callbacks);
		target->internal_framebuffer = 0;
		if (free_internal_memory)
			target->attachments.free_data();
	}

}
