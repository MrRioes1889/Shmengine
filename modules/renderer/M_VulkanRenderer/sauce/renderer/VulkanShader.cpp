#include "VulkanBackend.hpp"

#include "VulkanInternal.hpp"
#include <resources/loaders/GenericLoader.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/ShaderSystem.hpp>

// TODO: Get rid of frontend include
#include <renderer/RendererFrontend.hpp>

#include "optick.h"

namespace Renderer::Vulkan
{

	extern VulkanContext* context;

	static bool32 create_shader_module(VulkanShader* shader, const VulkanShaderStageConfig& config, VulkanShaderStage* shader_stage);

	static const uint32 desc_set_index_global = 0;
	static const uint32 desc_set_index_instance = 1;

	bool32 vk_shader_create(Shader* shader, const ShaderConfig* config, const RenderPass* renderpass)
	{

		if (shader->internal_data)
		{
			SHMERROR("Shader already has internal vulkan data assigned. Creation failed.");
			return false;
		}

		shader->internal_data = Memory::allocate(sizeof(VulkanShader), AllocationTag::RENDERER);

		VkShaderStageFlags vk_stages[RendererConfig::shader_max_stages];
		for (uint8 i = 0; i < config->stages_count; ++i) {
			switch (config->stages[i].stage) {
			case ShaderStage::FRAGMENT:
				vk_stages[i] = VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			case ShaderStage::VERTEX:
				vk_stages[i] = VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case ShaderStage::GEOMETRY:
				SHMWARN("shader_create: VK_SHADER_STAGE_GEOMETRY_BIT is set but not yet supported.");
				vk_stages[i] = VK_SHADER_STAGE_GEOMETRY_BIT;
				break;
			case ShaderStage::COMPUTE:
				SHMWARN("shader_create: SHADER_STAGE_COMPUTE is set but not yet supported.");
				vk_stages[i] = VK_SHADER_STAGE_COMPUTE_BIT;
				break;
			default:
				SHMWARNV("Unsupported stage type: %i", config->stages[i].stage);
				break;
			}
		}

		VulkanShader* v_shader = (VulkanShader*)shader->internal_data;
		v_shader->renderpass = (VulkanRenderpass*)renderpass->internal_data.data;
		v_shader->config.max_descriptor_set_count = RendererConfig::shader_max_instances;

		v_shader->config.stage_count = 0;

		for (uint32 i = 0; i < config->stages_count; i++) {

			if (v_shader->config.stage_count + 1 > RendererConfig::shader_max_stages) {
				SHMERRORV("Shaders may have a maximum of %i stages", RendererConfig::shader_max_stages);
				return false;
			}

			VkShaderStageFlagBits stage_flag;
			switch (config->stages[i].stage) {
			case ShaderStage::VERTEX:
				stage_flag = VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case ShaderStage::FRAGMENT:
				stage_flag = VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			default:
				SHMERRORV("vulkan_shader_create: Unsupported shader stage flagged: %i. Stage ignored.", config->stages[i].stage);
				continue;
			}

			v_shader->config.stages[v_shader->config.stage_count].stage = stage_flag;
			CString::copy(config->stages[i].filename, v_shader->config.stages[v_shader->config.stage_count].filename, VulkanShaderStageConfig::max_filename_length);
			v_shader->config.stage_count++;
		}

		// TODO: Replace
		v_shader->config.pool_sizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 };
		v_shader->config.pool_sizes[1] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 };

		v_shader->config.descriptor_sets[0].sampler_binding_index = INVALID_ID8;
		v_shader->config.descriptor_sets[1].sampler_binding_index = INVALID_ID8;

		v_shader->config.cull_mode = config->cull_mode;

		v_shader->global_uniform_count = 0;
		v_shader->global_uniform_sampler_count = 0;
		v_shader->instance_uniform_sampler_count = 0;
		v_shader->instance_uniform_sampler_count = 0;
		v_shader->local_uniform_count = 0;
		for (uint32 i = 0; i < config->uniforms_count; i++)
		{
			switch (config->uniforms[i].scope)
			{
			case ShaderScope::GLOBAL:
			{
				if (config->uniforms[i].type == ShaderUniformType::SAMPLER)
					v_shader->global_uniform_sampler_count++;
				else
					v_shader->global_uniform_count++;
				break;
			}
			case ShaderScope::INSTANCE:
			{
				if (config->uniforms[i].type == ShaderUniformType::SAMPLER)
					v_shader->instance_uniform_sampler_count++;
				else
					v_shader->instance_uniform_count++;
				break;
			}
			case ShaderScope::LOCAL:
			{
				v_shader->local_uniform_count++;
				break;
			}
			}
		}

		if (v_shader->global_uniform_count > 0 || v_shader->global_uniform_sampler_count > 0)
		{
			VulkanDescriptorSetConfig& set_config = v_shader->config.descriptor_sets[v_shader->config.descriptor_set_count];

			if (v_shader->global_uniform_count > 0)
			{
				uint8 binding_index = set_config.binding_count;
				set_config.bindings[binding_index].binding = binding_index;
				set_config.bindings[binding_index].descriptorCount = 1;
				set_config.bindings[binding_index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				set_config.bindings[binding_index].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_config.binding_count++;
			}

			if (v_shader->global_uniform_sampler_count > 0)
			{
				uint8 binding_index = set_config.binding_count;
				set_config.bindings[binding_index].binding = binding_index;
				set_config.bindings[binding_index].descriptorCount = v_shader->global_uniform_sampler_count;
				set_config.bindings[binding_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				set_config.bindings[binding_index].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_config.sampler_binding_index = binding_index;
				set_config.binding_count++;
			}

			v_shader->config.descriptor_set_count++;
		}

		if (v_shader->instance_uniform_count > 0 || v_shader->instance_uniform_sampler_count > 0)
		{
			VulkanDescriptorSetConfig& set_config = v_shader->config.descriptor_sets[v_shader->config.descriptor_set_count];

			if (v_shader->instance_uniform_count > 0)
			{
				uint8 binding_index = set_config.binding_count;
				set_config.bindings[binding_index].binding = binding_index;
				set_config.bindings[binding_index].descriptorCount = 1;
				set_config.bindings[binding_index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				set_config.bindings[binding_index].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_config.binding_count++;
			}

			if (v_shader->instance_uniform_sampler_count > 0)
			{
				uint8 binding_index = set_config.binding_count;
				set_config.bindings[binding_index].binding = binding_index;
				set_config.bindings[binding_index].descriptorCount = v_shader->instance_uniform_sampler_count;
				set_config.bindings[binding_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				set_config.bindings[binding_index].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
				set_config.sampler_binding_index = binding_index;
				set_config.binding_count++;
			}

			v_shader->config.descriptor_set_count++;
		}

		// TODO: dynamic
		for (uint32 i = 0; i < RendererConfig::shader_max_instances; ++i) {
			v_shader->instance_states[i].id = INVALID_ID;
		}

		// Grab the UBO alignment requirement from the device.
		shader->required_ubo_alignment = context->device.properties.limits.minUniformBufferOffsetAlignment;

		return true;

	}

	void vk_shader_destroy(Shader* shader)
	{

		if (!shader->internal_data)
			return;

		VulkanShader* v_shader = (VulkanShader*)shader->internal_data;
		if (!v_shader)
		{
			SHMERROR("vulkan_renderer_shader_destroy requires a valid pointer to a shader.");
			return;
		}

		VkDevice logical_device = context->device.logical_device;
		VkAllocationCallbacks* vk_allocator = context->allocator_callbacks;

		for (uint32 i = 0; i < v_shader->config.descriptor_set_count; ++i)
		{
			if (v_shader->descriptor_set_layouts[i]) {
				vkDestroyDescriptorSetLayout(logical_device, v_shader->descriptor_set_layouts[i], vk_allocator);
				v_shader->descriptor_set_layouts[i] = 0;
			}
		}

		if (v_shader->descriptor_pool)
		{
			vkDestroyDescriptorPool(logical_device, v_shader->descriptor_pool, vk_allocator);
		}
	
		v_shader->mapped_uniform_buffer = 0;	

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
		for (uint32 i = 0; i < v_shader->config.stage_count; ++i)
		{
			vkDestroyShaderModule(context->device.logical_device, v_shader->stages[i].handle, context->allocator_callbacks);
		}

		// Free the internal data memory.
		Memory::free_memory(shader->internal_data);
		shader->internal_data = 0;

	}

	bool32 vk_shader_init(Shader* shader)
	{

		VkDevice& logical_device = context->device.logical_device;
		VkAllocationCallbacks* vk_allocator = context->allocator_callbacks;
		VulkanShader* v_shader = (VulkanShader*)shader->internal_data;

		// Create a module for each stage.
		for (uint32 i = 0; i < v_shader->config.stage_count; ++i)
		{
			if (!create_shader_module(v_shader, v_shader->config.stages[i], &v_shader->stages[i]))
			{
				SHMERRORV("Unable to create %s shader module for '%s'. Shader will be destroyed.", v_shader->config.stages[i].filename, shader->name.c_str());
				return false;
			}
		}

		// Static lookup table for our types->Vulkan ones.
		static VkFormat types[11] = {};
		if (!types[0])
		{
			types[(uint32)ShaderAttributeType::FLOAT32] = VK_FORMAT_R32_SFLOAT;
			types[(uint32)ShaderAttributeType::FLOAT32_2] = VK_FORMAT_R32G32_SFLOAT;
			types[(uint32)ShaderAttributeType::FLOAT32_3] = VK_FORMAT_R32G32B32_SFLOAT;
			types[(uint32)ShaderAttributeType::FLOAT32_4] = VK_FORMAT_R32G32B32A32_SFLOAT;
			types[(uint32)ShaderAttributeType::INT8] = VK_FORMAT_R8_SINT;
			types[(uint32)ShaderAttributeType::UINT8] = VK_FORMAT_R8_UINT;
			types[(uint32)ShaderAttributeType::INT16] = VK_FORMAT_R16_SINT;
			types[(uint32)ShaderAttributeType::UINT16] = VK_FORMAT_R16_UINT;
			types[(uint32)ShaderAttributeType::INT32] = VK_FORMAT_R32_SINT;
			types[(uint32)ShaderAttributeType::UINT32] = VK_FORMAT_R32_UINT;
		}

		// Process attributes
		uint32 offset = 0;
		for (uint32 i = 0; i < shader->attributes.count; ++i)
		{
			// Setup the new attribute.
			VkVertexInputAttributeDescription attribute;
			attribute.location = i;
			attribute.binding = 0;
			attribute.offset = offset;
			attribute.format = types[(uint32)shader->attributes[i].type];

			// Push into the config's attribute collection and add to the stride.
			v_shader->config.attributes[i] = attribute;

			offset += shader->attributes[i].size;
		}

		// Descriptor pool.
		VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.poolSizeCount = 2;
		pool_info.pPoolSizes = v_shader->config.pool_sizes;
		pool_info.maxSets = v_shader->config.max_descriptor_set_count;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		// Create descriptor pool.
		VkResult result = vkCreateDescriptorPool(logical_device, &pool_info, vk_allocator, &v_shader->descriptor_pool);
		if (!vk_result_is_success(result))
		{
			SHMERRORV("vulkan_shader_initialize failed creating descriptor pool: '%s'", vk_result_string(result, true));
			return false;
		}

		// Create descriptor set layouts.
		for (uint32 i = 0; i < v_shader->config.descriptor_set_count; ++i)
		{
			VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layout_info.bindingCount = v_shader->config.descriptor_sets[i].binding_count;
			layout_info.pBindings = v_shader->config.descriptor_sets[i].bindings;
			result = vkCreateDescriptorSetLayout(logical_device, &layout_info, vk_allocator, &v_shader->descriptor_set_layouts[i]);
			if (!vk_result_is_success(result))
			{
				SHMERRORV("vulkan_shader_initialize failed creating descriptor pool: '%s'", vk_result_string(result, true));
				return false;
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

		VkPipelineShaderStageCreateInfo stage_create_infos[RendererConfig::shader_max_stages] = {};
		for (uint32 i = 0; i < v_shader->config.stage_count; ++i)
		{
			stage_create_infos[i] = v_shader->stages[i].shader_stage_create_info;
		}

		/*VulkanPipelineConfig p_config = {};
		p_config.renderpass = v_shader->renderpass;
		p_config.vertex_stride = shader->attribute_stride;
		p_config.attribute_count = shader->attributes.count;
		p_config.attribute_descriptions = v_shader->config.attributes;
		p_config.descriptor_set_layout_count = v_shader->config.descriptor_set_count;
		p_config.descriptor_set_layouts = v_shader->descriptor_set_layouts;
		p_config.stage_count = v_shader->config.stage_count;
		p_config.stages = stage_create_infos;
		p_config.viewport = viewport;
		p_config.scissor = scissor;
		p_config.cull_mode = v_shader->config.cull_mode;
		p_config.is_wireframe = false;
		p_config.shader_flags = shader->shader_flags;
		p_config.push_constant_range_count = shader->push_constant_range_count;
		p_config.push_constant_ranges = shader->push_constant_ranges;

		bool32 pipeline_result = vk_pipeline_create(&p_config, &v_shader->pipeline);

		if (!pipeline_result)
		{
			SHMERROR("Failed to load graphics pipeline for object shader.");
			return false;
		}*/

		v_shader->pipelines.init((uint32)VulkanTopologyCLass::TOPOLOGY_CLASS_COUNT, 0);

		RenderTopologyTypeFlags::Value pipeline_topologies[(uint32)VulkanTopologyCLass::TOPOLOGY_CLASS_COUNT];

		pipeline_topologies[(uint32)VulkanTopologyCLass::POINT] = RenderTopologyTypeFlags::POINT_LIST;
		pipeline_topologies[(uint32)VulkanTopologyCLass::LINE] = RenderTopologyTypeFlags::LINE_LIST;
		pipeline_topologies[(uint32)VulkanTopologyCLass::LINE] |= RenderTopologyTypeFlags::LINE_STRIP;
		pipeline_topologies[(uint32)VulkanTopologyCLass::TRIANGLE] = RenderTopologyTypeFlags::TRIANGLE_LIST;
		pipeline_topologies[(uint32)VulkanTopologyCLass::TRIANGLE] |= RenderTopologyTypeFlags::TRIANGLE_STRIP;
		pipeline_topologies[(uint32)VulkanTopologyCLass::TRIANGLE] |= RenderTopologyTypeFlags::TRIANGLE_FAN;

		if (shader->topologies & RenderTopologyTypeFlags::POINT_LIST)
			v_shader->pipelines[(uint32)VulkanTopologyCLass::POINT] = (VulkanPipeline*)Memory::allocate(sizeof(VulkanPipeline), AllocationTag::RENDERER);

		if (shader->topologies & RenderTopologyTypeFlags::LINE_LIST || shader->topologies & RenderTopologyTypeFlags::LINE_STRIP)
			v_shader->pipelines[(uint32)VulkanTopologyCLass::LINE] = (VulkanPipeline*)Memory::allocate(sizeof(VulkanPipeline), AllocationTag::RENDERER);

		if (shader->topologies & RenderTopologyTypeFlags::TRIANGLE_LIST || shader->topologies & RenderTopologyTypeFlags::TRIANGLE_STRIP || shader->topologies & RenderTopologyTypeFlags::TRIANGLE_FAN)
			v_shader->pipelines[(uint32)VulkanTopologyCLass::TRIANGLE] = (VulkanPipeline*)Memory::allocate(sizeof(VulkanPipeline), AllocationTag::RENDERER);

		v_shader->bound_pipeline_id = INVALID_ID;
		for (uint32 i = 0; i < v_shader->pipelines.capacity; i++)
		{
			if (!v_shader->pipelines[i])
				continue;

			VulkanPipelineConfig p_config = {};
			p_config.renderpass = v_shader->renderpass;
			p_config.vertex_stride = shader->attribute_stride;
			p_config.attribute_count = shader->attributes.count;
			p_config.attribute_descriptions = v_shader->config.attributes;
			p_config.descriptor_set_layout_count = v_shader->config.descriptor_set_count;
			p_config.descriptor_set_layouts = v_shader->descriptor_set_layouts;
			p_config.stage_count = v_shader->config.stage_count;
			p_config.stages = stage_create_infos;
			p_config.viewport = viewport;
			p_config.scissor = scissor;
			p_config.cull_mode = v_shader->config.cull_mode;
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

			if (v_shader->bound_pipeline_id == INVALID_ID)
			{
				v_shader->bound_pipeline_id = i;

				for (RenderTopologyTypeFlags::Value type = 1; type < RenderTopologyTypeFlags::ALL_TYPES_MASK; type = type << 1) 
				{
					if (v_shader->pipelines[i]->topologies & type) 
					{

						switch (type) {
						case RenderTopologyTypeFlags::POINT_LIST:
							v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
						case RenderTopologyTypeFlags::LINE_LIST:
							v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
						case RenderTopologyTypeFlags::LINE_STRIP:
							v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
						case RenderTopologyTypeFlags::TRIANGLE_LIST:
							v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
						case RenderTopologyTypeFlags::TRIANGLE_STRIP:
							v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
						case RenderTopologyTypeFlags::TRIANGLE_FAN:
							v_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
						default:
							SHMWARNV("primitive topology '%u' not supported. Skipping.", type); break;
						}

						break;
					}
				}
			}	
		}

		if (v_shader->bound_pipeline_id == INVALID_ID)
		{
			SHMERROR("No available topology classes are available, so a pipeline cannot be bound. Check shader configuration.");
			return false;
		}

		// Map the entire buffer's memory.
		v_shader->mapped_uniform_buffer = vk_buffer_map_memory(&shader->uniform_buffer, 0, VK_WHOLE_SIZE);

		// Allocate global descriptor sets, one per frame. Global is always the first set.
		VkDescriptorSetLayout global_layouts[3] =
		{
			v_shader->descriptor_set_layouts[desc_set_index_global],
			v_shader->descriptor_set_layouts[desc_set_index_global],
			v_shader->descriptor_set_layouts[desc_set_index_global]
		};

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = v_shader->descriptor_pool;
		alloc_info.descriptorSetCount = 3;
		alloc_info.pSetLayouts = global_layouts;
		VK_CHECK(vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, v_shader->global_descriptor_sets));

		return true;

	}

	bool32 vk_shader_use(Shader* s)
	{
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		VulkanCommandBuffer* command_buffer = &context->graphics_command_buffers[context->bound_framebuffer_index];

		vk_pipeline_bind(&context->graphics_command_buffers[context->bound_framebuffer_index], VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipelines[v_shader->bound_pipeline_id]);
		vkCmdSetPrimitiveTopology(command_buffer->handle, v_shader->current_topology);
		return true;
	}

	bool32 vk_shader_bind_globals(Shader* s)
	{
		if (!s)
		{
			return false;
		}

		uint32 image_index = context->bound_framebuffer_index;
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		VkCommandBuffer command_buffer = context->graphics_command_buffers[image_index].handle;
		VkDescriptorSet global_descriptor = v_shader->global_descriptor_sets[image_index];

		s->bound_ubo_offset = s->global_ubo_offset;
		// Bind the global descriptor set to be updated.
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipelines[v_shader->bound_pipeline_id]->layout, 0, 1, &global_descriptor, 0, 0);
		return true;
	}

	bool32 vk_shader_bind_instance(Shader* s, uint32 instance_id)
	{

		if (!s)
		{
			SHMERROR("vulkan_shader_bind_instance requires a valid pointer to a shader.");
			return false;
		}
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		uint32 image_index = context->bound_framebuffer_index;
		VkCommandBuffer command_buffer = context->graphics_command_buffers[image_index].handle;
		
		s->bound_instance_id = instance_id;
		VulkanShaderInstanceState* instance_state = &v_shader->instance_states[instance_id];
		s->bound_ubo_offset = (uint32)instance_state->offset;

		VulkanShaderInstanceState* object_state = &v_shader->instance_states[s->bound_instance_id];
		VkDescriptorSet object_descriptor_set = object_state->descriptor_set_state.descriptor_sets[image_index];

		// Bind the descriptor set to be updated, or in case the shader changed.
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipelines[v_shader->bound_pipeline_id]->layout, 1, 1, &object_descriptor_set, 0, 0);
		return true;

	}

	bool32 vk_shader_apply_globals(Shader* s)
	{

		OPTICK_EVENT();

		uint32 image_index = context->bound_framebuffer_index;
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;

		// Apply UBO first
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = ((VulkanBuffer*)s->uniform_buffer.internal_data.data)->handle;
		bufferInfo.offset = s->global_ubo_offset;
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

		uint32 global_set_binding_count = v_shader->config.descriptor_sets[desc_set_index_global].binding_count;
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

	bool32 vk_shader_apply_instance(Shader* s, bool32 needs_update)
	{
		
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		if (v_shader->instance_uniform_count < 1 && v_shader->instance_uniform_sampler_count < 1)
		{
			SHMERROR("This shader does not use instances.");
			return false;
		}
		uint32 image_index = context->bound_framebuffer_index;
		VkCommandBuffer command_buffer = context->graphics_command_buffers[image_index].handle;

		// Obtain instance data.
		VulkanShaderInstanceState* object_state = &v_shader->instance_states[s->bound_instance_id];
		VkDescriptorSet object_descriptor_set = object_state->descriptor_set_state.descriptor_sets[image_index];

		// TODO: if needs update
		if (needs_update)
		{
			VkWriteDescriptorSet descriptor_writes[2] = {};  // Always a max of 2 descriptor sets.
			uint32 descriptor_count = 0;
			uint32 descriptor_index = 0;

			// Descriptor 0 - Uniform buffer
			// Only do this if the descriptor has not yet been updated.
			uint8* instance_ubo_generation = &(object_state->descriptor_set_state.descriptor_states[descriptor_index].generations[image_index]);

			VkDescriptorBufferInfo buffer_info;
			buffer_info.buffer = ((VulkanBuffer*)s->uniform_buffer.internal_data.data)->handle;
			buffer_info.offset = object_state->offset;
			buffer_info.range = s->ubo_stride;

			VkWriteDescriptorSet ubo_descriptor = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			ubo_descriptor.dstSet = object_descriptor_set;
			ubo_descriptor.dstBinding = descriptor_index;
			ubo_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubo_descriptor.descriptorCount = 1;
			ubo_descriptor.pBufferInfo = &buffer_info;

			// TODO: determine if update is required.
			if (v_shader->instance_uniform_count)
			{	
				if (*instance_ubo_generation == INVALID_ID8 /*|| *global_ubo_generation != material->generation*/)
				{
					descriptor_writes[descriptor_count] = ubo_descriptor;
					descriptor_count++;

					// Update the frame generation. In this case it is only needed once since this is a buffer.
					*instance_ubo_generation = 1;  // material->generation; TODO: some generation from... somewhere
				}
				descriptor_index++;
			}

			// Samplers will always be in the binding. If the binding count is less than 2, there are no samplers.
			VkDescriptorImageInfo image_infos[RendererConfig::shader_max_instance_textures];
			if (v_shader->instance_uniform_sampler_count)
			{
				uint8 sampler_binding_index = v_shader->config.descriptor_sets[desc_set_index_instance].sampler_binding_index;
				uint32 total_sampler_count = v_shader->config.descriptor_sets[desc_set_index_instance].bindings[sampler_binding_index].descriptorCount;
				uint32 update_sampler_count = 0;
				for (uint32 i = 0; i < total_sampler_count; ++i)
				{
					// TODO: only update in the list if actually needing an update.
					TextureMap* map = v_shader->instance_states[s->bound_instance_id].instance_texture_maps[i];
					Texture* t = map->texture;

					if (t->generation == INVALID_ID)
						t = TextureSystem::get_default_texture();

					VulkanImage* image = (VulkanImage*)t->internal_data.data;
					image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					image_infos[i].imageView = image->view;
					image_infos[i].sampler = (VkSampler)map->internal_data;

					// TODO: change up descriptor state to handle this properly.
					// Sync frame generation if not using a default texture.
					// if (t->generation != INVALID_ID) {
					//     *descriptor_generation = t->generation;
					//     *descriptor_id = t->id;
					// }		
					// 	

					update_sampler_count++;
				}

				VkWriteDescriptorSet sampler_descriptor = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				sampler_descriptor.dstSet = object_descriptor_set;
				sampler_descriptor.dstBinding = descriptor_index;
				sampler_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				sampler_descriptor.descriptorCount = update_sampler_count;
				sampler_descriptor.pImageInfo = image_infos;

				descriptor_writes[descriptor_count] = sampler_descriptor;
				descriptor_count++;
			}

			
			if (descriptor_count > 0)
			{
				vkUpdateDescriptorSets(context->device.logical_device, descriptor_count, descriptor_writes, 0, 0);
			}

		}

		//// Bind the descriptor set to be updated, or in case the shader changed.
		//vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipelines[v_shader->bound_pipeline_id]->layout, 1, 1, &object_descriptor_set, 0, 0);
		return true;

	}

	bool32 vk_shader_acquire_instance_resources(Shader* s, uint32 maps_count, TextureMap** maps, uint32* out_instance_id)
	{

		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		// TODO: dynamic
		*out_instance_id = INVALID_ID;
		for (uint32 i = 0; i < 1024; ++i)
		{
			if (v_shader->instance_states[i].id == INVALID_ID)
			{
				v_shader->instance_states[i].id = i;
				*out_instance_id = i;
				break;
			}
		}
		if (*out_instance_id == INVALID_ID)
		{
			SHMERROR("vulkan_shader_acquire_instance_resources failed to acquire new id");
			return false;
		}

		VulkanShaderInstanceState* instance_state = &v_shader->instance_states[*out_instance_id];
		uint8 sampler_binding_index = v_shader->config.descriptor_sets[desc_set_index_instance].sampler_binding_index;
		uint32 instance_texture_count = v_shader->config.descriptor_sets[desc_set_index_instance].bindings[sampler_binding_index].descriptorCount;

		if (maps_count)
		{
			// Wipe out the memory for the entire array, even if it isn't all used.
			instance_state->instance_texture_maps.init(maps_count, true, AllocationTag::RENDERER);
			Texture* default_texture = TextureSystem::get_default_texture();
			// Set all the texture pointers to default until assigned.
			for (uint32 i = 0; i < maps_count; ++i)
			{
				instance_state->instance_texture_maps[i] = maps[i];
				if (!maps[i]->texture)
					instance_state->instance_texture_maps[i]->texture = default_texture;
			}
		}	

		// Allocate some space in the UBO - by the stride, not the size.
		uint64 size = s->ubo_stride;
		if (size > 0)
		{
			if (!renderbuffer_allocate(&s->uniform_buffer, size, &instance_state->offset))
			{
				SHMERROR("vulkan_material_shader_acquire_resources failed to acquire ubo space");
				return false;
			}
		}
		
		VulkanShaderDescriptorSetState* set_state = &instance_state->descriptor_set_state;

		// Each descriptor binding in the set
		uint32 binding_count = v_shader->config.descriptor_sets[desc_set_index_instance].binding_count;

		for (uint32 i = 0; i < binding_count; ++i)
		{
			for (uint32 j = 0; j < 3; ++j)
			{
				set_state->descriptor_states[i].generations[j] = INVALID_ID8;
				set_state->descriptor_states[i].ids[j] = INVALID_ID;
			}
		}

		// Allocate 3 descriptor sets (one per frame).
		VkDescriptorSetLayout layouts[Renderer::RendererConfig::framebuffer_count] =
		{
			v_shader->descriptor_set_layouts[desc_set_index_instance],
			v_shader->descriptor_set_layouts[desc_set_index_instance],
			v_shader->descriptor_set_layouts[desc_set_index_instance]
		};

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = v_shader->descriptor_pool;
		alloc_info.descriptorSetCount = 3;
		alloc_info.pSetLayouts = layouts;
		VkResult result = vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, instance_state->descriptor_set_state.descriptor_sets);
		if (result != VK_SUCCESS)
		{
			SHMERRORV("Error allocating instance descriptor sets in shader: '%s'.", vk_result_string(result, true));
			return false;
		}

		return true;

	}

	bool32 vk_shader_release_instance_resources(Shader* s, uint32 instance_id)
	{

		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		VulkanShaderInstanceState* instance_state = &v_shader->instance_states[instance_id];

		// Wait for any pending operations using the descriptor set to finish.
		vkDeviceWaitIdle(context->device.logical_device);

		// Free 3 descriptor sets (one per frame)
		VkResult result = vkFreeDescriptorSets(
			context->device.logical_device,
			v_shader->descriptor_pool,
			3,
			instance_state->descriptor_set_state.descriptor_sets);
		if (result != VK_SUCCESS)
		{
			SHMERROR("Error freeing object shader descriptor sets!");
		}

		instance_state->instance_texture_maps.free_data();

		renderbuffer_free(&s->uniform_buffer, instance_state->offset);
		instance_state->offset = INVALID_ID64;
		instance_state->id = INVALID_ID;

		return true;

	}

	bool32 vk_shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value)
	{

		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		if (uniform->type == ShaderUniformType::SAMPLER)
		{
			if (uniform->scope == ShaderScope::GLOBAL)
			{
				s->global_texture_maps[uniform->location] = (TextureMap*)value;
			}
			else
			{
				v_shader->instance_states[s->bound_instance_id].instance_texture_maps[uniform->location] = (TextureMap*)value;
			}
		}
		else
		{
			if (uniform->scope == ShaderScope::LOCAL)
			{
				// Is local, using push constants. Do this immediately.
				VkCommandBuffer command_buffer = context->graphics_command_buffers[context->bound_framebuffer_index].handle;
				vkCmdPushConstants(command_buffer, v_shader->pipelines[v_shader->bound_pipeline_id]->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, uniform->offset, uniform->size, value);
			}
			else
			{
				// Map the appropriate memory location and copy the data over.
				uint64 addr = (uint64)v_shader->mapped_uniform_buffer;
				addr += s->bound_ubo_offset + uniform->offset;
				Memory::copy_memory(value, (void*)addr, uniform->size);
			}
		}
		return true;
	}

	bool32 create_shader_module(VulkanShader* shader, const VulkanShaderStageConfig& config, VulkanShaderStage* shader_stage)
	{
		// Read the resource.
		Buffer data = {};
		if (!ResourceSystem::generic_loader_load(config.filename, 0, &data))
		{
			SHMERRORV("Unable to read shader module: %s.", config.filename);
			return false;
		}

		shader_stage->module_create_info = {};
		shader_stage->module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		// Use the resource's size and data directly.
		shader_stage->module_create_info.codeSize = data.size;
		shader_stage->module_create_info.pCode = (uint32*)data.data;

		VK_CHECK(vkCreateShaderModule(
			context->device.logical_device,
			&shader_stage->module_create_info,
			context->allocator_callbacks,
			&shader_stage->handle));

		// Release the resource.
		ResourceSystem::generic_loader_unload(&data);

		// Shader stage info
		shader_stage->shader_stage_create_info = {};
		shader_stage->shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage->shader_stage_create_info.stage = config.stage;
		shader_stage->shader_stage_create_info.module = shader_stage->handle;
		shader_stage->shader_stage_create_info.pName = "main";

		return true;
	}

	static VkSamplerAddressMode convert_repeat_type(TextureRepeat::Value repeat) {
		switch (repeat) {
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

	static VkFilter convert_filter_type(TextureFilter::Value filter) {
		switch (filter) {
		case TextureFilter::NEAREST:
			return VK_FILTER_NEAREST;
		case TextureFilter::LINEAR:
			return VK_FILTER_LINEAR;
		default:
			SHMWARNV("convert_filter_type: Unsupported filter type %u, defaulting to linear.", (uint32)filter);
			return VK_FILTER_LINEAR;
		}
	}

	bool32 vk_texture_map_acquire_resources(TextureMap* out_map) 
	{
		// Create a sampler for the texture
		VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

		sampler_info.minFilter = convert_filter_type(out_map->filter_minify);
		sampler_info.magFilter = convert_filter_type(out_map->filter_magnify);

		sampler_info.addressModeU = convert_repeat_type(out_map->repeat_u);
		sampler_info.addressModeV = convert_repeat_type(out_map->repeat_v);
		sampler_info.addressModeW = convert_repeat_type(out_map->repeat_w);

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

		VkResult result = vkCreateSampler(context->device.logical_device, &sampler_info, context->allocator_callbacks, (VkSampler*)&out_map->internal_data);
		if (!vk_result_is_success(VK_SUCCESS)) {
			SHMERRORV("Error creating texture sampler: %s", vk_result_string(result, true));
			return false;
		}

		VK_DEBUG_SET_OBJECT_NAME(context, VK_OBJECT_TYPE_SAMPLER, (VkSampler)out_map->internal_data, out_map->texture->name);

		return true;
	}

	void vk_texture_map_release_resources(TextureMap* map) {
		if (map) {
			vkDeviceWaitIdle(context->device.logical_device);
			vkDestroySampler(context->device.logical_device, (VkSampler)map->internal_data, context->allocator_callbacks);
			map->internal_data = 0;
		}
	}

}