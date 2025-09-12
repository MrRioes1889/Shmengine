#include "renderer/RendererFrontend.hpp"
#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "resources/loaders/MaterialLoader.hpp"
#include "utility/math/Transform.hpp"

#include "systems/JobSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/ShaderSystem.hpp"

namespace Renderer
{
	static bool8 _material_init(MaterialConfig* config, Material* out_material);
    static void _material_destroy(Material* material);

	static void _material_init_from_resource_job_success(void* params);
	static void _material_init_from_resource_job_fail(void* params);
	static bool8 _material_init_from_resource_job(uint32 thread_index, void* user_data);

	bool8 material_init(MaterialConfig* config, Material* out_material)
	{
		if (out_material->state >= ResourceState::Initialized)
			return false;

		out_material->state = ResourceState::Initializing;
		_material_init(config, out_material);

        Shader* shader = ShaderSystem::get_shader(out_material->shader_id);
        out_material->shader_instance_id = Renderer::shader_acquire_instance(shader);

        for (uint32 i = 0; i < out_material->maps.capacity && i < config->texture_count; i++)
        {
            if (!config->texture_names[i] || !config->texture_names[i][0])
                continue;

			out_material->maps[i].texture = TextureSystem::acquire(config->texture_names[i], TextureType::Plane, true);
        }

		out_material->state = ResourceState::Initialized;

		return true;
	}

	struct MaterialLoadParams
	{
		Material* out_material;
		const char* resource_name;
		MaterialResourceData resource;
	};

	bool8 material_init_from_resource_async(const char* name, Material* out_material)
	{
		if (out_material->state >= ResourceState::Initialized)
			return false;

		out_material->state = ResourceState::Initializing;

		JobSystem::JobInfo job = JobSystem::job_create(_material_init_from_resource_job, _material_init_from_resource_job_success, _material_init_from_resource_job_fail, sizeof(MaterialLoadParams));
		MaterialLoadParams* params = (MaterialLoadParams*)job.user_data;
		params->out_material = out_material;
		params->resource_name = name;
		params->resource = {};
		JobSystem::submit(job);

		return true;
	}

	bool8 material_destroy(Material* material)
	{
		if (material->state != ResourceState::Initialized)
			return false;

		material->state = ResourceState::Destroying;
        _material_destroy(material);
		material->state = ResourceState::Destroyed;
		return true;
	}

	static bool8 _material_init(MaterialConfig* config, Material* out_material)
	{
        CString::copy(config->name, out_material->name, Constants::max_material_name_length);
        out_material->shader_instance_id.invalidate();
        out_material->shader_id.invalidate();
        out_material->type = config->type;

		TextureMapConfig default_map_config = { 0 };
		default_map_config.filter_magnify = default_map_config.filter_minify = TextureFilter::LINEAR;
		default_map_config.repeat_u = default_map_config.repeat_v = default_map_config.repeat_w = TextureRepeat::MIRRORED_REPEAT;

        switch (out_material->type)
        {
        case MaterialType::PHONG:
        {
            out_material->properties_size = sizeof(MaterialPhongProperties);
            out_material->properties = Memory::allocate(out_material->properties_size, AllocationTag::MaterialInstance);
            MaterialPhongProperties* properties = (MaterialPhongProperties*)out_material->properties;

            for (uint32 i = 0; i < config->properties_count; i++)
            {
                MaterialProperty* prop = &config->properties[i];

                if (CString::equal_i(prop->name, "diffuse_color"))
                    properties->diffuse_color = *(Math::Vec4f*)prop->f32;
                else if (CString::equal_i(prop->name, "shininess"))
                    properties->shininess = prop->f32[0];
                else
                    SHMWARNV("Material property named %s not included in ui material type!", prop->name);
            }

            out_material->maps.init(3, 0);


            goto_if(!Renderer::texture_map_init(config->texture_count > 0 ? &config->map_configs[0] : &default_map_config, TextureSystem::get_default_diffuse_texture(), &out_material->maps[0]), fail);
            goto_if(!Renderer::texture_map_init(config->texture_count > 1 ? &config->map_configs[1] : &default_map_config, TextureSystem::get_default_specular_texture(), &out_material->maps[1]), fail);
            goto_if(!Renderer::texture_map_init(config->texture_count > 2 ? &config->map_configs[2] : &default_map_config, TextureSystem::get_default_normal_texture(), &out_material->maps[2]), fail);

            out_material->shader_id = ShaderSystem::get_shader_id(*config->shader_name ? config->shader_name : Renderer::RendererConfig::builtin_shader_name_material_phong);
            break;
        }
        case MaterialType::UI:
        {
            out_material->properties_size = sizeof(MaterialUIProperties);
            out_material->properties = Memory::allocate(out_material->properties_size, AllocationTag::MaterialInstance);
            MaterialUIProperties* properties = (MaterialUIProperties*)out_material->properties;

            for (uint32 i = 0; i < config->properties_count; i++)
            {
                MaterialProperty* prop = &config->properties[i];

                if (CString::equal_i(prop->name, "diffuse_color"))
                    properties->diffuse_color = *(Math::Vec4f*)prop->f32;
                else
                    SHMWARNV("Material property named %s not included in ui material type!", prop->name);
            }

            out_material->maps.init(1, 0);

            goto_if(!Renderer::texture_map_init(config->texture_count > 0 ? &config->map_configs[0] : &default_map_config, TextureSystem::get_default_diffuse_texture(), &out_material->maps[0]), fail);
            out_material->shader_id = ShaderSystem::get_shader_id(*config->shader_name ? config->shader_name : Renderer::RendererConfig::builtin_shader_name_ui);
            break;
        }
        /*
        case MaterialType::CUSTOM:
        {
            goto_if_log(!config->shader_name[0], fail, "Failed to load shader. Shader name is required for custom materials!");
            out_material->maps[0].texture = TextureSystem::get_default_diffuse_texture();
            out_material->shader_id = ShaderSystem::get_shader_id(config->shader_name);
            break;
        }
        */
        default:
        {
			goto_if_log(true, fail, "Failed to load material. Type either unknown or not supported yet!");
            break;
        }
        }
        
        goto_if_log(!out_material->shader_id.is_valid(), fail, "Failed to find valid shader for material.");

        return true;

    fail:
        SHMERRORV("Failed to create material '%s'.", out_material->name);
        _material_destroy(out_material);
        return false;
	}

    static void _material_destroy(Material* material)
    {
        // Release texture references.
        for (uint32 i = 0; i < material->maps.capacity; i++)
        {
            TextureSystem::release(material->maps[i].texture->name);
            Renderer::texture_map_destroy(&material->maps[i]);
        }

        // Release renderer resources.
        if (material->shader_id.is_valid() && material->shader_instance_id != Constants::max_u32)
        {
            Renderer::shader_release_instance(ShaderSystem::get_shader(material->shader_id), material->shader_instance_id);
            material->shader_id.invalidate();
        }

        if (material->properties)
            Memory::free_memory(material->properties);

        // Zero it out, invalidate IDs.
        Memory::zero_memory(material, sizeof(Material));
        material->shader_instance_id.invalidate();
    }

	static void _material_init_from_resource_job_success(void* params) 
	{
		MaterialLoadParams* load_params = (MaterialLoadParams*)params;
		Material* material = load_params->out_material;  

		MaterialConfig config = ResourceSystem::material_loader_get_config_from_resource(&load_params->resource);
        Shader* shader = ShaderSystem::get_shader(material->shader_id);
        material->shader_instance_id = Renderer::shader_acquire_instance(shader);

        for (uint32 i = 0; i < material->maps.capacity && i < config.texture_count; i++)
        {
            if (!config.texture_names[i] || !config.texture_names[i][0])
                continue;

			material->maps[i].texture = TextureSystem::acquire(config.texture_names[i], TextureType::Plane, true);
        }
        
		load_params->out_material->state = ResourceState::Initialized;

		ResourceSystem::material_loader_unload(&load_params->resource);
		SHMTRACEV("Successfully loaded material '%s'.", material->name);
	}

	static void _material_init_from_resource_job_fail(void* params) 
	{
		MaterialLoadParams* load_params = (MaterialLoadParams*)params;
		Material* material = load_params->out_material;

		ResourceSystem::material_loader_unload(&load_params->resource);
		_material_destroy(material);
        material->state = ResourceState::Destroyed;
		SHMERRORV("Failed to load material '%s'.", material->name);
	}

	static bool8 _material_init_from_resource_job(uint32 thread_index, void* user_data) 
	{
		MaterialLoadParams* load_params = (MaterialLoadParams*)user_data;
		Material* material = load_params->out_material;

		if (!ResourceSystem::material_loader_load(load_params->resource_name, &load_params->resource))
		{
			SHMERRORV("Failed to load material from resource '%s'", load_params->resource_name);
			return false;
		}

		MaterialConfig config = ResourceSystem::material_loader_get_config_from_resource(&load_params->resource);
		return _material_init(&config, material);
	}

}
