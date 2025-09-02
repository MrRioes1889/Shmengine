#include "RendererFrontend.hpp"
#include "renderer/Utility.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "resources/loaders/ShaderLoader.hpp"

namespace Renderer
{
	extern SystemState* system_state;

	static bool8 _shader_init(ShaderConfig* config, Shader* out_shader);
	static void _shader_destroy(Shader* shader);

	static bool8 _add_attribute(Shader* shader, ShaderAttributeConfig* config, ShaderAttribute* out_attrib);
	static bool8 _add_uniform(Shader* shader, ShaderUniformId index, ShaderUniformConfig* config, uint32* global_sampler_counter, uint32* instance_sampler_counter, ShaderUniform* out_uniform);

	bool8 shader_init(ShaderConfig* config, Shader* out_shader)
	{
		if (out_shader->state >= ResourceState::Initialized)
			return false;

		out_shader->state = ResourceState::Initializing;
		goto_if(!_shader_init(config, out_shader), fail);
		out_shader->state = ResourceState::Initialized;

		return true;

	fail:
		shader_destroy(out_shader);
		return false;
	}

	bool8 shader_init_from_resource(const char* name, RenderPass* renderpass, Shader* out_shader)
	{
		if (out_shader->state >= ResourceState::Initialized)
			return false;

		out_shader->state = ResourceState::Initializing;

		ShaderResourceData resource = {};
		if (!ResourceSystem::shader_loader_load(name, &resource))
		{
			SHMERROR("Failed to load world pick shader config.");
			return false;
		}

		ShaderConfig config = ResourceSystem::shader_loader_get_config_from_resource(&resource, renderpass);
		goto_if(!_shader_init(&config, out_shader), fail);

		ResourceSystem::shader_loader_unload(&resource);
		out_shader->state = ResourceState::Initialized;

		return true;

	fail:
		ResourceSystem::shader_loader_unload(&resource);
		shader_destroy(out_shader);
		return false;
	}

	void shader_destroy(Shader* s) 
	{
		s->state = ResourceState::Destroying;
		_shader_destroy(s);
		s->state = ResourceState::Destroyed;
	}

	static bool8 _shader_init(ShaderConfig* config, Shader* out_shader)
	{
		out_shader->name = config->name;
		out_shader->bound_instance_id = Constants::max_u32;
		out_shader->last_update_frame_number = Constants::max_u8;

		out_shader->global_ubo_size = 0;
		out_shader->ubo_size = 0;

		out_shader->push_constant_stride = 128;
		out_shader->push_constant_size = 0;

		out_shader->topologies = config->topologies;
		out_shader->shader_flags = 0;
		if (config->depth_test)
			out_shader->shader_flags |= ShaderFlags::DepthTest;
		if (config->depth_write)
			out_shader->shader_flags |= ShaderFlags::DepthWrite;

		for (uint32 i = 0; i < RendererConfig::shader_max_instances; ++i)
			out_shader->instances[i].alloc_ref = {};

		out_shader->global_uniform_count = 0;
		out_shader->global_uniform_sampler_count = 0;
		out_shader->instance_uniform_count = 0;
		out_shader->instance_uniform_sampler_count = 0;
		out_shader->local_uniform_count = 0;

		for (uint32 i = 0; i < config->uniforms_count; i++)
		{
			switch (config->uniforms[i].scope)
			{
			case ShaderScope::Global:
			{
				if (config->uniforms[i].type == ShaderUniformType::Sampler)
					out_shader->global_uniform_sampler_count++;
				else
					out_shader->global_uniform_count++;
				break;
			}
			case ShaderScope::Instance:
			{
				if (config->uniforms[i].type == ShaderUniformType::Sampler)
					out_shader->instance_uniform_sampler_count++;
				else
					out_shader->instance_uniform_count++;
				break;
			}
			case ShaderScope::Local:
			{
				out_shader->local_uniform_count++;
				break;
			}
			}
		}

		if (out_shader->global_uniform_sampler_count >= system_state->max_shader_global_textures)
		{
			SHMERRORV("Shader global texture count %u exceeds max of %u", out_shader->global_uniform_sampler_count, system_state->max_shader_global_textures);
			return false;
		}

		if (out_shader->instance_uniform_sampler_count >= system_state->max_shader_instance_textures)
		{
			SHMERRORV("Shader instance texture count %u exceeds max of %u", out_shader->instance_uniform_sampler_count, system_state->max_shader_instance_textures);
			return false;
		}

		out_shader->attributes.init(config->attributes_count, 0, AllocationTag::Renderer);
		for (uint32 i = 0; i < out_shader->attributes.capacity; i++)
			_add_attribute(out_shader, &config->attributes[i], &out_shader->attributes[i]);

		out_shader->global_texture_maps.init(out_shader->global_uniform_sampler_count, 0, AllocationTag::Renderer);
		for (uint32 i = 0; i < out_shader->global_texture_maps.capacity; i++)
			out_shader->global_texture_maps[i] = MaterialSystem::get_default_texture_map();

		out_shader->uniform_lookup.init((uint32)(config->uniforms_count * 1.5f), 0);
		out_shader->uniforms.init(config->uniforms_count, 0, AllocationTag::Renderer);
		for (uint32 i = 0, global_sampler_counter = 0, instance_sampler_counter = 0; i < out_shader->uniforms.capacity; i++)
			_add_uniform(out_shader, (uint16)i, &config->uniforms[i], &global_sampler_counter, &instance_sampler_counter, &out_shader->uniforms[i]);

		out_shader->instance_count = 0;

		// Make sure the UBO is aligned according to device requirements.
		out_shader->global_ubo_stride = (uint32)get_aligned_pow2(out_shader->global_ubo_size, system_state->device_properties.required_ubo_offset_alignment);
		out_shader->ubo_stride = (uint32)get_aligned_pow2(out_shader->ubo_size, system_state->device_properties.required_ubo_offset_alignment);

		// Uniform  buffer.
		// TODO: max count should be configurable, or perhaps long term support of buffer resizing.
		uint64 total_buffer_size = out_shader->global_ubo_stride + (out_shader->ubo_stride * RendererConfig::max_material_count);  // global + (locals)
		char u_buffer_name[Constants::max_buffer_name_length];
		CString::print_s(u_buffer_name, Constants::max_buffer_name_length, "%s%s", out_shader->name.c_str(), "_u_buf");
		if (!renderbuffer_init(u_buffer_name, RenderBufferType::UNIFORM, total_buffer_size, true, &out_shader->uniform_buffer))
		{
			SHMERROR("Vulkan buffer creation failed for object shader.");
			return false;
		}
		renderbuffer_bind(&out_shader->uniform_buffer, 0);

		// Allocate space for the global UBO, whcih should occupy the _stride_ space, _not_ the actual size used.
		if (!renderbuffer_allocate(&out_shader->uniform_buffer, out_shader->global_ubo_stride, &out_shader->global_ubo_alloc_ref))
		{
			renderbuffer_destroy(&out_shader->uniform_buffer);
			SHMERROR("Failed to allocate space for the uniform buffer!");
			return false;
		}

		bool8 res = system_state->module.shader_init(config, out_shader);
		if (!res)
			renderbuffer_destroy(&out_shader->uniform_buffer);

		return res;
	}

	static void _shader_destroy(Shader* shader)
	{
		renderbuffer_destroy(&shader->uniform_buffer);
		system_state->module.shader_destroy(shader);
		shader->name.free_data();
		shader->state = ResourceState::Destroyed;
		shader->~Shader();
	}

	bool8 shader_use(Shader* s) 
	{
		return system_state->module.shader_use(s);
	}

	bool8 shader_bind_globals(Shader* s) 
	{
		s->bound_ubo_offset = s->global_ubo_alloc_ref.byte_offset;
		return system_state->module.shader_bind_globals(s);
	}

	bool8 shader_bind_instance(Shader* s, uint32 instance_id) 
	{
		s->bound_instance_id = instance_id;
		s->bound_ubo_offset = s->instances[instance_id].alloc_ref.byte_offset;

		return system_state->module.shader_bind_instance(s, instance_id);
	}

	bool8 shader_apply_globals(Shader* s) 
	{
		if (s->last_update_frame_number == system_state->frame_number)
			return true;

		s->last_update_frame_number = system_state->frame_number;
		return system_state->module.shader_apply_globals(s);
	}

	bool8 shader_apply_instance(Shader* s) 
	{
		if (s->instance_uniform_count < 1 && s->instance_uniform_sampler_count < 1)
		{
			SHMERROR("This shader does not use instances.");
			return false;
		}

		ShaderInstance* instance = &s->instances[s->bound_instance_id];
		if (instance->last_update_frame_number == system_state->frame_number)
			return true;

		instance->last_update_frame_number = system_state->frame_number;
		return system_state->module.shader_apply_instance(s);
	}

	bool8 shader_acquire_instance_resources(Shader* s, uint32 texture_maps_count, uint32* out_instance_id)
	{
		// TODO: dynamic
		*out_instance_id = Constants::max_u32;
		for (uint32 i = 0; i < 1024; ++i)
		{
			if (!s->instances[i].alloc_ref.byte_size)
			{
				*out_instance_id = i;
				break;
			}
		}
		if (*out_instance_id == Constants::max_u32)
		{
			SHMERROR("vulkan_shader_acquire_instance_resources failed to acquire new id");
			return false;
		}

		s->instance_count++;
		ShaderInstance* instance = &s->instances[*out_instance_id];

		if (texture_maps_count)
			instance->instance_texture_maps.init(texture_maps_count, true, AllocationTag::Renderer);

		// Allocate some space in the UBO - by the stride, not the size.
		uint64 size = s->ubo_stride;
		if (size > 0)
		{
			if (!renderbuffer_allocate(&s->uniform_buffer, size, &instance->alloc_ref))
			{
				SHMERROR("vulkan_material_shader_acquire_resources failed to acquire ubo space");
				return false;
			}
		}

		return system_state->module.shader_acquire_instance_resources(s, texture_maps_count, *out_instance_id);
	}

	bool8 shader_release_instance_resources(Shader* s, uint32 instance_id) 
	{
		ShaderInstance* instance = &s->instances[instance_id];

		instance->instance_texture_maps.free_data();

		renderbuffer_free(&s->uniform_buffer, &instance->alloc_ref);
		s->instance_count--;

		return system_state->module.shader_release_instance_resources(s, instance_id);
	}

	ShaderUniformId shader_get_uniform_index(Shader* shader, const char* uniform_name)
	{
		ShaderUniformId* id = shader->uniform_lookup.get(uniform_name);
		if (!id)
		{
			SHMERRORV("Shader '%s' does not have a uniform named '%s' registered.", shader->name.c_str(), uniform_name);
			return ShaderUniformId::invalid_value;
		}

		return shader->uniforms[(*id)].index;
	}

	bool8 shader_set_uniform(Shader* shader, ShaderUniformId uniform_id, const void* value) 
	{
		ShaderUniform* uniform = &shader->uniforms[uniform_id];
		if (shader->bound_scope != uniform->scope) {
			if (uniform->scope == ShaderScope::Global) {
				Renderer::shader_bind_globals(shader);
			}
			else if (uniform->scope == ShaderScope::Instance) {
				Renderer::shader_bind_instance(shader, shader->bound_instance_id);
			}
			else {
				// NOTE: Nothing to do here for locals, just set the uniform.
			}
			shader->bound_scope = uniform->scope;
		}

		if (uniform->type == ShaderUniformType::Sampler)
		{
			if (uniform->scope == ShaderScope::Global)
				shader->global_texture_maps[uniform->location] = (TextureMap*)value;
			else
				shader->instances[shader->bound_instance_id].instance_texture_maps[uniform->location] = (TextureMap*)value;

			return true;
		}

		return system_state->module.shader_set_uniform(shader, uniform, value);	
	}

	static bool8 _add_attribute(Shader* shader, ShaderAttributeConfig* config, ShaderAttribute* out_attrib)
	{
		uint16 size = 0;
		switch (config->type)
		{
		case ShaderAttributeType::Int8:
		case ShaderAttributeType::UInt8:
			size = sizeof(int8);
			break;
		case ShaderAttributeType::Int16:
		case ShaderAttributeType::UInt16:
			size = sizeof(int16);
			break;
		case ShaderAttributeType::Int32:
		case ShaderAttributeType::UInt32:
		case ShaderAttributeType::Float32:
			size = sizeof(int32);
			break;
		case ShaderAttributeType::Float32_2:
			size = sizeof(float32) * 2;
			break;
		case ShaderAttributeType::Float32_3:
			size = sizeof(float32) * 3;
			break;
		case ShaderAttributeType::Float32_4:
			size = sizeof(float32) * 4;
			break;
		case ShaderAttributeType::Mat4:
			size = sizeof(Math::Mat4);
			break;
		default:
			SHMERROR("Unrecognized type %i, defaulting to size 0.");
			size = 0;
			break;
		}

		shader->attribute_stride += size;

		out_attrib->name = config->name;
		out_attrib->size = size;
		out_attrib->type = config->type;

		return true;
	}

	static bool8 _add_uniform(Shader* shader, ShaderUniformId index, ShaderUniformConfig* config, uint32* global_sampler_counter, uint32* instance_sampler_counter, ShaderUniform* out_uniform)
	{
		if (!config->name[0])
			return false;

		out_uniform->index = index;
		out_uniform->scope = config->scope;
		out_uniform->type = config->type;

		switch (out_uniform->type)
		{
		case ShaderUniformType::Sampler:
		{
			uint32 set_location = Constants::max_u32;
			switch (out_uniform->scope)
			{
			case ShaderScope::Global:
			{
				set_location = *global_sampler_counter;
				(*global_sampler_counter)++;
				break;
			}
			case ShaderScope::Instance:
			{
				set_location = *instance_sampler_counter;
				(*instance_sampler_counter)++;
				break;
			}
			default:
			{
				SHMERRORV("%s : Local scope shader samplers not supported!", config->name);
				return false;
			}
			}

			out_uniform->location = (uint16)set_location;
			out_uniform->offset = 0;
			out_uniform->size = 0;
			out_uniform->set_index = (uint8)out_uniform->scope;
			break;
		}
		default:
		{
			switch (out_uniform->scope)
			{
			case ShaderScope::Global:
			{
				out_uniform->location = out_uniform->index;
				out_uniform->set_index = (uint8)out_uniform->scope;
				out_uniform->size = config->size;
				out_uniform->offset = shader->global_ubo_size;
				shader->global_ubo_size += out_uniform->size;
				break;
			}
			case ShaderScope::Instance:
			{
				out_uniform->location = out_uniform->index;
				out_uniform->set_index = (uint8)out_uniform->scope;
				out_uniform->size = config->size;
				out_uniform->offset = shader->ubo_size;
				shader->ubo_size += out_uniform->size;
				break;
			}
			case ShaderScope::Local:
			{
				out_uniform->set_index = Constants::max_u8;
				Range r = get_aligned_range(shader->push_constant_size, config->size, 4);

				out_uniform->offset = (uint32)r.offset;
				out_uniform->size = (uint16)r.size;

				shader->push_constant_ranges[shader->push_constant_range_count] = r;
				shader->push_constant_range_count++;

				shader->push_constant_size += (uint32)r.size;
				break;
			}
			}

			break;
		}
		}

		shader->uniform_lookup.set_value(config->name, out_uniform->index);

		return true;
	}
}
