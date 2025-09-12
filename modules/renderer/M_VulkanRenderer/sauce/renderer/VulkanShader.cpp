#include "VulkanBackend.hpp"

#include "VulkanInternal.hpp"
#include <resources/loaders/GenericLoader.hpp>
#include <systems/TextureSystem.hpp>
#include <systems/MaterialSystem.hpp>

// TODO: Get rid of frontend include
#include <renderer/RendererFrontend.hpp>

#include "optick.h"

namespace Renderer::Vulkan
{
	extern VulkanContext* context;

	static bool8 _create_shader_module(VulkanShader* shader, const char* filename, VkShaderStageFlagBits stage_flags, VulkanShaderStage* shader_stage);

	static VkShaderStageFlagBits _convert_shader_stage(ShaderStage::Value stage);
	static VkSamplerAddressMode _convert_repeat_type(TextureRepeat::Value repeat);
	static VkFilter convert_filter_type(TextureFilter::Value filter);

	bool8 vk_shader_init(ShaderConfig* config, Shader* shader)
	{
		VkDevice& logical_device = context->device.logical_device;
		VkAllocationCallbacks* vk_allocator = context->allocator_callbacks;

		if (shader->internal_data)
		{
			SHMERROR("Shader already has internal vulkan data assigned. Creation failed.");
			return false;
		}

		shader->internal_data = Memory::allocate(sizeof(VulkanShader), AllocationTag::Renderer);
		VulkanShader* v_shader = (VulkanShader*)shader->internal_data;

		VkShaderStageFlags vk_stages[RendererConfig::shader_max_stage_count];
		for (uint8 i = 0; i < config->stages_count; ++i) {
			switch (config->stages[i].stage) {
			case ShaderStage::Fragment:
				vk_stages[i] = VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			case ShaderStage::Vertex:
				vk_stages[i] = VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case ShaderStage::Geometry:
				SHMWARN("shader_create: VK_SHADER_STAGE_GEOMETRY_BIT is set but not yet supported.");
				vk_stages[i] = VK_SHADER_STAGE_GEOMETRY_BIT;
				break;
			case ShaderStage::Compute:
				SHMWARN("shader_create: SHADER_STAGE_COMPUTE is set but not yet supported.");
				vk_stages[i] = VK_SHADER_STAGE_COMPUTE_BIT;
				break;
			default:
				SHMWARNV("Unsupported stage type: %i", config->stages[i].stage);
				break;
			}
		}

		v_shader->renderpass = (VulkanRenderpass*)config->renderpass->internal_data.data;

		v_shader->stage_count = config->stages_count;

		// Create a module for each stage.
		for (uint32 i = 0; i < v_shader->stage_count; ++i)
		{
			VkShaderStageFlagBits stage_flags = _convert_shader_stage(config->stages[i].stage);
			if (!_create_shader_module(v_shader, config->stages[i].filename, stage_flags, &v_shader->stages[i]))
			{
				SHMERRORV("Unable to create %s shader module for '%s'. Shader will be destroyed.", config->stages[i].filename, shader->name.c_str());
				return false;
			}
		}

		// Static lookup table for our types->Vulkan ones.
		static VkFormat types[11] = {};
		if (!types[0])
		{
			types[(uint32)ShaderAttributeType::Float32] = VK_FORMAT_R32_SFLOAT;
			types[(uint32)ShaderAttributeType::Float32_2] = VK_FORMAT_R32G32_SFLOAT;
			types[(uint32)ShaderAttributeType::Float32_3] = VK_FORMAT_R32G32B32_SFLOAT;
			types[(uint32)ShaderAttributeType::Float32_4] = VK_FORMAT_R32G32B32A32_SFLOAT;
			types[(uint32)ShaderAttributeType::Int8] = VK_FORMAT_R8_SINT;
			types[(uint32)ShaderAttributeType::UInt8] = VK_FORMAT_R8_UINT;
			types[(uint32)ShaderAttributeType::Int16] = VK_FORMAT_R16_SINT;
			types[(uint32)ShaderAttributeType::UInt16] = VK_FORMAT_R16_UINT;
			types[(uint32)ShaderAttributeType::Int32] = VK_FORMAT_R32_SINT;
			types[(uint32)ShaderAttributeType::UInt32] = VK_FORMAT_R32_UINT;
		}

		// Process attributes
		VkVertexInputAttributeDescription attribute_descriptions[RendererConfig::shader_max_attribute_count];
		uint32 offset = 0;
		for (uint32 i = 0; i < shader->attributes.capacity; ++i)
		{
			// Setup the new attribute.
			VkVertexInputAttributeDescription attribute;
			attribute.location = i;
			attribute.binding = 0;
			attribute.offset = offset;
			attribute.format = types[(uint32)shader->attributes[i].type];

			// Push into the config's attribute collection and add to the stride.
			attribute_descriptions[i] = attribute;

			offset += shader->attributes[i].size;
		}

		// TODO: Replace
		VkDescriptorPoolSize pool_sizes[2];
		pool_sizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 };
		pool_sizes[1] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 };

		// Descriptor pool.
		VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.poolSizeCount = 2;
		pool_info.pPoolSizes = pool_sizes;
		pool_info.maxSets = RendererConfig::shader_max_instance_count;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		// Create descriptor pool.
		VkResult result = vkCreateDescriptorPool(logical_device, &pool_info, vk_allocator, &v_shader->descriptor_pool);
		if (!vk_result_is_success(result))
		{
			SHMERRORV("vulkan_shader_initialize failed creating descriptor pool: '%s'", vk_result_string(result, true));
			return false;
		}

		{
			VulkanDescriptorSetLayout& set_layout = v_shader->global_descriptor_set_layout;
			VkDescriptorSetLayoutBinding bindings[2] = {};

			if (shader->global_uniform_count > 0)
			{
				VkDescriptorSetLayoutBinding* ubo_binding = &bindings[set_layout.binding_count];
				ubo_binding->binding = set_layout.binding_count;
				ubo_binding->descriptorCount = 1;
				ubo_binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				ubo_binding->stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_layout.binding_count++;
			}

			if (shader->global_uniform_sampler_count > 0)
			{
				VkDescriptorSetLayoutBinding* samplers_binding = &bindings[set_layout.binding_count];
				samplers_binding->binding = set_layout.binding_count;
				samplers_binding->descriptorCount = shader->global_uniform_sampler_count;
				samplers_binding->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				samplers_binding->stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_layout.binding_count++;
			}

			if (set_layout.binding_count > 0)
			{
				VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
				layout_info.bindingCount = set_layout.binding_count;
				layout_info.pBindings = bindings;
				result = vkCreateDescriptorSetLayout(logical_device, &layout_info, vk_allocator, &v_shader->global_descriptor_set_layout.handle);
				if (!vk_result_is_success(result))
				{
					SHMERRORV("Failed creating descriptor set layout: '%s'", vk_result_string(result, true));
					return false;
				}
			}
		}

		{
			VulkanDescriptorSetLayout& set_layout = v_shader->instance_descriptor_set_layout;
			VkDescriptorSetLayoutBinding bindings[2] = {};

			if (shader->instance_uniform_count > 0)
			{
				VkDescriptorSetLayoutBinding* ubo_binding = &bindings[set_layout.binding_count];
				ubo_binding->binding = set_layout.binding_count;
				ubo_binding->descriptorCount = 1;
				ubo_binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				ubo_binding->stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_layout.binding_count++;
			}

			if (shader->instance_uniform_sampler_count > 0)
			{
				VkDescriptorSetLayoutBinding* samplers_binding = &bindings[set_layout.binding_count];
				samplers_binding->binding = set_layout.binding_count;
				samplers_binding->descriptorCount = shader->instance_uniform_sampler_count;
				samplers_binding->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				samplers_binding->stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_layout.binding_count++;
			}

			if (set_layout.binding_count > 0)
			{
				VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
				layout_info.bindingCount = set_layout.binding_count;
				layout_info.pBindings = bindings;
				result = vkCreateDescriptorSetLayout(logical_device, &layout_info, vk_allocator, &v_shader->instance_descriptor_set_layout.handle);
				if (!vk_result_is_success(result))
				{
					SHMERRORV("Failed creating descriptor set layout: '%s'", vk_result_string(result, true));
					return false;
				}
			}
		}

		// TODO: This feels wrong to have these here, at least in this fashion. Should probably
		// Be configured to pull from someplace instead.
		// Viewport.
		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = (float32)context->framebuffer_height;
		viewport.width = (float32)context->framebuffer_width;
		viewport.height = -(float32)context->framebuffer_height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Scissor
		VkRect2D scissor;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		scissor.extent.width = context->framebuffer_width;
		scissor.extent.height = context->framebuffer_height;

		VkPipelineShaderStageCreateInfo stage_create_infos[RendererConfig::shader_max_stage_count] = {};
		for (uint32 i = 0; i < v_shader->stage_count; ++i)
		{
			stage_create_infos[i] = {};
			stage_create_infos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage_create_infos[i].stage = v_shader->stages[i].stage_flags;
			stage_create_infos[i].module = v_shader->stages[i].shader_module_handle;
			stage_create_infos[i].pName = "main";
		}

		v_shader->pipelines.init((uint32)VulkanTopologyCLass::TOPOLOGY_CLASS_COUNT, 0);

		RenderTopologyTypeFlags::Value pipeline_topologies[(uint32)VulkanTopologyCLass::TOPOLOGY_CLASS_COUNT];

		pipeline_topologies[(uint32)VulkanTopologyCLass::POINT] = RenderTopologyTypeFlags::PointList;
		pipeline_topologies[(uint32)VulkanTopologyCLass::LINE] = RenderTopologyTypeFlags::LineList;
		pipeline_topologies[(uint32)VulkanTopologyCLass::LINE] |= RenderTopologyTypeFlags::LineStrip;
		pipeline_topologies[(uint32)VulkanTopologyCLass::TRIANGLE] = RenderTopologyTypeFlags::TriangleList;
		pipeline_topologies[(uint32)VulkanTopologyCLass::TRIANGLE] |= RenderTopologyTypeFlags::TriangleStrip;
		pipeline_topologies[(uint32)VulkanTopologyCLass::TRIANGLE] |= RenderTopologyTypeFlags::TriangleFan;

		if (shader->topologies & RenderTopologyTypeFlags::PointList)
			v_shader->pipelines[(uint32)VulkanTopologyCLass::POINT] = (VulkanPipeline*)Memory::allocate(sizeof(VulkanPipeline), AllocationTag::Renderer);

		if (shader->topologies & RenderTopologyTypeFlags::LineList || shader->topologies & RenderTopologyTypeFlags::LineStrip)
			v_shader->pipelines[(uint32)VulkanTopologyCLass::LINE] = (VulkanPipeline*)Memory::allocate(sizeof(VulkanPipeline), AllocationTag::Renderer);

		if (shader->topologies & RenderTopologyTypeFlags::TriangleList || shader->topologies & RenderTopologyTypeFlags::TriangleStrip || shader->topologies & RenderTopologyTypeFlags::TriangleFan)
			v_shader->pipelines[(uint32)VulkanTopologyCLass::TRIANGLE] = (VulkanPipeline*)Memory::allocate(sizeof(VulkanPipeline), AllocationTag::Renderer);

		VkDescriptorSetLayout descriptor_set_layouts[8] = {};
		uint32 descriptor_set_layout_count = 0;
		if (v_shader->global_descriptor_set_layout.handle) descriptor_set_layouts[descriptor_set_layout_count++] = v_shader->global_descriptor_set_layout.handle;
		if (v_shader->instance_descriptor_set_layout.handle) descriptor_set_layouts[descriptor_set_layout_count++] = v_shader->instance_descriptor_set_layout.handle;
		
		v_shader->bound_pipeline_id = Constants::max_u32;
		for (uint32 i = 0; i < v_shader->pipelines.capacity; i++)
		{
			if (!v_shader->pipelines[i])
				continue;

			VulkanPipelineConfig p_config = {};
			p_config.renderpass = v_shader->renderpass;
			p_config.vertex_stride = shader->attribute_stride;
			p_config.attribute_count = shader->attributes.capacity;
			p_config.attribute_descriptions = attribute_descriptions;
			p_config.descriptor_set_layout_count = descriptor_set_layout_count;
			p_config.descriptor_set_layouts = descriptor_set_layouts;
			p_config.stage_count = v_shader->stage_count;
			p_config.stages = stage_create_infos;
			p_config.viewport = viewport;
			p_config.scissor = scissor;
			p_config.cull_mode = shader->cull_mode;
			p_config.is_wireframe = false;
			p_config.shader_flags = shader->shader_flags;
			p_config.push_constant_range_count = shader->push_constant_range_count;
			p_config.push_constant_ranges = shader->push_constant_ranges;
			p_config.topologies = pipeline_topologies[i];

			if (!vk_pipeline_create(&p_config, v_shader->pipelines[i]))
			{
				SHMERROR("Failed to load graphics pipeline for object shader.");
				return false;
			}

			if (v_shader->bound_pipeline_id != Constants::max_u32)
				continue;

			v_shader->bound_pipeline_id = i;

			for (RenderTopologyTypeFlags::Value type = 1; type < RenderTopologyTypeFlags::AllTypesMask; type = type << 1) 
			{
				if (v_shader->pipelines[i]->topologies & type) 
				{
					switch (type) 
					{
					case RenderTopologyTypeFlags::PointList:
						v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
					case RenderTopologyTypeFlags::LineList:
						v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
					case RenderTopologyTypeFlags::LineStrip:
						v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
					case RenderTopologyTypeFlags::TriangleList:
						v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
					case RenderTopologyTypeFlags::TriangleStrip:
						v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
					case RenderTopologyTypeFlags::TriangleFan:
						v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
					default:
						SHMWARNV("primitive topology '%u' not supported. Skipping.", type); break;
					}

					break;
				}
			}
		}

		if (v_shader->bound_pipeline_id == Constants::max_u32)
		{
			SHMERROR("No available topology classes are available, so a pipeline cannot be bound. Check shader configuration.");
			return false;
		}

		// Allocate global descriptor sets, one per frame in flight. Global is always the first set.
		VkDescriptorSetLayout global_layouts[3] =
		{
			v_shader->global_descriptor_set_layout.handle,
			v_shader->global_descriptor_set_layout.handle,
			v_shader->global_descriptor_set_layout.handle
		};

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = v_shader->descriptor_pool;
		alloc_info.descriptorSetCount = RendererConfig::framebuffer_count;
		alloc_info.pSetLayouts = global_layouts;
		VK_CHECK(vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, v_shader->global_descriptor_sets));

		return true;
	}

	void vk_shader_destroy(Shader* shader)
	{
		VulkanShader* v_shader = (VulkanShader*)shader->internal_data;
		if (!v_shader)
		{
			SHMERROR("vulkan_renderer_shader_destroy requires a valid pointer to a shader.");
			return;
		}

		VkDevice logical_device = context->device.logical_device;
		VkAllocationCallbacks* vk_allocator = context->allocator_callbacks;

		if (v_shader->global_descriptor_set_layout.handle)
		{
			vkDestroyDescriptorSetLayout(logical_device, v_shader->global_descriptor_set_layout.handle, vk_allocator);
			v_shader->global_descriptor_set_layout.handle = 0;
		}

		if (v_shader->instance_descriptor_set_layout.handle)
		{
			vkDestroyDescriptorSetLayout(logical_device, v_shader->instance_descriptor_set_layout.handle, vk_allocator);
			v_shader->instance_descriptor_set_layout.handle = 0;
		}

		if (v_shader->descriptor_pool)
			vkDestroyDescriptorPool(logical_device, v_shader->descriptor_pool, vk_allocator);
	
		// Pipeline
		for (uint32 i = 0; i < v_shader->pipelines.capacity; i++)
		{
			if (!v_shader->pipelines[i])
				continue;

			vk_pipeline_destroy(v_shader->pipelines[i]);
			Memory::free_memory(v_shader->pipelines[i]);		
		}
		v_shader->pipelines.free_data();

		// Shader modules
		for (uint32 i = 0; i < v_shader->stage_count; ++i)
			vkDestroyShaderModule(context->device.logical_device, v_shader->stages[i].shader_module_handle, context->allocator_callbacks);

		// Free the internal data memory.
		Memory::free_memory(shader->internal_data);
		shader->internal_data = 0;
	}

	bool8 vk_shader_use(Shader* s)
	{
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		VulkanCommandBuffer* command_buffer = &context->graphics_command_buffers[context->bound_framebuffer_index];

		vk_pipeline_bind(&context->graphics_command_buffers[context->bound_framebuffer_index], VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipelines[v_shader->bound_pipeline_id]);
		vkCmdSetPrimitiveTopology(command_buffer->handle, v_shader->current_topology);
		return true;
	}

	bool8 vk_shader_bind_globals(Shader* s)
	{
		uint32 image_index = context->bound_framebuffer_index;
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		VkCommandBuffer command_buffer = context->graphics_command_buffers[image_index].handle;
		VkDescriptorSet global_descriptor = v_shader->global_descriptor_sets[image_index];

		// Bind the global descriptor set to be updated.
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipelines[v_shader->bound_pipeline_id]->layout, 0, 1, &global_descriptor, 0, 0);
		return true;
	}

	bool8 vk_shader_bind_instance(Shader* s, ShaderInstanceId instance_id)
	{
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		uint32 framebuffer_index = context->bound_framebuffer_index;
		VkCommandBuffer command_buffer = context->graphics_command_buffers[framebuffer_index].handle;

		VulkanShaderInstanceDescriptorSet* instance_descriptor = &v_shader->instance_descriptor_sets[s->bound_instance_id];
		VkDescriptorSet object_descriptor_set = instance_descriptor->descriptor_sets[framebuffer_index];

		// Bind the descriptor set to be updated, or in case the shader changed.
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipelines[v_shader->bound_pipeline_id]->layout, 1, 1, &object_descriptor_set, 0, 0);
		return true;
	}

	bool8 vk_shader_apply_globals(Shader* s)
	{
		OPTICK_EVENT();

		uint32 image_index = context->bound_framebuffer_index;
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;

		// Apply UBO first
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = ((VulkanBuffer*)s->uniform_buffer.internal_data.data)->handle;
		bufferInfo.offset = s->global_ubo_alloc_ref.byte_offset;
		bufferInfo.range = s->global_ubo_stride;

		// Update descriptor sets.
		VkWriteDescriptorSet ubo_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		ubo_write.dstSet = v_shader->global_descriptor_sets[image_index];
		ubo_write.dstBinding = 0;
		ubo_write.dstArrayElement = 0;
		ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_write.descriptorCount = 1;
		ubo_write.pBufferInfo = &bufferInfo;

		VkWriteDescriptorSet descriptor_writes[2];
		descriptor_writes[0] = ubo_write;

		uint32 global_set_binding_count = v_shader->global_descriptor_set_layout.binding_count;
		if (global_set_binding_count > 1)
		{
			// TODO: There are samplers to be written. Support this.
			global_set_binding_count = 1;
			SHMERROR("Global image samplers are not yet supported.");

			// VkWriteDescriptorSet sampler_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			// descriptor_writes[1] = ...
		}

		vkUpdateDescriptorSets(context->device.logical_device, global_set_binding_count, descriptor_writes, 0, 0);
		
		return true;
	}

	bool8 vk_shader_apply_instance(Shader* shader)
	{
		VulkanShader* v_shader = (VulkanShader*)shader->internal_data;
		uint32 image_index = context->bound_framebuffer_index;
		VkCommandBuffer command_buffer = context->graphics_command_buffers[image_index].handle;

		ShaderInstance* instance = &shader->instances[shader->bound_instance_id];
		VulkanShaderInstanceDescriptorSet* instance_descriptor = &v_shader->instance_descriptor_sets[shader->bound_instance_id];
		VkDescriptorSet descriptor_set = instance_descriptor->descriptor_sets[image_index];

		VkWriteDescriptorSet descriptor_writes[2] = {};  // Always a max of 2 descriptor sets.
		uint32 descriptor_count = 0;
		uint32 descriptor_index = 0;

		uint8* instance_ubo_generation = &(instance_descriptor->descriptor_states[descriptor_index].generations[image_index]);

		VkDescriptorBufferInfo buffer_info;
		buffer_info.buffer = ((VulkanBuffer*)shader->uniform_buffer.internal_data.data)->handle;
		buffer_info.offset = instance->alloc_ref.byte_offset;
		buffer_info.range = shader->instance_ubo_stride;

		VkWriteDescriptorSet ubo_descriptor = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		ubo_descriptor.dstSet = descriptor_set;
		ubo_descriptor.dstBinding = descriptor_index;
		ubo_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_descriptor.descriptorCount = 1;
		ubo_descriptor.pBufferInfo = &buffer_info;

		if (shader->instance_uniform_count)
		{	
			if (*instance_ubo_generation == Constants::max_u8)
			{
				descriptor_writes[descriptor_count] = ubo_descriptor;
				descriptor_count++;

				// Update the frame generation. In this case it is only needed once since this is a buffer.
				*instance_ubo_generation = 1;
			}
			descriptor_index++;
		}

		VkDescriptorImageInfo image_infos[RendererConfig::shader_max_instance_texture_count];
		if (shader->instance_uniform_sampler_count)
		{
			uint32 total_sampler_count = shader->instance_uniform_sampler_count;
			uint32 update_sampler_count = 0;
			for (uint32 i = 0; i < total_sampler_count; ++i)
			{
				// TODO: only update in the list if actually needing an update.
				TextureMap* map = shader->instance_texture_maps[(shader->bound_instance_id * shader->instance_uniform_sampler_count) + i];
				TextureSampler* sampler = Renderer::get_texture_sampler(map->sampler_id);
				Texture* texture = map->texture;

				if (!(texture->flags & TextureFlags::IsLoaded))
					texture = TextureSystem::get_default_texture();

				VulkanImage* image = (VulkanImage*)texture->internal_data.data;
				image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				image_infos[i].imageView = image->view;
				image_infos[i].sampler = (VkSampler)sampler->internal_data;

				update_sampler_count++;
			}

			VkWriteDescriptorSet sampler_descriptor = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			sampler_descriptor.dstSet = descriptor_set;
			sampler_descriptor.dstBinding = descriptor_index;
			sampler_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			sampler_descriptor.descriptorCount = update_sampler_count;
			sampler_descriptor.pImageInfo = image_infos;

			descriptor_writes[descriptor_count] = sampler_descriptor;
			descriptor_count++;
		}
			
		if (descriptor_count > 0)
			vkUpdateDescriptorSets(context->device.logical_device, descriptor_count, descriptor_writes, 0, 0);

		return true;
	}

	bool8 vk_shader_acquire_instance(Shader* shader, ShaderInstanceId instance_id)
	{
		VulkanShader* v_shader = (VulkanShader*)shader->internal_data;

		uint32 instance_texture_count = shader->instance_uniform_sampler_count;
		
		VulkanShaderInstanceDescriptorSet* instance_descriptor = &v_shader->instance_descriptor_sets[instance_id];

		// Each descriptor binding in the set
		uint32 binding_count = v_shader->instance_descriptor_set_layout.binding_count;
		for (uint32 i = 0; i < binding_count; ++i)
		{
			for (uint32 j = 0; j < 3; ++j)
			{
				instance_descriptor->descriptor_states[i].generations[j] = Constants::max_u8;
				instance_descriptor->descriptor_states[i].ids[j] = Constants::max_u32;
			}
		}

		// Allocate 3 descriptor sets (one per frame).
		VkDescriptorSetLayout layouts[Renderer::RendererConfig::framebuffer_count] =
		{
			v_shader->instance_descriptor_set_layout.handle,
			v_shader->instance_descriptor_set_layout.handle,
			v_shader->instance_descriptor_set_layout.handle
		};

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = v_shader->descriptor_pool;
		alloc_info.descriptorSetCount = RendererConfig::framebuffer_count;
		alloc_info.pSetLayouts = layouts;
		VkResult result = vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, instance_descriptor->descriptor_sets);
		if (result != VK_SUCCESS)
		{
			SHMERRORV("Error allocating instance descriptor sets in shader: '%s'.", vk_result_string(result, true));
			return false;
		}

		return true;

	}

	bool8 vk_shader_release_instance(Shader* s, ShaderInstanceId instance_id)
	{

		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		VulkanShaderInstanceDescriptorSet* instance_descriptor = &v_shader->instance_descriptor_sets[instance_id];

		// Wait for any pending operations using the descriptor set to finish.
		vkDeviceWaitIdle(context->device.logical_device);

		// Free 3 descriptor sets (one per frame)
		VkResult result = vkFreeDescriptorSets(context->device.logical_device, v_shader->descriptor_pool, 3, instance_descriptor->descriptor_sets);
		if (result != VK_SUCCESS)
		{
			SHMERROR("Error freeing object shader descriptor sets!");
		}

		return true;

	}

	bool8 vk_shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value)
	{
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		
		if (uniform->scope == ShaderScope::Local)
		{
			VkCommandBuffer command_buffer = context->graphics_command_buffers[context->bound_framebuffer_index].handle;
			vkCmdPushConstants(command_buffer, v_shader->pipelines[v_shader->bound_pipeline_id]->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, uniform->offset, uniform->size, value);
		}

		return true;
	}

	bool8 vk_texture_sampler_init(TextureSampler* out_sampler) 
	{
		// Create a sampler for the texture
		VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

		sampler_info.minFilter = convert_filter_type(out_sampler->filter_minify);
		sampler_info.magFilter = convert_filter_type(out_sampler->filter_magnify);

		sampler_info.addressModeU = _convert_repeat_type(out_sampler->repeat_u);
		sampler_info.addressModeV = _convert_repeat_type(out_sampler->repeat_v);
		sampler_info.addressModeW = _convert_repeat_type(out_sampler->repeat_w);

		// TODO: Configurable
		sampler_info.anisotropyEnable = VK_TRUE;
		sampler_info.maxAnisotropy = 16;
		sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		sampler_info.unnormalizedCoordinates = VK_FALSE;
		sampler_info.compareEnable = VK_FALSE;
		sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.minLod = 0.0f;
		sampler_info.maxLod = 0.0f;

		VkResult result = vkCreateSampler(context->device.logical_device, &sampler_info, context->allocator_callbacks, (VkSampler*)&out_sampler->internal_data);
		if (!vk_result_is_success(VK_SUCCESS)) {
			SHMERRORV("Error creating texture sampler: %s", vk_result_string(result, true));
			return false;
		}

		return true;
	}

	void vk_texture_sampler_destroy(TextureSampler* sampler) 
	{
		vkDeviceWaitIdle(context->device.logical_device);
		vkDestroySampler(context->device.logical_device, (VkSampler)sampler->internal_data, context->allocator_callbacks);
		sampler->internal_data = 0;
	}

	static bool8 _create_shader_module(VulkanShader* shader, const char* filename, VkShaderStageFlagBits stage_flags, VulkanShaderStage* shader_stage)
	{
		// Read the resource.
		Buffer data = {};
		if (!ResourceSystem::generic_loader_load(filename, &data))
		{
			SHMERRORV("Unable to read shader module file: %s.", filename);
			return false;
		}

		VkShaderModuleCreateInfo module_create_info = {};
		module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		module_create_info.codeSize = data.size;
		module_create_info.pCode = (uint32*)data.data;

		VK_CHECK(vkCreateShaderModule(
			context->device.logical_device,
			&module_create_info,
			context->allocator_callbacks,
			&shader_stage->shader_module_handle));

		// Release the resource.
		ResourceSystem::generic_loader_unload(&data);
		shader_stage->stage_flags = stage_flags;

		return true;
	}

	static constexpr VkShaderStageFlagBits _shader_stage_lookup[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT
	};

	static VkShaderStageFlagBits _convert_shader_stage(ShaderStage::Value stage)
	{
		uint32 vk_stage = 0;
		if (stage & ShaderStage::Vertex) vk_stage |= VK_SHADER_STAGE_VERTEX_BIT;
		if (stage & ShaderStage::Fragment) vk_stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (stage & ShaderStage::Geometry) vk_stage |= VK_SHADER_STAGE_GEOMETRY_BIT;
		if (stage & ShaderStage::Compute) vk_stage |= VK_SHADER_STAGE_COMPUTE_BIT;

		return (VkShaderStageFlagBits)vk_stage;
	}

	static VkSamplerAddressMode _convert_repeat_type(TextureRepeat::Value repeat) 
	{
		switch (repeat) 
		{
		case TextureRepeat::REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case TextureRepeat::MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case TextureRepeat::CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case TextureRepeat::CLAMP_TO_BORDER:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		default:
			SHMWARNV("convert_repeat_type Type %u not supported, defaulting to repeat.", (uint32)repeat);
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	static VkFilter convert_filter_type(TextureFilter::Value filter) 
	{
		switch (filter) 
		{
		case TextureFilter::NEAREST:
			return VK_FILTER_NEAREST;
		case TextureFilter::LINEAR:
			return VK_FILTER_LINEAR;
		default:
			SHMWARNV("convert_filter_type: Unsupported filter type %u, defaulting to linear.", (uint32)filter);
			return VK_FILTER_LINEAR;
		}
	}
}