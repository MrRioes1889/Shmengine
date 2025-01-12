#include "VulkanTypes.hpp"
#include "VulkanBackend.hpp"
#include "VulkanInternal.hpp"

#include <systems/TextureSystem.hpp>

namespace Renderer::Vulkan
{

	extern VulkanContext* context;

	bool32 vk_renderpass_create(const RenderPassConfig* config, RenderPass* out_renderpass)
	{

		out_renderpass->internal_data.init(sizeof(VulkanRenderpass), 0, AllocationTag::RENDERER);
		VulkanRenderpass* v_renderpass = (VulkanRenderpass*)out_renderpass->internal_data.data;

		v_renderpass->depth = config->depth;
		v_renderpass->stencil = config->stencil;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		Darray<VkAttachmentDescription> attachment_descriptions(4, 0);
		Darray<VkAttachmentDescription> color_att_descriptions(4, 0);
		Darray<VkAttachmentDescription> depth_att_descriptions(4, 0);

		bool8 do_clear_color = (out_renderpass->clear_flags & RenderpassClearFlags::COLOR_BUFFER) != 0;
		bool8 do_clear_depth = (out_renderpass->clear_flags & RenderpassClearFlags::DEPTH_BUFFER) != 0;

		for (uint32 i = 0; i < config->target_config.attachment_configs.capacity; i++)
		{
			const RenderTargetAttachmentConfig* att_config = &config->target_config.attachment_configs[i];

			VkAttachmentDescription att_desc = {};
			if (att_config->type == RenderTargetAttachmentType::COLOR)
			{
				if (att_config->source == RenderTargetAttachmentSource::DEFAULT)
					att_desc.format = context->swapchain.image_format.format;
				else
					att_desc.format = VK_FORMAT_R8G8B8A8_UNORM;

				att_desc.samples = VK_SAMPLE_COUNT_1_BIT;

				if (att_config->load_op == RenderTargetAttachmentLoadOp::DONT_CARE)
				{
					att_desc.loadOp = do_clear_color ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				}
				else if (att_config->load_op == RenderTargetAttachmentLoadOp::LOAD)
				{
					if (do_clear_color)
					{
						SHMWARN("Color attachment load operation overwritten by clear flag!");
						att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					}
					else
					{
						att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					}
				}
				else
				{
					SHMERROR("Failed to load attachment descriptions: Invalid load operation.");
					return false;
				}

				if (att_config->store_op == RenderTargetAttachmentStoreOp::DONT_CARE)
				{
					att_desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				}
				else if (att_config->store_op == RenderTargetAttachmentStoreOp::STORE)
				{
					att_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				}
				else
				{
					SHMERROR("Failed to load attachment descriptions: Invalid store operation.");
					return false;
				}

				att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				att_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				att_desc.initialLayout = att_config->load_op == RenderTargetAttachmentLoadOp::LOAD ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
				att_desc.finalLayout = att_config->present_after ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				color_att_descriptions.emplace(att_desc);
			}
			if (att_config->type == RenderTargetAttachmentType::DEPTH)
			{
				if (att_config->source == RenderTargetAttachmentSource::DEFAULT)
					att_desc.format = context->device.depth_format;
				else
					att_desc.format = context->device.depth_format;

				att_desc.samples = VK_SAMPLE_COUNT_1_BIT;

				if (att_config->load_op == RenderTargetAttachmentLoadOp::DONT_CARE)
				{
					att_desc.loadOp = do_clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				}
				else if (att_config->load_op == RenderTargetAttachmentLoadOp::LOAD)
				{
					if (do_clear_depth)
					{
						SHMWARN("Depth attachment load operation overwritten by clear flag!");
						att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					}
					else
					{
						att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					}
				}
				else
				{
					SHMERROR("Failed to load attachment descriptions: Invalid load operation.");
					return false;
				}

				if (att_config->store_op == RenderTargetAttachmentStoreOp::DONT_CARE)
				{
					att_desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				}
				else if (att_config->store_op == RenderTargetAttachmentStoreOp::STORE)
				{
					att_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				}
				else
				{
					SHMERROR("Failed to load attachment descriptions: Invalid store operation.");
					return false;
				}

				att_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				att_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				att_desc.initialLayout = att_config->load_op == RenderTargetAttachmentLoadOp::LOAD ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
				att_desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				depth_att_descriptions.emplace(att_desc);
			}

			attachment_descriptions.emplace(att_desc);
		}

		uint32 att_added = 0;

		subpass.colorAttachmentCount = 0;
		subpass.pColorAttachments = 0;
		Sarray<VkAttachmentReference> color_att_refs(color_att_descriptions.count, 0);
		if (color_att_refs.capacity > 0)
		{
			for (uint32 i = 0; i < color_att_refs.capacity; i++)
			{
				color_att_refs[i].attachment = att_added++;
				color_att_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}

			subpass.colorAttachmentCount = color_att_refs.capacity;
			subpass.pColorAttachments = color_att_refs.data;
		}

		subpass.pDepthStencilAttachment = 0;
		Sarray<VkAttachmentReference> depth_att_refs(depth_att_descriptions.count, 0);
		if (depth_att_refs.capacity > 0)
		{
			for (uint32 i = 0; i < depth_att_refs.capacity; i++)
			{
				depth_att_refs[i].attachment = att_added++;
				depth_att_refs[i].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}

			subpass.pDepthStencilAttachment = depth_att_refs.data;
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
		create_info.attachmentCount = attachment_descriptions.count;
		create_info.pAttachments = attachment_descriptions.data;
		create_info.subpassCount = 1;
		create_info.pSubpasses = &subpass;
		create_info.dependencyCount = 1;
		create_info.pDependencies = &dependency;
		create_info.pNext = 0;
		create_info.flags = 0;

		VK_CHECK(vkCreateRenderPass(context->device.logical_device, &create_info, context->allocator_callbacks, &v_renderpass->handle));

		attachment_descriptions.free_data();
		color_att_descriptions.free_data();
		color_att_refs.free_data();
		depth_att_descriptions.free_data();
		depth_att_refs.free_data();

		return true;

	}

	void vk_renderpass_destroy(RenderPass* renderpass)
	{
		if (renderpass->internal_data.data)
		{
			VulkanRenderpass* v_renderpass = (VulkanRenderpass*)renderpass->internal_data.data;
			vkDestroyRenderPass(context->device.logical_device, v_renderpass->handle, context->allocator_callbacks);
			v_renderpass->handle = 0;
			renderpass->internal_data.free_data();
		}
	}

	bool32 vk_renderpass_begin(RenderPass* renderpass, RenderTarget* render_target)
	{
		VulkanCommandBuffer* command_buffer = &context->graphics_command_buffers[context->bound_framebuffer_index];
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

		Math::Vec4f label_color = { Math::random_float32_clamped(0.0f, 1.0f), Math::random_float32_clamped(0.0f, 1.0f), Math::random_float32_clamped(0.0f, 1.0f), 1.0f };
		VK_DEBUG_BEGIN_LABEL(context, command_buffer->handle, renderpass->name.c_str(), label_color);

		return true;
	}

	bool32 vk_renderpass_end(RenderPass* renderpass)
	{
		VulkanRenderpass* v_renderpass = (VulkanRenderpass*)renderpass->internal_data.data;

		for (uint32 target_i = 0; target_i < renderpass->render_targets.capacity; target_i++)
		{
			for (uint32 att_i = 0; att_i < renderpass->render_targets[target_i].attachments.capacity; att_i++)
			{
				RenderTargetAttachment* att = (RenderTargetAttachment*)&renderpass->render_targets[target_i].attachments[att_i];
				VulkanImage* image = (VulkanImage*)att->texture->internal_data.data;

				VkImageLayout layout;
				if (att->type == RenderTargetAttachmentType::COLOR)
					layout = att->present_after ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				else
					layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				TaskInfo task;
				task.type = TaskType::SetImageLayout;
				task.set_image_layout.image = image;
				task.set_image_layout.new_layout = layout;

				context->end_of_frame_task_queue.enqueue(task);
			}
		}

		VulkanCommandBuffer* command_buffer = &context->graphics_command_buffers[context->bound_framebuffer_index];
		vkCmdEndRenderPass(command_buffer->handle);
		VK_DEBUG_END_LABEL(context, command_buffer->handle);
		command_buffer->state = VulkanCommandBufferState::RECORDING;
		return true;
	}

	bool32 vk_render_target_create(uint32 attachment_count, const RenderTargetAttachment* attachments, RenderPass* pass, uint32 width, uint32 height, RenderTarget* out_target)
	{
		SHMASSERT(attachment_count <= 32);
		VkImageView attachment_views[32];
		for (uint32 i = 0; i < attachment_count; i++) 
			attachment_views[i] = ((VulkanImage*)attachments[i].texture->internal_data.data)->view;

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

		return true;
	}

	void vk_render_target_destroy(RenderTarget* target, bool32 free_internal_memory)
	{
		if (!target->internal_framebuffer)
			return;

		vkDestroyFramebuffer(context->device.logical_device, (VkFramebuffer)target->internal_framebuffer, context->allocator_callbacks);
		target->internal_framebuffer = 0;
		if (free_internal_memory)
			target->attachments.free_data();
	}

}
