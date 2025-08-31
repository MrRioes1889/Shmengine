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
		uint16 count;
	};

	struct SystemState
	{
		Material default_material;
		Material default_ui_material;

		Sarray<ReferenceCounter> material_ref_counters;
		LinearHashedStorage<Material, MaterialId, Constants::max_material_name_length> material_storage;
	};

	static SystemState* system_state = 0;

	static bool8 _create_default_materials();

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

        _create_default_materials();

        return true;
    }

    void system_shutdown(void* state) 
    {
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
        system_state->material_ref_counters.free_data();

        system_state = 0;
    }

    MaterialId acquire_material_id(const char* name, Material** out_create_ptr)
    {
        *out_create_ptr = 0;
        MaterialId id;

        system_state->material_storage.acquire(name, &id, out_create_ptr);
        if (!id.is_valid())
        {
			SHMERROR("Failed to load material: Out of memory!");
			return MaterialId::invalid_value;
        }
        else if (!(*out_create_ptr))
        {
			system_state->material_ref_counters[id].count++;
            return id;
        }

        system_state->material_ref_counters[id].count = 1;
        return id;
    }

    void release_material_id(const char* name, Material** out_destroy_ptr) 
    {
        *out_destroy_ptr = 0;
        MaterialId id = system_state->material_storage.get_id(name);
		if (!id.is_valid())
            return;

        ReferenceCounter* ref_counter = &system_state->material_ref_counters[id];
        if (ref_counter->count > 0)
			ref_counter->count--;

        if (ref_counter->count > 0)
            return;
		
		system_state->material_storage.release(name, &id, out_destroy_ptr);
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
    
    static bool8 _create_default_materials() 
    {
        {
			MaterialProperty default_properties[2] = {};
			(*(Math::Vec4f*)default_properties[0].f32) = { 1.0f, 1.0f, 1.0f, 1.0f };
			CString::copy("diffuse_color", default_properties[0].name, MaterialProperty::max_name_length);
			default_properties[1].f32[0] = 8.0;
			CString::copy("shininess", default_properties[1].name, MaterialProperty::max_name_length);

			MaterialConfig config = {};
			config.type = MaterialType::PHONG;
			config.maps_count = 0;
			config.maps = 0;
			config.name = SystemConfig::default_material_name;
			config.properties_count = 2;
			config.properties = default_properties;
			config.shader_name = Renderer::RendererConfig::builtin_shader_name_material_phong;

			Renderer::material_init(&config, &system_state->default_material);
        }

        {
			MaterialProperty default_properties[1] = {};
			(*(Math::Vec4f*)default_properties[0].f32) = { 1.0f, 1.0f, 1.0f, 1.0f };
			CString::copy("diffuse_color", default_properties[0].name, MaterialProperty::max_name_length);

			MaterialConfig config = {};
			config.type = MaterialType::UI;
			config.maps_count = 0;
			config.maps = 0;
			config.name = SystemConfig::default_ui_material_name;
			config.properties_count = 1;
			config.properties = default_properties;
			config.shader_name = Renderer::RendererConfig::builtin_shader_name_ui;

			Renderer::material_init(&config, &system_state->default_ui_material);
        }

        return true;
    }

}