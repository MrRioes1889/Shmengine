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
		HashtableOA<ShaderId> lookup;
		Sarray<Shader> shaders;
		TextureMap default_texture_map;

		ShaderId bound_shader_id;
		ShaderId material_shader_id;
		ShaderId terrain_shader_id;
		ShaderId ui_shader_id;
		ShaderId skybox_shader_id;
		ShaderId color3D_shader_id;
	};

	static SystemState* system_state = 0;

	static bool32 add_attribute(Shader* shader, const ShaderAttributeConfig* config);
	static bool32 add_sampler(Shader* shader, const ShaderUniformConfig* config);
	static bool32 add_uniform(Shader* shader, const ShaderUniformConfig* config);
	static bool32 add_uniform(Shader* shader, const char* uniform_name, uint32 size, ShaderUniformType type, ShaderScope scope, uint32 set_location, bool32 is_sampler);

	static bool32 create_default_texture_map();

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		if (sys_config->max_shader_count < 512)
			SHMWARN("shader_system_initialize - config.max_shader_count is recommended to be at least 512.");

		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *sys_config;
		system_state->bound_shader_id.invalidate();

		system_state->material_shader_id.invalidate();
		system_state->terrain_shader_id.invalidate();
		system_state->ui_shader_id.invalidate();
		system_state->skybox_shader_id.invalidate();
		system_state->color3D_shader_id.invalidate();

		uint64 hashtable_data_size = system_state->lookup.get_external_size_requirement(sys_config->max_shader_count);
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->lookup.init(sys_config->max_shader_count, HashtableOAFlag::ExternalMemory, AllocationTag::UNKNOWN, hashtable_data);

		system_state->lookup.floodfill({});

		uint64 shader_array_size = sizeof(Shader) * sys_config->max_shader_count;
		void* shader_array = allocator_callback(allocator, shader_array_size);
		system_state->shaders.init(sys_config->max_shader_count, SarrayFlags::ExternalMemory, AllocationTag::UNKNOWN, shader_array);

		for (uint32 i = 0; i < system_state->shaders.capacity; i++)
			system_state->shaders[i].id.invalidate();

		return true;

	}

	void system_shutdown(void* state)
	{
		if (!system_state)
			return;

		for (uint32 i = 0; i < system_state->shaders.capacity; i++)
		{
			if (system_state->shaders[i].id.is_valid())
				destroy_shader(system_state->shaders[i].id);
		}

		Renderer::texture_map_release_resources(&system_state->default_texture_map);
		system_state->lookup.free_data();
		system_state->shaders.free_data();

		system_state = 0;
	}

	bool32 create_shader(const Renderer::RenderPass* renderpass, const ShaderConfig* config)
	{
		using namespace Renderer;

		ShaderId id = {};
		for (Id16 i = 0; i < system_state->shaders.capacity; i++)
		{
			if (!system_state->shaders[i].id.is_valid())
			{
				id = i;
				break;
			}
		}

		if (!id.is_valid())
		{
			SHMERROR("shader_create - Unable to find free slot for new shader");
			return false;
		}

		Shader* shader = &system_state->shaders[id];
		Memory::zero_memory(shader, sizeof(Shader));
		shader->id = id;
		shader->state = ShaderState::Uninitialized;

		if (!Renderer::shader_create(shader, config, renderpass))
		{
			destroy_shader(shader->id);
			SHMERROR("shader_create - Error creating shader.");
			return false;
		}

		shader->state = ShaderState::Initializing;
		for (uint32 i = 0; i < config->attributes_count; i++)
			add_attribute(shader, &config->attributes[i]);

		for (uint32 i = 0; i < config->uniforms_count; i++)
		{
			if (config->uniforms[i].type == ShaderUniformType::Sampler)
				add_sampler(shader, &config->uniforms[i]);
			else
				add_uniform(shader, &config->uniforms[i]);
		}

		if (!Renderer::shader_init(shader))
		{
			destroy_shader(shader->id);
			SHMERRORV("Error initializing shader '%s'.", shader->name.c_str());
			return false;
		}

		shader->state = ShaderState::Initialized;
		system_state->lookup.set_value(shader->name.c_str(), shader->id);

		if (!system_state->material_shader_id.is_valid() && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_material_phong))
			system_state->material_shader_id = shader->id;
		else if (!system_state->terrain_shader_id.is_valid() && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_terrain))
			system_state->terrain_shader_id = shader->id;
		else if (!system_state->ui_shader_id.is_valid() && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_ui))
			system_state->ui_shader_id = shader->id;
		else if (!system_state->skybox_shader_id.is_valid() && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_skybox))
			system_state->skybox_shader_id = shader->id;
		else if (!system_state->color3D_shader_id.is_valid() && CString::equal(config->name, Renderer::RendererConfig::builtin_shader_name_color3D))
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

	ShaderId get_shader_id(const char* shader_name)
	{
		return system_state->lookup.get_value(shader_name);
	}

	Shader* get_shader(ShaderId shader_id)
	{
		if (shader_id >= system_state->config.max_shader_count || !system_state->shaders[shader_id].id.is_valid())
			return 0;

		return &system_state->shaders[shader_id];
	}

	Shader* get_shader(const char* shader_name)
	{
		ShaderId shader_id = get_shader_id(shader_name);
		return get_shader(shader_id);
	}

	void destroy_shader(ShaderId shader_id)
	{
		if (!shader_id.is_valid())
			return;

		Shader* shader = &system_state->shaders[shader_id];
		system_state->lookup.set_value(shader->name.c_str(), {});
		if (!shader->id.is_valid())
			return;

		Renderer::shader_destroy(shader);
		if (system_state->bound_shader_id == shader->id)
			system_state->bound_shader_id.invalidate();
		shader->id.invalidate();
		shader->state = ShaderState::Initialized;
	}

	void destroy_shader(const char* shader_name)
	{
		destroy_shader(get_shader_id(shader_name));
	}

	void bind_shader(ShaderId shader_id)
	{
		system_state->bound_shader_id = shader_id;
	}

	bool32 use_shader(ShaderId shader_id)
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
		ShaderId shader_id = get_shader_id(shader_name);
		if (!shader_id.is_valid())
			return false;

		return use_shader(shader_id);
	}

	ShaderUniformId get_uniform_index(Shader* shader, const char* uniform_name)
	{
		ShaderUniformId index = shader->uniform_lookup.get_value(uniform_name);
		if (!index.is_valid())
		{
			SHMERRORV("Shader '%s' does not have a uniform named '%s' registered.", shader->name.c_str(), uniform_name);
			return ShaderUniformId::invalid_value;
		}

		return shader->uniforms[index].index;
	}

	bool32 set_uniform(ShaderUniformId index, const void* value)
	{
		using namespace Renderer;

		Shader* shader = &system_state->shaders[system_state->bound_shader_id];
		ShaderUniform* uniform = &shader->uniforms[index];
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
		return Renderer::shader_set_uniform(shader, uniform, value);

	}

	bool32 set_uniform(const char* uniform_name, const void* value)
	{
		if (!system_state->bound_shader_id.is_valid()) 
		{
			SHMERROR("set_uniform - called without a shader in use.");
			return false;
		}

		Shader* shader = &system_state->shaders[system_state->bound_shader_id];
		ShaderUniformId index = get_uniform_index(shader, uniform_name);
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

	ShaderId get_material_phong_shader_id()
	{
		return system_state->material_shader_id;
	}

	ShaderId get_terrain_shader_id()
	{
		return system_state->terrain_shader_id;
	}

	ShaderId get_ui_shader_id()
	{
		return system_state->ui_shader_id;
	}

	ShaderId get_skybox_shader_id()
	{
		return system_state->skybox_shader_id;
	}

	ShaderId get_color3D_shader_id()
	{
		return system_state->color3D_shader_id;
	}

	static bool32 add_attribute(Shader* shader, const ShaderAttributeConfig* config)
	{
		using namespace Renderer;

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

	static bool32 add_sampler(Shader* shader, const ShaderUniformConfig* config)
	{
		using namespace Renderer;

		if (config->scope == ShaderScope::Local)
		{
			SHMERROR("add_sampler cannot add a sampler at local scope.");
			return false;
		}

		if (!config->name[0] || shader->state != ShaderState::Initializing)
			return false;

		uint32 location = 0;
		if (config->scope == ShaderScope::Global)
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
		return add_uniform(shader, config->name, 0, config->type, config->scope, location, true);
	}

	static bool32 add_uniform(Shader* shader, const ShaderUniformConfig* config)
	{
		return add_uniform(shader, config->name, config->size, config->type, config->scope, 0, false);
	}

	static bool32 add_uniform(Shader* shader, const char* uniform_name, uint32 size, ShaderUniformType type, ShaderScope scope, uint32 set_location, bool32 is_sampler)
	{
		using namespace Renderer;

		if (shader->uniforms.count + 1 > system_state->config.max_uniform_count)
		{
			SHMERRORV("A shader can only accept a combined maximum of %d uniforms and samplers at global, instance and local scopes.", system_state->config.max_uniform_count);
			return false;
		}

		if (!uniform_name[0] || shader->state != ShaderState::Initializing)
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