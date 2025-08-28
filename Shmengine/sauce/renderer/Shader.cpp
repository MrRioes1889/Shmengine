#include "RendererFrontend.hpp"
#include "renderer/Utility.hpp"

namespace Renderer
{
	extern SystemState* system_state;

	bool8 shader_create(const ShaderConfig* config, Shader* shader)
	{
		shader->state = ShaderState::Uninitialized;
		shader->name = config->name;
		shader->bound_instance_id = Constants::max_u32;
		shader->last_update_frame_number = Constants::max_u8;

		shader->global_texture_maps.init(1, 0, AllocationTag::Renderer);
		shader->uniforms.init(1, 0, AllocationTag::Renderer);
		shader->attributes.init(1, 0, AllocationTag::Renderer);

		shader->uniform_lookup.init(1024, 0);
		shader->uniform_lookup.floodfill(Constants::max_u16);

		shader->global_ubo_size = 0;
		shader->ubo_size = 0;

		shader->push_constant_stride = 128;
		shader->push_constant_size = 0;

		shader->topologies = config->topologies;
		shader->shader_flags = 0;
		if (config->depth_test)
			shader->shader_flags |= ShaderFlags::DepthTest;
		if (config->depth_write)
			shader->shader_flags |= ShaderFlags::DepthWrite;

		for (uint32 i = 0; i < RendererConfig::shader_max_instances; ++i)
			shader->instances[i].alloc_ref = {};

		shader->global_uniform_count = 0;
		shader->global_uniform_sampler_count = 0;
		shader->instance_uniform_count = 0;
		shader->instance_uniform_sampler_count = 0;
		shader->local_uniform_count = 0;

		for (uint32 i = 0; i < config->uniforms_count; i++)
		{
			switch (config->uniforms[i].scope)
			{
			case ShaderScope::Global:
			{
				if (config->uniforms[i].type == ShaderUniformType::Sampler)
					shader->global_uniform_sampler_count++;
				else
					shader->global_uniform_count++;
				break;
			}
			case ShaderScope::Instance:
			{
				if (config->uniforms[i].type == ShaderUniformType::Sampler)
					shader->instance_uniform_sampler_count++;
				else
					shader->instance_uniform_count++;
				break;
			}
			case ShaderScope::Local:
			{
				shader->local_uniform_count++;
				break;
			}
			}
		}

		return system_state->module.shader_create(config, shader);
	}

	bool8 shader_init(Shader* s) 
	{

		s->instance_count = 0;

		// Make sure the UBO is aligned according to device requirements.
		s->global_ubo_stride = (uint32)get_aligned_pow2(s->global_ubo_size, s->required_ubo_alignment);
		s->ubo_stride = (uint32)get_aligned_pow2(s->ubo_size, s->required_ubo_alignment);

		// Uniform  buffer.
		// TODO: max count should be configurable, or perhaps long term support of buffer resizing.
		uint64 total_buffer_size = s->global_ubo_stride + (s->ubo_stride * RendererConfig::max_material_count);  // global + (locals)
		char u_buffer_name[Constants::max_buffer_name_length];
		CString::print_s(u_buffer_name, Constants::max_buffer_name_length, "%s%s", s->name.c_str(), "_u_buf");
		if (!renderbuffer_init(u_buffer_name, RenderBufferType::UNIFORM, total_buffer_size, true, &s->uniform_buffer))
		{
			SHMERROR("Vulkan buffer creation failed for object shader.");
			return false;
		}
		renderbuffer_bind(&s->uniform_buffer, 0);

		// Allocate space for the global UBO, whcih should occupy the _stride_ space, _not_ the actual size used.
		if (!renderbuffer_allocate(&s->uniform_buffer, s->global_ubo_stride, &s->global_ubo_alloc_ref))
		{
			renderbuffer_destroy(&s->uniform_buffer);
			SHMERROR("Failed to allocate space for the uniform buffer!");
			return false;
		}

		bool8 res = system_state->module.shader_init(s);
		if (!res)
			renderbuffer_destroy(&s->uniform_buffer);

		return res;
	}

	void shader_destroy(Shader* s) 
	{
		renderbuffer_destroy(&s->uniform_buffer);
		system_state->module.shader_destroy(s);
		s->name.free_data();
		s->state = ShaderState::Uninitialized;
		s->~Shader();
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

	bool8 shader_set_uniform(Shader* s, ShaderUniform* uniform, const void* value) 
	{
		if (uniform->type == ShaderUniformType::Sampler)
		{
			if (uniform->scope == ShaderScope::Global)
				s->global_texture_maps[uniform->location] = (TextureMap*)value;
			else
				s->instances[s->bound_instance_id].instance_texture_maps[uniform->location] = (TextureMap*)value;

			return true;
		}

		return system_state->module.shader_set_uniform(s, uniform, value);	
	}
}
