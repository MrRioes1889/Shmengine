#include "ShaderSystem.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "containers/LinearStorage.hpp"

#include "renderer/RendererFrontend.hpp"
#include "TextureSystem.hpp"
#include "MaterialSystem.hpp"

#include "resources/loaders/ShaderLoader.hpp"

namespace ShaderSystem
{

	struct SystemState
	{
		LinearHashedStorage<Shader, ShaderId, Constants::max_shader_name_length> shader_storage;
		TextureMap default_texture_map;

		ShaderId bound_shader_id;
		ShaderId material_shader_id;
		ShaderId terrain_shader_id;
		ShaderId ui_shader_id;
		ShaderId skybox_shader_id;
		ShaderId color3D_shader_id;
	};

	static SystemState* system_state = 0;

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		if (sys_config->max_shader_count < 512)
			SHMWARN("shader_system_initialize - config.max_shader_count is recommended to be at least 512.");

		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->bound_shader_id.invalidate();

		system_state->material_shader_id.invalidate();
		system_state->terrain_shader_id.invalidate();
		system_state->ui_shader_id.invalidate();
		system_state->skybox_shader_id.invalidate();
		system_state->color3D_shader_id.invalidate();

		uint64 shader_storage_size = system_state->shader_storage.get_external_size_requirement(sys_config->max_shader_count);
		void* shader_storage_data = allocator_callback(allocator, shader_storage_size);
		system_state->shader_storage.init(sys_config->max_shader_count, AllocationTag::Array, shader_storage_data);

		return true;
	}

    void system_shutdown(void* state) 
    {
		auto iter = system_state->shader_storage.get_iterator();
		while (Shader* shader = iter.get_next())
		{
			MaterialId shader_id;
			system_state->shader_storage.release(shader->name.c_str(), &shader_id, &shader);
			Renderer::shader_destroy(shader);
		}
		system_state->shader_storage.destroy();

		Renderer::texture_map_destroy(&system_state->default_texture_map);

        system_state = 0;
    }

    ShaderId acquire_shader_id(const char* name, Shader** out_init_ptr)
    {
        MaterialId id;

        system_state->shader_storage.acquire(name, &id, out_init_ptr);
        if (!id.is_valid())
        {
			SHMERROR("Failed to load shader: Out of memory!");
			return ShaderId::invalid_value;
        }
        else if (!(*out_init_ptr))
        {
            return id;
        }

        return id;
    }

	void release_shader_id(const char* name, Shader** out_destroy_ptr)
	{
        *out_destroy_ptr = 0;
        ShaderId id = system_state->shader_storage.get_id(name);
		if (!id.is_valid())
            return;
		
		system_state->shader_storage.release(name, &id, out_destroy_ptr);
	}

	ShaderId get_shader_id(const char* name)
	{
		return system_state->shader_storage.get_id(name);
	}

	Shader* get_shader(ShaderId id)
    {
        return system_state->shader_storage.get_object(id);
    }

	ShaderId get_material_phong_shader_id()
	{
		if (!system_state->material_shader_id.is_valid())
			system_state->material_shader_id = system_state->shader_storage.get_id(Renderer::RendererConfig::builtin_shader_name_material_phong);

		return system_state->material_shader_id;
	}

	ShaderId get_terrain_shader_id()
	{
		if (!system_state->terrain_shader_id.is_valid())
			system_state->terrain_shader_id = system_state->shader_storage.get_id(Renderer::RendererConfig::builtin_shader_name_terrain);

		return system_state->terrain_shader_id;
	}

	ShaderId get_ui_shader_id()
	{
		if (!system_state->ui_shader_id.is_valid())
			system_state->ui_shader_id = system_state->shader_storage.get_id(Renderer::RendererConfig::builtin_shader_name_ui);

		return system_state->ui_shader_id;
	}

	ShaderId get_skybox_shader_id()
	{
		if (!system_state->skybox_shader_id.is_valid())
			system_state->skybox_shader_id = system_state->shader_storage.get_id(Renderer::RendererConfig::builtin_shader_name_skybox);

		return system_state->skybox_shader_id;
	}

	ShaderId get_color3D_shader_id()
	{
		if (!system_state->color3D_shader_id.is_valid())
			system_state->color3D_shader_id = system_state->shader_storage.get_id(Renderer::RendererConfig::builtin_shader_name_color3D);

		return system_state->color3D_shader_id;
	}

}