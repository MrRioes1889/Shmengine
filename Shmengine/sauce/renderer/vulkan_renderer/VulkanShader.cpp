#include "VulkanShader.hpp"
#include "VulkanBackend.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanUtils.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/TextureSystem.hpp"

static VulkanContext* context = 0;

namespace Renderer::Vulkan
{

	static bool32 create_module(VulkanShader* shader, const VulkanShaderStageConfig& config, VulkanShaderStage* shader_stage);

	static const uint32 desc_set_index_global = 0;
	static const uint32 desc_set_index_instance = 1;
	static const uint32 binding_index_ubo = 0;
	static const uint32 binding_index_sampler = 1;

	void shaders_init_context(VulkanContext* c)
	{
		context = c;
	}

	bool32 shader_create(Shader* shader, uint8 renderpass_id, uint8 stage_count, const Darray<String>& stage_filenames, ShaderStage::Value* stages)
	{

		shader->internal_data = Memory::allocate(sizeof(VulkanShader), true, AllocationTag::MAIN);

		VulkanRenderpass* renderpass = renderpass_id == 1 ? &context->world_renderpass : &context->ui_renderpass;

		VkShaderStageFlags vk_stages[VulkanConfig::shader_max_stages];
		for (uint8 i = 0; i < stage_count; ++i) {
			switch (stages[i]) {
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
				SHMWARNV("Unsupported stage type: %i", stages[i]);
				break;
			}
		}

		VulkanShader* out_shader = (VulkanShader*)shader->internal_data;
		out_shader->renderpass = renderpass;
		out_shader->config.max_descriptor_set_count = VulkanConfig::shader_max_instances;

		out_shader->config.stage_count = 0;

		for (uint32 i = 0; i < stage_count; i++) {

			if (out_shader->config.stage_count + 1 > VulkanConfig::shader_max_stages) {
				SHMERRORV("Shaders may have a maximum of %i stages", VulkanConfig::shader_max_stages);
				return false;
			}

			VkShaderStageFlagBits stage_flag;
			switch (stages[i]) {
			case ShaderStage::VERTEX:
				stage_flag = VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case ShaderStage::FRAGMENT:
				stage_flag = VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			default:
				SHMERRORV("vulkan_shader_create: Unsupported shader stage flagged: %i. Stage ignored.", stages[i]);
				continue;
			}

			out_shader->config.stages[out_shader->config.stage_count].stage = stage_flag;
			CString::copy(VulkanShaderStageConfig::max_filename_length, out_shader->config.stages[out_shader->config.stage_count].filename, stage_filenames[i].c_str());
			out_shader->config.stage_count++;
		}

		// TODO: Replace
		out_shader->config.pool_sizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 };
		out_shader->config.pool_sizes[1] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 };

		VulkanDescriptorSetConfig global_descriptor_set_config = {};

		global_descriptor_set_config.bindings[binding_index_ubo].binding = binding_index_ubo;
		global_descriptor_set_config.bindings[binding_index_ubo].descriptorCount = 1;
		global_descriptor_set_config.bindings[binding_index_ubo].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		global_descriptor_set_config.bindings[binding_index_ubo].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		global_descriptor_set_config.binding_count++;

		out_shader->config.descriptor_sets[desc_set_index_global] = global_descriptor_set_config;
		out_shader->config.descriptor_set_count++;

		if (shader->use_instances) {
			// If using instances, add a second descriptor set.
			VulkanDescriptorSetConfig instance_descriptor_set_config = {};

			// Add a UBO to it, as instances should always have one available.
			// NOTE: Might be a good idea to only add this if it is going to be used...
			instance_descriptor_set_config.bindings[binding_index_ubo].binding = binding_index_ubo;
			instance_descriptor_set_config.bindings[binding_index_ubo].descriptorCount = 1;
			instance_descriptor_set_config.bindings[binding_index_ubo].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			instance_descriptor_set_config.bindings[binding_index_ubo].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			instance_descriptor_set_config.binding_count++;

			out_shader->config.descriptor_sets[desc_set_index_instance] = instance_descriptor_set_config;
			out_shader->config.descriptor_set_count++;
		}

		// TODO: dynamic
		for (uint32 i = 0; i < VulkanConfig::shader_max_instances; ++i) {
			out_shader->instance_states[i].id = INVALID_ID;
		}

		return true;

	}

	void shader_destroy(Shader* shader)
	{

		if (!shader->internal_data)
			return;

		VulkanShader* s = (VulkanShader*)shader->internal_data;
		if (!s)
		{
			SHMERROR("vulkan_renderer_shader_destroy requires a valid pointer to a shader.");
			return;
		}

		VkDevice logical_device = context->device.logical_device;
		VkAllocationCallbacks* vk_allocator = context->allocator_callbacks;

		for (uint32 i = 0; i < s->config.descriptor_set_count; ++i)
		{
			if (s->descriptor_set_layouts[i]) {
				vkDestroyDescriptorSetLayout(logical_device, s->descriptor_set_layouts[i], vk_allocator);
				s->descriptor_set_layouts[i] = 0;
			}
		}

		if (s->descriptor_pool)
		{
			vkDestroyDescriptorPool(logical_device, s->descriptor_pool, vk_allocator);
		}

		buffer_unlock_memory(context, &s->uniform_buffer);
		s->mapped_uniform_buffer = 0;
		buffer_destroy(context, &s->uniform_buffer);

		// Pipeline
		pipeline_destroy(context, &s->pipeline);

		// Shader modules
		for (uint32 i = 0; i < s->config.stage_count; ++i)
		{
			vkDestroyShaderModule(context->device.logical_device, s->stages[i].handle, context->allocator_callbacks);
		}

		// Free the internal data memory.
		Memory::free_memory(shader->internal_data, true, AllocationTag::MAIN);
		shader->internal_data = 0;

	}

	bool32 shader_init(Shader* shader)
	{

		VkDevice& logical_device = context->device.logical_device;
		VkAllocationCallbacks* vk_allocator = context->allocator_callbacks;
		VulkanShader* s = (VulkanShader*)shader->internal_data;

		// Create a module for each stage.
		for (uint32 i = 0; i < s->config.stage_count; ++i)
		{
			if (!create_module(s, s->config.stages[i], &s->stages[i]))
			{
				SHMERRORV("Unable to create %s shader module for '%s'. Shader will be destroyed.", s->config.stages[i].filename, shader->name.c_str());
				return false;
			}
		}

		// Static lookup table for our types->Vulkan ones.
		static VkFormat* types = 0;
		static VkFormat t[11];
		if (!types)
		{
			t[(uint32)ShaderAttributeType::FLOAT32] = VK_FORMAT_R32_SFLOAT;
			t[(uint32)ShaderAttributeType::FLOAT32_2] = VK_FORMAT_R32G32_SFLOAT;
			t[(uint32)ShaderAttributeType::FLOAT32_3] = VK_FORMAT_R32G32B32_SFLOAT;
			t[(uint32)ShaderAttributeType::FLOAT32_4] = VK_FORMAT_R32G32B32A32_SFLOAT;
			t[(uint32)ShaderAttributeType::INT8] = VK_FORMAT_R8_SINT;
			t[(uint32)ShaderAttributeType::UINT8] = VK_FORMAT_R8_UINT;
			t[(uint32)ShaderAttributeType::INT16] = VK_FORMAT_R16_SINT;
			t[(uint32)ShaderAttributeType::UINT16] = VK_FORMAT_R16_UINT;
			t[(uint32)ShaderAttributeType::INT32] = VK_FORMAT_R32_SINT;
			t[(uint32)ShaderAttributeType::UINT32] = VK_FORMAT_R32_UINT;
			types = t;
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
			s->config.attributes[i] = attribute;

			offset += shader->attributes[i].size;
		}

		// Process uniforms.
		for (uint32 i = 0; i < shader->uniforms.count; ++i)
		{
			// For samplers, the descriptor bindings need to be updated. Other types of uniforms don't need anything to be done here.
			if (shader->uniforms[i].type == ShaderUniformType::SAMPLER)
			{
				const uint32 set_index = (shader->uniforms[i].scope == ShaderScope::GLOBAL ? desc_set_index_global : desc_set_index_instance);
				VulkanDescriptorSetConfig* set_config = &s->config.descriptor_sets[set_index];
				if (set_config->binding_count < 2)
				{
					// There isn't a binding yet, meaning this is the first sampler to be added.
					// Create the binding with a single descriptor for this sampler.
					set_config->bindings[binding_index_sampler].binding = binding_index_sampler;  // Always going to be the second one.
					set_config->bindings[binding_index_sampler].descriptorCount = 1;              // Default to 1, will increase with each sampler added to the appropriate level.
					set_config->bindings[binding_index_sampler].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					set_config->bindings[binding_index_sampler].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
					set_config->binding_count++;
				}
				else
				{
					// There is already a binding for samplers, so just add a descriptor to it.
					// Take the current descriptor count as the location and increment the number of descriptors.
					set_config->bindings[binding_index_sampler].descriptorCount++;
				}
			}
		}

		// Descriptor pool.
		VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pool_info.poolSizeCount = 2;
		pool_info.pPoolSizes = s->config.pool_sizes;
		pool_info.maxSets = s->config.max_descriptor_set_count;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		// Create descriptor pool.
		VkResult result = vkCreateDescriptorPool(logical_device, &pool_info, vk_allocator, &s->descriptor_pool);
		if (!vulkan_result_is_success(result))
		{
			SHMERRORV("vulkan_shader_initialize failed creating descriptor pool: '%s'", vulkan_result_string(result, true));
			return false;
		}

		// Create descriptor set layouts.
		for (uint32 i = 0; i < s->config.descriptor_set_count; ++i)
		{
			VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layout_info.bindingCount = s->config.descriptor_sets[i].binding_count;
			layout_info.pBindings = s->config.descriptor_sets[i].bindings;
			result = vkCreateDescriptorSetLayout(logical_device, &layout_info, vk_allocator, &s->descriptor_set_layouts[i]);
			if (!vulkan_result_is_success(result))
			{
				SHMERRORV("vulkan_shader_initialize failed creating descriptor pool: '%s'", vulkan_result_string(result, true));
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

		VkPipelineShaderStageCreateInfo stage_create_infos[VulkanConfig::shader_max_stages] = {};
		for (uint32 i = 0; i < s->config.stage_count; ++i)
		{
			stage_create_infos[i] = s->stages[i].shader_stage_create_info;
		}

		bool32 pipeline_result = pipeline_create(
			context,
			s->renderpass,
			shader->attribute_stride,
			shader->attributes.count,
			s->config.attributes,  // shader->attributes,
			s->config.descriptor_set_count,
			s->descriptor_set_layouts,
			s->config.stage_count,
			stage_create_infos,
			viewport,
			scissor,
			false,
			true,
			shader->push_constant_range_count,
			shader->push_constant_ranges,
			&s->pipeline);

		if (!pipeline_result)
		{
			SHMERROR("Failed to load graphics pipeline for object shader.");
			return false;
		}

		// Grab the UBO alignment requirement from the device.
		shader->required_ubo_alignment = context->device.properties.limits.minUniformBufferOffsetAlignment;

		// Make sure the UBO is aligned according to device requirements.
		shader->global_ubo_stride = (uint32)get_aligned(shader->global_ubo_size, shader->required_ubo_alignment);
		shader->ubo_stride = (uint32)get_aligned(shader->ubo_size, shader->required_ubo_alignment);

		// Uniform  buffer.
		uint32 device_local_bits = context->device.supports_device_local_host_visible ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
		// TODO: max count should be configurable, or perhaps long term support of buffer resizing.
		uint64 total_buffer_size = shader->global_ubo_stride + (shader->ubo_stride * VulkanConfig::max_material_count);  // global + (locals)
		if (!buffer_create(
			context,
			total_buffer_size,
			(VkBufferUsageFlagBits)(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | device_local_bits,
			true,
			true,
			&s->uniform_buffer))
		{
			SHMERROR("Vulkan buffer creation failed for object shader.");
			return false;
		}

		// Allocate space for the global UBO, whcih should occupy the _stride_ space, _not_ the actual size used.
		if (!buffer_allocate(&s->uniform_buffer, shader->global_ubo_stride, &shader->global_ubo_offset))
		{
			SHMERROR("Failed to allocate space for the uniform buffer!");
			return false;
		}

		// Map the entire buffer's memory.
		s->mapped_uniform_buffer = buffer_lock_memory(context, &s->uniform_buffer, 0, VK_WHOLE_SIZE /*total_buffer_size*/, 0);

		// Allocate global descriptor sets, one per frame. Global is always the first set.
		VkDescriptorSetLayout global_layouts[3] =
		{
			s->descriptor_set_layouts[desc_set_index_global],
			s->descriptor_set_layouts[desc_set_index_global],
			s->descriptor_set_layouts[desc_set_index_global]
		};

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = s->descriptor_pool;
		alloc_info.descriptorSetCount = 3;
		alloc_info.pSetLayouts = global_layouts;
		VK_CHECK(vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, s->global_descriptor_sets));

		return true;

	}

	bool32 shader_use(Shader* s)
	{
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		pipeline_bind(&context->graphics_command_buffers[context->image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, &v_shader->pipeline);
		return true;
	}

	bool32 shader_bind_globals(Shader* s)
	{
		if (!s)
		{
			return false;
		}

		// Global UBO is always at the beginning, but use this anyway.
		s->bound_ubo_offset = s->global_ubo_offset;
		return true;
	}

	bool32 shader_bind_instance(Shader* s, uint32 instance_id)
	{

		if (!s)
		{
			SHMERROR("vulkan_shader_bind_instance requires a valid pointer to a shader.");
			return false;
		}
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;

		s->bound_instance_id = instance_id;
		VulkanShaderInstanceState* instance_state = &v_shader->instance_states[instance_id];
		s->bound_ubo_offset = (uint32)instance_state->offset;
		return true;

	}

	bool32 shader_apply_globals(Shader* s)
	{

		uint32 image_index = context->image_index;
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		VkCommandBuffer command_buffer = context->graphics_command_buffers[image_index].handle;
		VkDescriptorSet global_descriptor = v_shader->global_descriptor_sets[image_index];

		// Apply UBO first
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = v_shader->uniform_buffer.handle;
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

		// Bind the global descriptor set to be updated.
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipeline.layout, 0, 1, &global_descriptor, 0, 0);
		return true;

	}

	bool32 shader_apply_instance(Shader* s, bool32 needs_update)
	{

		if (!s->use_instances)
		{
			SHMERROR("This shader does not use instances.");
			return false;
		}
		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		uint32 image_index = context->image_index;
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
			buffer_info.buffer = v_shader->uniform_buffer.handle;
			buffer_info.offset = object_state->offset;
			buffer_info.range = s->ubo_stride;

			VkWriteDescriptorSet ubo_descriptor = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			ubo_descriptor.dstSet = object_descriptor_set;
			ubo_descriptor.dstBinding = descriptor_index;
			ubo_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubo_descriptor.descriptorCount = 1;
			ubo_descriptor.pBufferInfo = &buffer_info;

			// TODO: determine if update is required.
			if (*instance_ubo_generation == INVALID_ID8 /*|| *global_ubo_generation != material->generation*/)
			{
				descriptor_writes[descriptor_count] = ubo_descriptor;
				descriptor_count++;

				// Update the frame generation. In this case it is only needed once since this is a buffer.
				*instance_ubo_generation = 1;  // material->generation; TODO: some generation from... somewhere
			}
			descriptor_index++;

			// Samplers will always be in the binding. If the binding count is less than 2, there are no samplers.
			VkDescriptorImageInfo image_infos[VulkanConfig::shader_max_instance_textures];
			if (v_shader->config.descriptor_sets[desc_set_index_instance].binding_count > 1)
			{
				// Iterate samplers.
				uint32 total_sampler_count = v_shader->config.descriptor_sets[desc_set_index_instance].bindings[binding_index_sampler].descriptorCount;
				uint32 update_sampler_count = 0;
				for (uint32 i = 0; i < total_sampler_count; ++i)
				{
					// TODO: only update in the list if actually needing an update.
					Texture* t = v_shader->instance_states[s->bound_instance_id].instance_textures[i];

					VulkanTextureData* internal_data = (VulkanTextureData*)t->internal_data.data;
					image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					image_infos[i].imageView = internal_data->image.view;
					image_infos[i].sampler = internal_data->sampler;

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

		// Bind the descriptor set to be updated, or in case the shader changed.
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, v_shader->pipeline.layout, 1, 1, &object_descriptor_set, 0, 0);
		return true;

	}

	bool32 shader_acquire_instance_resources(Shader* s, uint32* out_instance_id)
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
		uint32 instance_texture_count = v_shader->config.descriptor_sets[desc_set_index_instance].bindings[binding_index_sampler].descriptorCount;
		// Wipe out the memory for the entire array, even if it isn't all used.
		instance_state->instance_textures = (Texture**)Memory::allocate(sizeof(Texture*) * s->instance_texture_count, true, AllocationTag::MAIN);
		Texture* default_texture = TextureSystem::get_default_texture();
		// Set all the texture pointers to default until assigned.
		for (uint32 i = 0; i < instance_texture_count; ++i)
		{
			instance_state->instance_textures[i] = default_texture;
		}

		// Allocate some space in the UBO - by the stride, not the size.
		uint64 size = s->ubo_stride;
		if (!buffer_allocate(&v_shader->uniform_buffer, size, &instance_state->offset))
		{
			SHMERROR("vulkan_material_shader_acquire_resources failed to acquire ubo space");
			return false;
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
		VkDescriptorSetLayout layouts[3] =
		{
			v_shader->descriptor_set_layouts[desc_set_index_instance],
			v_shader->descriptor_set_layouts[desc_set_index_instance],
			v_shader->descriptor_set_layouts[desc_set_index_instance]
		};

		VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		alloc_info.descriptorPool = v_shader->descriptor_pool;
		alloc_info.descriptorSetCount = 3;
		alloc_info.pSetLayouts = layouts;
		VkResult result = vkAllocateDescriptorSets(
			context->device.logical_device,
			&alloc_info,
			instance_state->descriptor_set_state.descriptor_sets);
		if (result != VK_SUCCESS)
		{
			SHMERRORV("Error allocating instance descriptor sets in shader: '%s'.", vulkan_result_string(result, true));
			return false;
		}

		return true;

	}

	bool32 shader_release_instance_resources(Shader* s, uint32 instance_id)
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

		if (instance_state->instance_textures)
		{
			Memory::free_memory(instance_state->instance_textures, true, AllocationTag::MAIN);
			instance_state->instance_textures = 0;
		}

		buffer_free(&v_shader->uniform_buffer, instance_state->offset);
		instance_state->offset = INVALID_ID;
		instance_state->id = INVALID_ID;

		return true;

	}

	bool32 shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value)
	{

		VulkanShader* v_shader = (VulkanShader*)s->internal_data;
		if (uniform->type == ShaderUniformType::SAMPLER)
		{
			if (uniform->scope == ShaderScope::GLOBAL)
			{
				s->global_textures[uniform->location] = (Texture*)value;
			}
			else
			{
				v_shader->instance_states[s->bound_instance_id].instance_textures[uniform->location] = (Texture*)value;
			}
		}
		else
		{
			if (uniform->scope == ShaderScope::LOCAL)
			{
				// Is local, using push constants. Do this immediately.
				VkCommandBuffer command_buffer = context->graphics_command_buffers[context->image_index].handle;
				vkCmdPushConstants(command_buffer, v_shader->pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, uniform->offset, uniform->size, value);
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

	bool32 create_module(VulkanShader* shader, const VulkanShaderStageConfig& config, VulkanShaderStage* shader_stage)
	{
		// Read the resource.
		Resource resource;
		if (!ResourceSystem::load(config.filename, ResourceType::GENERIC, &resource))
		{
			SHMERRORV("Unable to read shader module: %s.", config.filename);
			return false;
		}

		shader_stage->module_create_info = {};
		shader_stage->module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		// Use the resource's size and data directly.
		shader_stage->module_create_info.codeSize = resource.data_size;
		shader_stage->module_create_info.pCode = (uint32*)resource.data;

		VK_CHECK(vkCreateShaderModule(
			context->device.logical_device,
			&shader_stage->module_create_info,
			context->allocator_callbacks,
			&shader_stage->handle));

		// Release the resource.
		ResourceSystem::unload(&resource);

		// Shader stage info
		shader_stage->shader_stage_create_info = {};
		shader_stage->shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage->shader_stage_create_info.stage = config.stage;
		shader_stage->shader_stage_create_info.module = shader_stage->handle;
		shader_stage->shader_stage_create_info.pName = "main";

		return true;
	}

}