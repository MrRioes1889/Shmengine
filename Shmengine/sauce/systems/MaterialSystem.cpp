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

	static bool8 _create_default_material();
	static bool8 _create_default_ui_material();

    bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config) 
    {
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
			Renderer::material_destroy(material);
		}
		system_state->material_storage.destroy();

        // Destroy the default materials.
		Renderer::material_destroy(&system_state->default_ui_material);
        Renderer::material_destroy(&system_state->default_material);

        system_state = 0;
    }

    static bool8 _load_material(MaterialConfig* config, const char* name, const char* resource_name, bool8 auto_destroy)
    {
        MaterialId id;
        Material* m;

        system_state->material_storage.acquire(name, &id, &m);
        if (!id.is_valid())
        {
			SHMERROR("Failed to load material: Out of memory!");
			return true;
        }
        else if (!m)
        {
            SHMWARNV("Failed to load material: Material named '%s' already exists!", name);
            return system_state->material_storage.get_object(id);
        }

        if (config)
        {
			goto_if(!Renderer::material_init(config, m), fail);
		}
        else
        {
			goto_if(!Renderer::material_init_from_resource_async(resource_name, m), fail);
        }

        system_state->material_ref_counters[id] = { 0, auto_destroy };
        return m;

    fail:
		system_state->material_storage.release(name, &id, &m);
		SHMERRORV("Failed to load material '%s'.", name);
		return 0;
    }

    bool8 load_from_resource(const char* name, const char* resource_name, bool8 auto_destroy) 
    {
        return _load_material(0, name, resource_name, auto_destroy);
    }

    bool8 load_from_config(MaterialConfig* config, bool8 auto_destroy) 
    {
        return _load_material(config, config->name, 0, auto_destroy);
    }

    MaterialId acquire_reference(MaterialId id)
    {
        if (!system_state->material_storage.get_object(id))
            return MaterialId::invalid_value;

        system_state->material_ref_counters[id].reference_count++;
        return id;
    }

    MaterialId acquire_reference(const char* name)
    {
        MaterialId id = system_state->material_storage.get_id(name);
        if (!id.is_valid())
            return MaterialId::invalid_value;

        system_state->material_ref_counters[id].reference_count++;
        return id;
    }

    void release_reference(MaterialId id) 
    {
		if (!id.is_valid())
            return;

        ReferenceCounter* ref_counter = &system_state->material_ref_counters[id];
        if (ref_counter->reference_count > 0)
			ref_counter->reference_count--;

        if (!ref_counter->auto_destroy || ref_counter->reference_count > 0)
            return;
		
		Material* material = system_state->material_storage.get_object(id);
		if (!material)
		{
			SHMWARNV("Released material id %hu is not available for auto destruct. Skipping destruction.", id);
			return;
		}

		system_state->material_storage.release(material->name, &id, &material);
		Renderer::material_destroy(material);
    }

    void release_reference(const char* name) 
    {
		release_reference(system_state->material_storage.get_id(name));
	}

    Material* get_material(MaterialId id)
    {
        return system_state->material_storage.get_object(id);
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