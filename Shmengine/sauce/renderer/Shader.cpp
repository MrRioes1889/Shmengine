#include "RendererFrontend.hpp"
#include "renderer/Utility.hpp"
#include "systems/TextureSystem.hpp"
#include "resources/loaders/ShaderLoader.hpp"

namespace Renderer
{
	extern SystemState* system_state;

	// TODO: temp
	static TextureMap default_texture_map;

	static bool8 _shader_init(ShaderConfig* config, Shader* out_shader);
	static void _shader_destroy(Shader* shader);

	static bool8 _add_attribute(Shader* shader, const ShaderAttributeConfig* config);
	static bool8 _add_sampler(Shader* shader, const ShaderUniformConfig* config);
	static bool8 _add_uniform(Shader* shader, const ShaderUniformConfig* config);
	static bool8 _add_uniform(Shader* shader, const char* uniform_name, uint32 size, ShaderUniformType type, ShaderScope scope, uint32 set_location, bool8 is_sampler);

	static bool8 _create_default_texture_map();

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

		out_shader->global_texture_maps.init(1, 0, AllocationTag::Renderer);
		out_shader->uniforms.init(1, 0, AllocationTag::Renderer);
		out_shader->attributes.init(1, 0, AllocationTag::Renderer);

		out_shader->uniform_lookup.init(1024, 0);
		out_shader->uniform_lookup.floodfill(Constants::max_u16);

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

		for (uint32 i = 0; i < config->attributes_count; i++)
			_add_attribute(out_shader, &config->attributes[i]);

		for (uint32 i = 0; i < config->uniforms_count; i++)
		{
			if (config->uniforms[i].type == ShaderUniformType::Sampler)
				_add_sampler(out_shader, &config->uniforms[i]);
			else
				_add_uniform(out_shader, &config->uniforms[i]);
		}

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
		ShaderUniformId index = shader->uniform_lookup.get_value(uniform_name);
		if (!index.is_valid())
		{
			SHMERRORV("Shader '%s' does not have a uniform named '%s' registered.", shader->name.c_str(), uniform_name);
			return ShaderUniformId::invalid_value;
		}

		return shader->uniforms[index].index;
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

	static bool8 _add_attribute(Shader* shader, const ShaderAttributeConfig* config)
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

		ShaderAttribute attrib = {};
		attrib.name = config->name;
		attrib.size = size;
		attrib.type = config->type;
		shader->attributes.emplace(attrib);

		return true;
	}

	static bool8 _add_sampler(Shader* shader, const ShaderUniformConfig* config)
	{
		if (config->scope == ShaderScope::Local)
		{
			SHMERROR("add_sampler cannot add a sampler at local scope.");
			return false;
		}

		if (!config->name[0])
			return false;

		uint32 location = 0;
		if (config->scope == ShaderScope::Global)
		{
			if (shader->global_texture_maps.count >= system_state->max_shader_global_textures)
			{
				SHMERRORV("Shader global texture count %u exceeds max of %u", shader->global_texture_maps.count + 1, system_state->max_shader_global_textures);
				return false;
			}
			location = shader->global_texture_maps.count;

			if (!default_texture_map.internal_data)
				_create_default_texture_map();

			shader->global_texture_maps.push(&default_texture_map);
		}
		else
		{
			if (shader->instance_texture_count >= system_state->max_shader_instance_textures)
			{
				SHMERRORV("Shader instance texture count %u exceeds max of %u", shader->instance_texture_count, system_state->max_shader_instance_textures);
				return false;
			}
			location = shader->instance_texture_count;
			shader->instance_texture_count++;
		}

		// Treat it like a uniform. NOTE: In the case of samplers, out_location is used to determine the
		// hashtable entry's 'location' field value directly, and is then set to the index of the uniform array.
		// This allows location lookups for samplers as if they were uniforms as well (since technically they are).
		// TODO: might need to store this elsewhere
		return _add_uniform(shader, config->name, 0, config->type, config->scope, location, true);
	}

	static bool8 _add_uniform(Shader* shader, const ShaderUniformConfig* config)
	{
		return _add_uniform(shader, config->name, config->size, config->type, config->scope, 0, false);
	}

	static bool8 _add_uniform(Shader* shader, const char* uniform_name, uint32 size, ShaderUniformType type, ShaderScope scope, uint32 set_location, bool8 is_sampler)
	{
		if (shader->uniforms.count >= system_state->max_shader_uniform_count)
		{
			SHMERRORV("A shader can only accept a combined maximum of %d uniforms and samplers at global, instance and local scopes.", system_state->max_shader_uniform_count);
			return false;
		}

		if (!uniform_name[0])
			return false;

		ShaderUniform entry;
		entry.index = (uint16)shader->uniforms.count;
		entry.scope = scope;
		entry.type = type;
		if (is_sampler)
			entry.location = (uint16)set_location;
		else
			entry.location = entry.index;

		if (scope != ShaderScope::Local)
		{
			entry.set_index = (uint8)scope;
			entry.offset = is_sampler ? 0 : (scope == ShaderScope::Global ? shader->global_ubo_size : shader->ubo_size);
			entry.size = is_sampler ? 0 : (uint16)size;
		}
		else
		{
			entry.set_index = Constants::max_u8;
			Range r = get_aligned_range(shader->push_constant_size, size, 4);

			entry.offset = (uint32)r.offset;
			entry.size = (uint16)r.size;

			shader->push_constant_ranges[shader->push_constant_range_count] = r;
			shader->push_constant_range_count++;

			shader->push_constant_size += (uint32)r.size;
		}

		shader->uniform_lookup.set_value(uniform_name, entry.index);
		shader->uniforms.emplace(entry);

		if (is_sampler)
			return true;

		if (entry.scope == ShaderScope::Global)
			shader->global_ubo_size += entry.size;
		else if (entry.scope == ShaderScope::Instance)
			shader->ubo_size += entry.size;

		return true;
	}

	static bool8 _create_default_texture_map()
	{
		TextureMapConfig map_config = {};
		map_config.filter_magnify = TextureFilter::LINEAR;
		map_config.filter_minify = TextureFilter::LINEAR;
		map_config.repeat_u = TextureRepeat::REPEAT;
		map_config.repeat_v = TextureRepeat::REPEAT;
		map_config.repeat_w = TextureRepeat::REPEAT;

		if (!Renderer::texture_map_init(&map_config, &default_texture_map)) {
			SHMERROR("Failed to acquire resources for default texture map.");
			return false;
		}

		default_texture_map.texture = TextureSystem::get_default_diffuse_texture();
		return true;
	}
}
