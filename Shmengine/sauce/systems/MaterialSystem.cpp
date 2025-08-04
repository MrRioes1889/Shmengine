#include "MaterialSystem.hpp"

#include "core/Logging.hpp"
#include "core/FrameData.hpp"
#include "memory/LinearAllocator.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "utility/Math.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "resources/loaders/MaterialLoader.hpp"
#include "systems/ShaderSystem.hpp"
#include "containers/LinearStorage.hpp"


namespace MaterialSystem
{

	struct ReferenceCounter
	{
		uint16 reference_count;
		bool8 auto_destroy;
	};

	struct SystemState
	{
		Material default_material;
		Material default_ui_material;

		Sarray<ReferenceCounter> material_ref_counters;
		LinearHashedStorage<Material, MaterialId, Constants::max_material_name_length> material_storage;
	};

	static SystemState* system_state = 0;

	static bool8 _create_material_from_resource(const char* name, Material* m);
	static bool8 _create_material(const MaterialConfig* config, Material* m);
	static void _destroy_material(Material* m);

	static bool8 _create_default_material();
	static bool8 _create_default_ui_material();

    static bool8 _assign_map(TextureMap* map, const TextureMapConfig* config, const char* material_name, Texture* default_texture);

    bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config) {

        SystemConfig* sys_config = (SystemConfig*)config;
        system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

        uint64 material_ref_counter_array_size = system_state->material_ref_counters.get_external_size_requirement(sys_config->max_material_count);
        void* material_ref_counter_array_data = allocator_callback(allocator, material_ref_counter_array_size);
        system_state->material_ref_counters.init(sys_config->max_material_count, 0, AllocationTag::Array, material_ref_counter_array_data);

        uint64 storage_data_size = system_state->material_storage.get_external_size_requirement(sys_config->max_material_count);
        void* storage_data = allocator_callback(allocator, storage_data_size);
        system_state->material_storage.init(sys_config->max_material_count, AllocationTag::Array, storage_data);

        _create_default_material();
        _create_default_ui_material();

        return true;

    }

    void system_shutdown(void* state) 
    {

        if (!system_state)
            return;

		auto iter = system_state->material_storage.get_iterator();
		while (Material* material = iter.get_next())
		{
			MaterialId material_id;
			system_state->material_storage.release(material->name, &material_id, &material);
			_destroy_material(material);
			system_state->material_storage.verify_write(material_id);
		}
		system_state->material_storage.destroy();

        // Destroy the default materials.
		_destroy_material(&system_state->default_ui_material);
		_destroy_material(&system_state->default_material);

        system_state = 0;

    }

    static Material* _acquire(const MaterialConfig* config, const char* name, bool8 auto_destroy)
    {
        if (CString::equal_i(name, SystemConfig::default_name))
            return &system_state->default_material;    

        if (CString::equal_i(name, SystemConfig::default_ui_name))
            return &system_state->default_material;

        if (CString::equal_i(name, SystemConfig::default_terrain_name))
            return &system_state->default_material;

        MaterialId id;
        Material* m;

        StorageReturnCode ret_code = system_state->material_storage.acquire(name, &id, &m);
		switch (ret_code)
		{
		case StorageReturnCode::OutOfMemory:
		{
			SHMERROR("Failed to create material: Out of memory!");
			return 0;
		}
		case StorageReturnCode::AlreadyExisted:
		{
            system_state->material_ref_counters[id].reference_count++;
            return system_state->material_storage.get_object(id);
		}
		}

        if (config)
        {
			goto_if(!_create_material(config, m), fail);
		}
        else
        {
			goto_if(!_create_material_from_resource(name, m), fail);
        }

        system_state->material_ref_counters[id] = { 1, auto_destroy };
        system_state->material_storage.verify_write(id);
        return m;

    fail:
		system_state->material_storage.revert_write(id);
		SHMERRORV("Failed to load material '%s'.", config->name);
		return 0;
    }

    Material* acquire(const char* name, bool8 auto_destroy) 
    {
        return _acquire(0, name, auto_destroy);
    }

    Material* acquire(const MaterialConfig* config, bool8 auto_destroy) 
    {
        return _acquire(config, config->name, auto_destroy);
    }

    void release(const char* name) {
        // Ignore release requests for the default material.
        if (CString::equal_i(name, SystemConfig::default_name) || 
            CString::equal_i(name, SystemConfig::default_ui_name) || 
            CString::equal_i(name, SystemConfig::default_terrain_name))
            return;

		MaterialId id = system_state->material_storage.get_id(name);
        if (!id.is_valid())
            return;

        ReferenceCounter* ref_counter = &system_state->material_ref_counters[id];
        if (ref_counter->reference_count > 0)
			ref_counter->reference_count--;

        if (ref_counter->reference_count == 0 && ref_counter->auto_destroy)
		{
            Material* m;
            system_state->material_storage.release(name, &id, &m);
			_destroy_material(m);
            system_state->material_storage.verify_write(id);
		}
    }

    static bool8 _assign_map(TextureMap* map, const TextureMapConfig* config, const char* material_name, Texture* default_texture)
    {
        map->filter_minify = config->filter_min;
        map->filter_magnify = config->filter_mag;
        map->repeat_u = config->repeat_u;
        map->repeat_v = config->repeat_v;
        map->repeat_w = config->repeat_w;

        if (config->texture_name && config->texture_name[0])
        {
            map->texture = TextureSystem::acquire(config->texture_name, true);
            if (!map->texture)
            {
                SHMWARNV("Unable to acquire texture '%s'.", config->texture_name);
                map->texture = default_texture;
            }
        }
        else
        {
            map->texture = default_texture;
        }

        if (!Renderer::texture_map_acquire_resources(map))
        {
            SHMERROR("Unable to acquire resources for texture map.");
            return false;
        }

        return true;
    }

    static bool8 _create_material_from_resource(const char* name, Material* m)
    {
        MaterialResourceData resource;
        if (!ResourceSystem::material_loader_load(name, &resource))
        {
            SHMERRORV("Failed to load material resources for material '%s'", name);
            return false;
        }

        MaterialConfig config = {};

        config.name = resource.name;
        config.shader_name = resource.shader_name;
        config.type = resource.type;
        config.properties_count = resource.properties.count;
        config.properties = resource.properties.data;

        Sarray<TextureMapConfig> map_configs(resource.maps.count, 0);
        for (uint32 i = 0; i < map_configs.capacity; i++)
        {
            map_configs[i].name = resource.maps[i].name;
            map_configs[i].texture_name = resource.maps[i].texture_name;
            map_configs[i].filter_min = resource.maps[i].filter_min;
            map_configs[i].filter_mag = resource.maps[i].filter_mag;
            map_configs[i].repeat_u = resource.maps[i].repeat_u;
            map_configs[i].repeat_v = resource.maps[i].repeat_v;
            map_configs[i].repeat_w = resource.maps[i].repeat_w;
        }

        config.maps_count = map_configs.capacity;
        config.maps = map_configs.data;

        // Now acquire from loaded config.
        bool8 success = _create_material(&config, m);

        map_configs.free_data();
        ResourceSystem::material_loader_unload(&resource);
        return success;
    }

    static bool8 _create_material(const MaterialConfig* config, Material* m)
    {
        Shader* shader = 0;
        CString::copy(config->name, m->name, Constants::max_material_name_length);
        m->shader_instance_id = Constants::max_u32;
        m->shader_id = ShaderSystem::get_shader_id(config->shader_name);
        m->type = config->type;

        switch (m->type)
        {
        case MaterialType::PHONG:
        {
            m->properties_size = sizeof(MaterialPhongProperties);
            m->properties = Memory::allocate(m->properties_size, AllocationTag::MaterialInstance);
            MaterialPhongProperties* properties = (MaterialPhongProperties*)m->properties;

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

            m->maps.init(3, 0);

            bool8 diffuse_assigned = false;
            bool8 specular_assigned = false;
            bool8 normal_assigned = false;

            for (uint32 i = 0; i < config->maps_count; i++)
            {
                if (CString::equal_i(config->maps[i].name, "diffuse"))
                {
                    goto_if(!_assign_map(&m->maps[0], &config->maps[i], m->name, TextureSystem::get_default_diffuse_texture()), fail)
                    diffuse_assigned = true;
                }
                else if (CString::equal_i(config->maps[i].name, "specular"))
                {
                    goto_if(!_assign_map(&m->maps[1], &config->maps[i], m->name, TextureSystem::get_default_specular_texture()), fail);
                    specular_assigned = true;
                }
                else if (CString::equal_i(config->maps[i].name, "normal"))
                {
                    goto_if(!_assign_map(&m->maps[2], &config->maps[i], m->name, TextureSystem::get_default_normal_texture()), fail);
                    normal_assigned = true;
                }
            }

            TextureMapConfig default_map_config = { 0 };
            default_map_config.filter_mag = default_map_config.filter_min = TextureFilter::LINEAR;
            default_map_config.repeat_u = default_map_config.repeat_v = default_map_config.repeat_w = TextureRepeat::MIRRORED_REPEAT;
            default_map_config.texture_name = 0;

            if (!diffuse_assigned) 
            {              
                default_map_config.name = "diffuse";
                goto_if(!_assign_map(&m->maps[0], &default_map_config, m->name, TextureSystem::get_default_diffuse_texture()), fail);
            }
            if (!specular_assigned) 
            {
                default_map_config.name = "specular";
                goto_if(!_assign_map(&m->maps[1], &default_map_config, m->name, TextureSystem::get_default_specular_texture()), fail);
            }
            if (!normal_assigned) 
            {
                default_map_config.name = "normal";
                goto_if(!_assign_map(&m->maps[2], &default_map_config, m->name, TextureSystem::get_default_normal_texture()), fail);
            }

            shader = ShaderSystem::get_shader(*config->shader_name ? config->shader_name : Renderer::RendererConfig::builtin_shader_name_material_phong);
            break;
        }
        case MaterialType::UI:
        {
            m->properties_size = sizeof(MaterialUIProperties);
            m->properties = Memory::allocate(m->properties_size, AllocationTag::MaterialInstance);
            MaterialUIProperties* properties = (MaterialUIProperties*)m->properties;

            for (uint32 i = 0; i < config->properties_count; i++)
            {
                MaterialProperty* prop = &config->properties[i];

                if (CString::equal_i(prop->name, "diffuse_color"))
                    properties->diffuse_color = *(Math::Vec4f*)prop->f32;
                else
                    SHMWARNV("Material property named %s not included in ui material type!", prop->name);
            }

            m->maps.init(1, 0);

            goto_if(!_assign_map(&m->maps[0], &config->maps[0], m->name, TextureSystem::get_default_diffuse_texture()), fail);
            shader = ShaderSystem::get_shader(*config->shader_name ? config->shader_name : Renderer::RendererConfig::builtin_shader_name_ui);
            break;
        }
        case MaterialType::CUSTOM:
        {
            goto_if_log(!config->shader_name[0], fail, "Failed to load shader. Shader name is required for custom materials!");
            shader = ShaderSystem::get_shader(config->shader_name);
            break;
        }
        default:
        {
			goto_if_log(true, fail, "Failed to load material. Type either unknown or not supported yet!");
            break;
        }
        }

        goto_if_log(!shader, fail, "No valid shader for material found!");
        goto_if_log(!Renderer::shader_acquire_instance_resources(shader, m->maps.capacity, &m->shader_instance_id), fail, "Failed to acquire renderer resources for material.");

        return true;

    fail:
        SHMERRORV("Failed to create material '%s'.", m->name);
        _destroy_material(m);
        return false;
    }

    static void _destroy_material(Material* m)
    {
        // Release texture references.
        for (uint32 i = 0; i < m->maps.capacity; i++)
        {
            TextureSystem::release(m->maps[i].texture->name);
            Renderer::texture_map_release_resources(&m->maps[i]);
        }

        // Release renderer resources.
        if (m->shader_id.is_valid() && m->shader_instance_id != Constants::max_u32)
        {
            Renderer::shader_release_instance_resources(ShaderSystem::get_shader(m->shader_id), m->shader_instance_id);
            m->shader_id.invalidate();
        }

        if (m->properties)
            Memory::free_memory(m->properties);

        // Zero it out, invalidate IDs.
        Memory::zero_memory(m, sizeof(Material));
        m->id.invalidate();
        m->shader_instance_id = Constants::max_u32;
    }

    Material* get_default_material()
    {
        return &system_state->default_material;
    }

    Material* get_default_ui_material()
    {
        return &system_state->default_ui_material;
    }
    
    static bool8 _create_default_material() {

        Material* mat = &system_state->default_material;

        Memory::zero_memory(mat, sizeof(Material));
        mat->id.invalidate();
        mat->type = MaterialType::PHONG;
        CString::copy(SystemConfig::default_name, mat->name, Constants::max_material_name_length);

        mat->properties_size = sizeof(MaterialPhongProperties);
        static MaterialPhongProperties default_properties = {};
        default_properties.diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        default_properties.shininess = 8.0f;
        mat->properties = &default_properties;

        Texture* default_textures[3] = { TextureSystem::get_default_diffuse_texture(), TextureSystem::get_default_specular_texture(), TextureSystem::get_default_normal_texture() };
        const uint32 maps_count = 3;
        static TextureMap default_maps[maps_count] = {};
        mat->maps.init(maps_count, SarrayFlags::ExternalMemory, AllocationTag::MaterialInstance, default_maps);
        for (uint32 i = 0; i < mat->maps.capacity; i++)
        {
            mat->maps[i].filter_minify = mat->maps[i].filter_magnify = TextureFilter::LINEAR;
            mat->maps[i].repeat_u = mat->maps[i].repeat_v = mat->maps[i].repeat_w = TextureRepeat::REPEAT;
            mat->maps[i].texture = default_textures[i % 3];
        }

        mat->shader_id = ShaderSystem::get_shader_id(Renderer::RendererConfig::builtin_shader_name_material_phong);
        Shader* s = ShaderSystem::get_shader(mat->shader_id);
        if (!Renderer::shader_acquire_instance_resources(s, maps_count, &mat->shader_instance_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }


        return true;
    }

    static bool8 _create_default_ui_material() {

        Material* mat = &system_state->default_ui_material;

        Memory::zero_memory(mat, sizeof(Material));
        mat->id.invalidate();
        mat->type = MaterialType::UI;
        CString::copy(SystemConfig::default_ui_name, mat->name, Constants::max_material_name_length);

        mat->properties_size = sizeof(MaterialUIProperties);
        static MaterialUIProperties default_properties = {};
        default_properties.diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        mat->properties = &default_properties;

        const uint32 maps_count = 1;
        static TextureMap default_maps[maps_count] = {};
        mat->maps.init(maps_count, SarrayFlags::ExternalMemory, AllocationTag::MaterialInstance, default_maps);
        for (uint32 i = 0; i < mat->maps.capacity; i++)
        {
            mat->maps[i].filter_minify = mat->maps[i].filter_magnify = TextureFilter::LINEAR;
            mat->maps[i].repeat_u = mat->maps[i].repeat_v = mat->maps[i].repeat_w = TextureRepeat::REPEAT;
            mat->maps[i].texture = TextureSystem::get_default_texture();
        }

        mat->shader_id = ShaderSystem::get_shader_id(Renderer::RendererConfig::builtin_shader_name_ui);
        Shader* s = ShaderSystem::get_shader(mat->shader_id);
        if (!Renderer::shader_acquire_instance_resources(s, maps_count, &mat->shader_instance_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }


        return true;
    }

}