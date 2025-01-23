#include "ShaderSystem.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"

#include "renderer/RendererFrontend.hpp"
#include "MaterialSystem.hpp"

#include "resources/loaders/ShaderLoader.hpp"

namespace ShaderSystem
{

	struct SystemState
	{
		SystemConfig config;
		Hashtable<uint32> lookup;
		uint32 bound_shader_id;
		Sarray<Shader> shaders;
		TextureMap default_texture_map;

		uint32 material_shader_id;
		uint32 terrain_shader_id;
		uint32 ui_shader_id;
		uint32 skybox_shader_id;
		uint32 color3D_shader_id;
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
	static bool32 create_default_texture_map();

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{

		// This is to help avoid hashtable collisions.
		SystemConfig* sys_config = (SystemConfig*)config;
		if (sys_config->max_shader_count < 512)
			SHMWARN("shader_system_initialize - config.max_shader_count is recommended to be at least 512.");

		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *sys_config;
		system_state->bound_shader_id = INVALID_ID;

		system_state->material_shader_id = INVALID_ID;
		system_state->terrain_shader_id = INVALID_ID;
		system_state->ui_shader_id = INVALID_ID;
		system_state->skybox_shader_id = INVALID_ID;
		system_state->color3D_shader_id = INVALID_ID;

		uint64 hashtable_data_size = sizeof(uint32) * sys_config->max_shader_count;
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->lookup.init(sys_config->max_shader_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->lookup.floodfill(INVALID_ID);

		uint64 shader_array_size = sizeof(Shader) * sys_config->max_shader_count;
		void* shader_array = allocator_callback(allocator, shader_array_size);
		system_state->shaders.init(sys_config->max_shader_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, shader_array);

		for (uint32 i = 0; i < system_state->shaders.capacity; i++)
		{
			system_state->shaders[i].id = INVALID_ID;
		}

		return true;

	}

	void system_shutdown(void* state)
	{
		if (!system_state)
			return;

		for (uint32 i = 0; i < system_state->shaders.capacity; i++)
		{
			if (system_state->shaders[i].id != INVALID_ID)
				destroy_shader(&system_state->shaders[i]);
		}

		Renderer::texture_map_release_resources(&system_state->default_texture_map);

		system_state->lookup.free_data();

		system_state = 0;
	}

	bool32 create_shader(const Renderer::RenderPass* renderpass, const ShaderConfig* config)
	{

		using namespace Renderer;

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

		if (!Renderer::shader_create(shader, config, renderpass))
		{
			SHMERROR("shader_create - Error creating shader.");
			return false;
		}

		shader->state = ShaderState::UNINITIALIZED;
		for (uint32 i = 0; i < config->attributes_count; i++)
			add_attribute(shader, config->attributes[i]);

		for (uint32 i = 0; i < config->uniforms_count; i++)
		{
			if (config->uniforms[i].type == ShaderUniformType::SAMPLER)
				add_sampler(shader, config->uniforms[i]);
			else
				uniform_add(shader, config->uniforms[i]);
		}

		if (!Renderer::shader_init(shader))
		{
			SHMERRORV("Error initializing shader '%s'.", shader->name.c_str());
			return false;
		}

		if (!system_state->lookup.set_value(shader->name.c_str(), shader->id))
		{
			Renderer::shader_destroy(shader);
			return false;
		}

		if (system_state->material_shader_id == INVALID_ID && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_material_phong)) 
			system_state->material_shader_id = shader->id;
		else if (system_state->terrain_shader_id == INVALID_ID && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_terrain)) 
			system_state->terrain_shader_id = shader->id;
		else if (system_state->ui_shader_id == INVALID_ID && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_ui)) 
			system_state->ui_shader_id = shader->id;
		else if (system_state->skybox_shader_id == INVALID_ID && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_skybox)) 
			system_state->skybox_shader_id = shader->id;
		else if (system_state->color3D_shader_id == INVALID_ID && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_color3D))
			system_state->color3D_shader_id = shader->id;

		return true;

	}

	bool32 create_shader_from_resource(const char* resource_name, Renderer::RenderPass* renderpass)
	{
		ShaderResourceData resource = {};
		if (!ResourceSystem::shader_loader_load(resource_name, 0, &resource))
		{
			SHMERROR("Failed to load world pick shader config.");
			return false;
		}

		ShaderConfig config = ResourceSystem::shader_loader_get_config_from_resource(&resource);
		if (!ShaderSystem::create_shader(renderpass, &config))
		{
			SHMERROR("Failed to create world pick shader.");
			ResourceSystem::shader_loader_unload(&resource);
			return false;
		}

		ResourceSystem::shader_loader_unload(&resource);
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

		(*shader).~Shader();
		shader->state = ShaderState::NOT_CREATED;

	}

	static void destroy_shader(const char* shader_name)
	{
		uint32 shader_id = get_shader_id(shader_name);
		if (shader_id == INVALID_ID)
			return;

		destroy_shader(&system_state->shaders[shader_id]);
	}

	void bind_shader(uint32 shader_id)
	{
		system_state->bound_shader_id = shader_id;
	}

	bool32 use_shader(uint32 shader_id)
	{

		Shader* shader = get_shader(shader_id);
		if (!Renderer::shader_use(shader))
		{
			SHMERRORV("use_shader - Failed using shader '%s'.", shader->name.c_str());
			return false;
		}

		bind_shader(shader_id);
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

		using namespace Renderer;

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

	bool32 bind_globals()
	{

		Shader* shader = &system_state->shaders[system_state->bound_shader_id];
		return Renderer::shader_bind_globals(shader);

	}

	bool32 bind_instance(uint32 instance_id)
	{

		Shader* shader = &system_state->shaders[system_state->bound_shader_id];
		shader->bound_instance_id = instance_id;
		return Renderer::shader_bind_instance(shader, instance_id);

	}

	uint32 get_material_phong_shader_id()
	{
		return system_state->material_shader_id;
	}

	uint32 get_terrain_shader_id()
	{
		return system_state->terrain_shader_id;
	}

	uint32 get_ui_shader_id()
	{
		return system_state->ui_shader_id;
	}

	uint32 get_skybox_shader_id()
	{
		return system_state->skybox_shader_id;
	}

	uint32 get_color3D_shader_id()
	{
		return system_state->color3D_shader_id;
	}

	static bool32 add_attribute(Shader* shader, const ShaderAttributeConfig& config)
	{

		using namespace Renderer;

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
		shader->attributes.emplace(attrib);

		return true;

	}

	static bool32 add_sampler(Shader* shader, const ShaderUniformConfig& config)
	{

		using namespace Renderer;

		if (config.scope == ShaderScope::LOCAL)
		{
			SHMERROR("add_sampler cannot add a sampler at local scope.");
			return false;
		}

		if (!uniform_name_valid(shader, config.name) || !shader_uniform_add_state_valid(shader))
			return false;

		uint32 location = 0;
		if (config.scope == ShaderScope::GLOBAL)
		{
			if (shader->global_texture_maps.count + 1 > system_state->config.max_global_textures)
			{
				SHMERRORV("Shader global texture count %i exceeds max of %i", shader->global_texture_maps.count + 1, system_state->config.max_global_textures);
				return false;
			}
			location = shader->global_texture_maps.count;

			if (!system_state->default_texture_map.internal_data)
				create_default_texture_map();

			shader->global_texture_maps.push(&system_state->default_texture_map);
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
		return uniform_add(shader, config.name, 0, config.type, config.scope, location, true);

	}

	static bool32 uniform_add(Shader* shader, const ShaderUniformConfig& config)
	{
		if (!uniform_name_valid(shader, config.name) || !shader_uniform_add_state_valid(shader))
			return false;

		return uniform_add(shader, config.name, config.size, config.type, config.scope, 0, false);
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

		using namespace Renderer;

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
			entry.set_index = INVALID_ID8;
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

	static bool32 create_default_texture_map()
	{

		system_state->default_texture_map.filter_magnify = TextureFilter::LINEAR;
		system_state->default_texture_map.filter_minify = TextureFilter::LINEAR;
		system_state->default_texture_map.repeat_u = TextureRepeat::REPEAT;
		system_state->default_texture_map.repeat_v = TextureRepeat::REPEAT;
		system_state->default_texture_map.repeat_w = TextureRepeat::REPEAT;

		system_state->default_texture_map.texture = TextureSystem::get_default_diffuse_texture();

		if (!Renderer::texture_map_acquire_resources(&system_state->default_texture_map)) {
			SHMERROR("Failed to acquire resources for default texture map.");
			return false;
		}
		return true;
	}

}