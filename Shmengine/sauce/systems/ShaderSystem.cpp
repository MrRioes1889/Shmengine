#include "ShaderSystem.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"

#include "renderer/RendererFrontend.hpp"

#include "TextureSystem.hpp"

namespace ShaderSystem
{

	struct SystemState
	{
		Config config;
		Hashtable<uint32> lookup;
		uint32 bound_shader_id;
		Sarray<Shader> shaders;
	};

	static SystemState* system_state = 0;

	static bool32 add_attribute(Shader* shader, const ShaderAttributeConfig& config);
	static bool32 add_sampler(Shader* shader, const ShaderUniformConfig& config);	
	static SHMINLINE uint32 get_shader_id(const char* shader_name);
	static uint32 new_shader_id();
	static bool32 uniform_add(Shader* shader, const ShaderUniformConfig& config);
	static bool32 uniform_add(Shader* shader, const char* uniform_name, uint32 size, ShaderUniformType type, ShaderScope scope, uint32 set_location, bool32 is_sampler);
	static bool32 uniform_name_valid(Shader* shader, const char* uniform_name);
	static bool32 shader_uniform_add_state_valid(Shader* shader);
	static void destroy_shader(const char* shader_name);
	static void destroy_shader(Shader* shader);

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config)
	{

		// This is to help avoid hashtable collisions.
		if (config.max_shader_count < 512) 			
			SHMWARN("shader_system_initialize - config.max_shader_count is recommended to be at least 512.");

		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		system_state->config = config;
		system_state->bound_shader_id = INVALID_ID;

		uint64 hashtable_data_size = sizeof(uint32) * config.max_shader_count;
		void* hashtable_data = allocator_callback(hashtable_data_size);
		system_state->lookup.init(config.max_shader_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->lookup.floodfill(INVALID_ID);

		uint64 shader_array_size = sizeof(Shader) * config.max_shader_count;
		void* shader_array = allocator_callback(shader_array_size);
		system_state->shaders.init(config.max_shader_count, SarrayFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, shader_array);

		for (uint32 i = 0; i < system_state->shaders.count; i++)
		{
			system_state->shaders[i].id = INVALID_ID;
		}

		return true;

	}

	void system_shutdown()
	{
		if (!system_state)
			return;

		for (uint32 i = 0; i < system_state->shaders.count; i++)
		{
			if (system_state->shaders[i].id != INVALID_ID)
				destroy_shader(&system_state->shaders[i]);
		}
		system_state->lookup.free_data();

		system_state = 0;
	}

	bool32 create_shader(const ShaderConfig& config)
	{
		
		uint32 id = new_shader_id();
		if (id == INVALID_ID)
		{
			SHMERROR("shader_create - Unable to find free slot for new shader");
			return false;
		}

		Shader* shader = &system_state->shaders[id];
		Memory::zero_memory(shader, sizeof(Shader));
		shader->id = id;
		shader->state = ShaderState::NOT_CREATED;
		shader->name = config.name;
		shader->use_instances = config.use_instances;
		shader->use_locals = config.use_local;
		shader->bound_instance_id = INVALID_ID;
		
		shader->global_textures.init(1, 0, AllocationTag::MAIN);
		shader->uniforms.init(1, 0, AllocationTag::MAIN);
		shader->attributes.init(1, 0, AllocationTag::MAIN);

		shader->uniform_lookup.init(1024, 0, AllocationTag::MAIN);
		shader->uniform_lookup.floodfill(INVALID_ID16);

		shader->global_ubo_size = 0;
		shader->ubo_size = 0;

		shader->push_constant_stride = 128;
		shader->push_constant_size = 0;

		uint8 renderpass_id = INVALID_ID8;
		if (!Renderer::get_renderpass_id(config.renderpass_name.c_str(), &renderpass_id))
		{
			SHMERRORV("shader_create - Unable to find renderpass '%s'", config.renderpass_name.c_str());
			return false;
		}

		if (!Renderer::shader_create(shader, renderpass_id, (uint8)config.stages.count, config.stage_filenames, config.stages.data))
		{
			SHMERROR("shader_create - Error creating shader.");
			return false;
		}

		shader->state = ShaderState::UNINITIALIZED;
		for (uint32 i = 0; i < config.attributes.count; i++)
			add_attribute(shader, config.attributes.data[i]);

		for (uint32 i = 0; i < config.uniforms.count; i++)
		{
			if (config.uniforms.data[i].type == ShaderUniformType::SAMPLER)
				add_sampler(shader, config.uniforms.data[i]);
			else
				uniform_add(shader, config.uniforms.data[i]);
		}
		
		if (!Renderer::shader_init(shader))
		{
			SHMERRORV("shader_create - Error initializing shader '%s'.", config.name.c_str());
			return false;
		}

		if (!system_state->lookup.set_value(config.name.c_str(), shader->id))
		{
			Renderer::shader_destroy(shader);
			return false;
		}

		return true;

	}

	uint32 get_id(const char* shader_name)
	{
		return get_shader_id(shader_name);
	}

	Shader* get_shader(uint32 shader_id)
	{
		if (shader_id >= system_state->config.max_shader_count || system_state->shaders[shader_id].id == INVALID_ID) {
			return 0;
		}
		return &system_state->shaders[shader_id];
	}

	Shader* get_shader(const char* shader_name)
	{
		uint32 shader_id = get_shader_id(shader_name);
		return get_shader(shader_id);
	}

	static void destroy_shader(Shader* shader)
	{

		Renderer::shader_destroy(shader);

		shader->state = ShaderState::NOT_CREATED;
		shader->name.free_data();
		for (uint32 i = 0; i < shader->attributes.count; i++)
			shader->attributes[i].name.free_data();
		shader->attributes.free_data();
		shader->uniforms.free_data();
		shader->uniform_lookup.free_data();
		shader->global_textures.free_data();

	}

	static void destroy_shader(const char* shader_name)
	{
		uint32 shader_id = get_shader_id(shader_name);
		if (shader_id == INVALID_ID)
			return;

		destroy_shader(&system_state->shaders[shader_id]);
	}

	bool32 use_shader(uint32 shader_id)
	{

		if (system_state->bound_shader_id == shader_id)
			return true;
	
		Shader* shader = get_shader(shader_id);
		if (!Renderer::shader_use(shader))
		{
			SHMERRORV("use_shader - Failed using shader '%s'.", shader->name.c_str());
			return false;
		}

		if (!Renderer::shader_bind_globals(shader))
		{
			SHMERRORV("use_shader - Failed binding globals for shader '%s'.", shader->name.c_str());
			return false;
		}

		system_state->bound_shader_id = shader_id;
		return true;

	}

	bool32 use_shader(const char* shader_name)
	{
		uint32 shader_id = get_shader_id(shader_name);
		if (shader_id == INVALID_ID)
			return false;

		return use_shader(shader_id);
	}

	uint16 get_uniform_index(Shader* shader, const char* uniform_name)
	{
		
		uint16 index = INVALID_ID16;
		index = shader->uniform_lookup.get_value(uniform_name);
		if (index == INVALID_ID16)
		{
			SHMERRORV("Shader '%s' does not have a uniform named '%s' registered.", shader->name.c_str(), uniform_name);
			return INVALID_ID16;
		}

		return shader->uniforms[index].index;

	}

	bool32 set_uniform(uint16 index, const void* value)
	{
		
		Shader* shader = &system_state->shaders[system_state->bound_shader_id];
		ShaderUniform* uniform = &shader->uniforms[index];
		if (shader->bound_scope != uniform->scope) {
			if (uniform->scope == ShaderScope::GLOBAL) {
				Renderer::shader_bind_globals(shader);
			}
			else if (uniform->scope == ShaderScope::INSTANCE) {
				Renderer::shader_bind_instance(shader, shader->bound_instance_id);
			}
			else {
				// NOTE: Nothing to do here for locals, just set the uniform.
			}
			shader->bound_scope = uniform->scope;
		}
		return Renderer::shader_set_uniform(shader, uniform, value);

	}

	bool32 set_uniform(const char* uniform_name, const void* value)
	{

		if (system_state->bound_shader_id == INVALID_ID) {
			SHMERROR("set_uniform - called without a shader in use.");
			return false;
		}

		Shader* shader = &system_state->shaders[system_state->bound_shader_id];
		uint16 index = get_uniform_index(shader, uniform_name);
		return set_uniform(index, value);

	}

	bool32 set_sampler(uint16 location, const void* value)
	{
		return set_uniform(location, value);
	}

	bool32 set_sampler(const char* sampler_name, const void* value)
	{
		return set_uniform(sampler_name, value);
	}

	bool32 apply_global()
	{
		return Renderer::shader_apply_globals(&system_state->shaders[system_state->bound_shader_id]);
	}

	bool32 apply_instance(bool32 needs_update)
	{
		return Renderer::shader_apply_instance(&system_state->shaders[system_state->bound_shader_id], needs_update);
	}

	bool32 bind_instance(uint32 instance_id)
	{
		
		Shader* shader = &system_state->shaders[system_state->bound_shader_id];
		shader->bound_instance_id = instance_id;
		return Renderer::shader_bind_instance(shader, instance_id);

	}

	static bool32 add_attribute(Shader* shader, const ShaderAttributeConfig& config)
	{
		
		uint16 size = 0;
		switch (config.type)
		{
		case ShaderAttributeType::INT8:
		case ShaderAttributeType::UINT8:
			size = sizeof(int8);
			break;
		case ShaderAttributeType::INT16:
		case ShaderAttributeType::UINT16:
			size = sizeof(int16);
			break;
		case ShaderAttributeType::INT32:
		case ShaderAttributeType::UINT32:
		case ShaderAttributeType::FLOAT32:
			size = sizeof(int32);
			break;
		case ShaderAttributeType::FLOAT32_2:
			size = sizeof(float32) * 2;
			break;
		case ShaderAttributeType::FLOAT32_3:
			size = sizeof(float32) * 3;
			break;
		case ShaderAttributeType::FLOAT32_4:
			size = sizeof(float32) * 4;
			break;
		case ShaderAttributeType::MAT4:
			size = sizeof(Math::Mat4);
			break;
		default:
			SHMERROR("Unrecognized type %i, defaulting to size 0.");
			size = 0;
			break;
		}

		shader->attribute_stride += size;

		ShaderAttribute attrib = {};
		attrib.name = config.name;
		attrib.size = size;
		attrib.type = config.type;
		shader->attributes.push(attrib);

		return true;

	}

	static bool32 add_sampler(Shader* shader, const ShaderUniformConfig& config)
	{

		if (config.scope == ShaderScope::INSTANCE && !shader->use_instances) 
		{
			SHMERROR("add_sampler cannot add an instance sampler for a shader that does not use instances.");
			return false;
		}

		if (config.scope == ShaderScope::LOCAL) 
		{
			SHMERROR("add_sampler cannot add a sampler at local scope.");
			return false;
		}

		if (!uniform_name_valid(shader, config.name.c_str()) || !shader_uniform_add_state_valid(shader)) 
			return false;

		uint32 location = 0;
		if (config.scope == ShaderScope::GLOBAL) 
		{
			if (shader->global_textures.count + 1 > system_state->config.max_global_textures)
			{
				SHMERRORV("Shader global texture count %i exceeds max of %i", shader->global_textures.count + 1, system_state->config.max_global_textures);
				return false;
			}
			shader->global_textures.push(TextureSystem::get_default_texture());
		}
		else 
		{
			if (shader->instance_texture_count + 1 > system_state->config.max_instance_textures) 
			{
				SHMERRORV("Shader instance texture count %i exceeds max of %i", shader->instance_texture_count, system_state->config.max_instance_textures);
				return false;
			}
			location = shader->instance_texture_count;
			shader->instance_texture_count++;
		}

		// Treat it like a uniform. NOTE: In the case of samplers, out_location is used to determine the
		// hashtable entry's 'location' field value directly, and is then set to the index of the uniform array.
		// This allows location lookups for samplers as if they were uniforms as well (since technically they are).
		// TODO: might need to store this elsewhere
		return uniform_add(shader, config.name.c_str(), 0, config.type, config.scope, location, true);

	}

	static bool32 uniform_add(Shader* shader, const ShaderUniformConfig& config)
	{
		if (!uniform_name_valid(shader, config.name.c_str()) || !shader_uniform_add_state_valid(shader))
			return false;

		return uniform_add(shader, config.name.c_str(), config.size, config.type, config.scope, 0, false);
	}

	static SHMINLINE uint32 get_shader_id(const char* shader_name)
	{
		uint32 shader_id = system_state->lookup.get_value(shader_name);
		return shader_id;
	}

	static uint32 new_shader_id()
	{
		for (uint32 i = 0; i < system_state->config.max_shader_count; i++)
		{
			if (system_state->shaders[i].id == INVALID_ID)
				return i;
		}
		return INVALID_ID;
	}

	static bool32 uniform_add(Shader* shader, const char* uniform_name, uint32 size, ShaderUniformType type, ShaderScope scope, uint32 set_location, bool32 is_sampler)
	{
		
		if (shader->uniforms.count + 1 > system_state->config.max_uniform_count)
		{
			SHMERRORV("A shader can only accept a combined maximum of %d uniforms and samplers at global, instance and local scopes.", system_state->config.max_uniform_count);
			return false;
		}

		ShaderUniform entry;
		entry.index = (uint16)shader->uniforms.count;
		entry.scope = scope;
		entry.type = type;
		if (is_sampler)
			entry.location = (uint16)set_location;
		else
			entry.location = entry.index;

		if (scope != ShaderScope::LOCAL)
		{
			entry.set_index = (uint8)scope;
			entry.offset = is_sampler ? 0 : (scope == ShaderScope::GLOBAL ? shader->global_ubo_size : shader->ubo_size);
			entry.size = is_sampler ? 0 : (uint16)size;
		}
		else
		{
			if (!shader->use_locals) {
				SHMERROR("Cannot add a locally-scoped uniform for a shader that does not support locals.");
				return false;
			}

			entry.set_index = INVALID_ID8;
			Range r = get_aligned_range(shader->push_constant_size, size, 4);

			entry.offset = (uint32)r.offset;
			entry.size = (uint16)r.size;

			shader->push_constant_ranges[shader->push_constant_range_count] = r;
			shader->push_constant_range_count++;

			shader->push_constant_size += (uint32)r.size;
		}

		shader->uniform_lookup.set_value(uniform_name, entry.index);
		shader->uniforms.push(entry);

		if (is_sampler)
			return true;

		if (entry.scope == ShaderScope::GLOBAL)
			shader->global_ubo_size += entry.size;
		else if (entry.scope == ShaderScope::INSTANCE)
			shader->ubo_size += entry.size;

		return true;
	}

	static bool32 uniform_name_valid(Shader* shader, const char* uniform_name)
	{
		return (uniform_name != 0 && uniform_name[0] != 0);
	}

	static bool32 shader_uniform_add_state_valid(Shader* shader)
	{
		if (shader->state != ShaderState::UNINITIALIZED)
		{
			SHMERROR("Uniforms may only be added to shaders before initialization.");
			return false;
		}
		return true;
	}

}