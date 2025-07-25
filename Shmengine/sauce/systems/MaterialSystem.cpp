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
#include "containers/Sarray.hpp"


namespace MaterialSystem
{

	struct MaterialReference
	{
		MaterialId id;
		uint16 reference_count;
		bool8 auto_unload;
	};

	struct SystemState
	{
		Material default_material;
		Material default_ui_material;

		Sarray<Material> materials;
		HashtableRH<MaterialReference> lookup_table;
	};

	static SystemState* system_state = 0;

	static bool8 load_material_from_resource(const char* name, Material* m);
	static bool8 load_material(const MaterialConfig* config, Material* m);
	static void destroy_material(Material* m);

	static bool8 create_default_material();
	static bool8 create_default_ui_material();

	static bool8 add_reference(const char* name, bool8 auto_unload, MaterialId* out_material_id, bool8* out_load);
	static MaterialId remove_reference(const char* name);
    static bool8 assign_map(TextureMap* map, const TextureMapConfig* config, const char* material_name, Texture* default_texture);

    bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config) {

        SystemConfig* sys_config = (SystemConfig*)config;
        system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

        uint64 material_array_size = system_state->materials.get_external_size_requirement(sys_config->max_material_count);
        void* material_array_data = allocator_callback(allocator, material_array_size);
        system_state->materials.init(sys_config->max_material_count, 0, AllocationTag::Array, material_array_data);

        uint64 hashtable_data_size = system_state->lookup_table.get_external_size_requirement(sys_config->max_material_count);
        void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
        system_state->lookup_table.init(sys_config->max_material_count, HashtableRHFlag::ExternalMemory, AllocationTag::Dict, hashtable_data);

        // Invalidate all materials in the array.
        for (uint32 i = 0; i < system_state->materials.capacity; ++i) {
            system_state->materials[i].id.invalidate();
            system_state->materials[i].shader_instance_id = Constants::max_u32;
        }

        create_default_material();
        create_default_ui_material();

        return true;

    }

    void system_shutdown(void* state) 
    {

        if (!system_state)
            return;

        // Invalidate all materials in the array.
        for (uint32 i = 0; i < system_state->materials.capacity; ++i) 
        {
            if (system_state->materials[i].id.is_valid()) 
                destroy_material(&system_state->materials[i]);
        }

        // Destroy the default materials.
		destroy_material(&system_state->default_ui_material);
		destroy_material(&system_state->default_material);

        system_state = 0;

    }

    static Material* acquire_(const MaterialConfig* config, const char* name, bool8 auto_unload)
    {
        if (CString::equal_i(name, SystemConfig::default_name))
            return &system_state->default_material;    

        if (CString::equal_i(name, SystemConfig::default_ui_name))
            return &system_state->default_material;

        if (CString::equal_i(name, SystemConfig::default_terrain_name))
            return &system_state->default_material;

        MaterialId id = MaterialId::invalid_value;
        bool8 load = false;
        Material* m = 0;

        if (!add_reference(name, auto_unload, &id, &load))
        {
            SHMERRORV("Failed to add reference to material system.");
            return 0;
        }
		m = &system_state->materials[id];

        bool8 loaded = true;
        if (load)
        {
            if (config)
                loaded = load_material(config, m);
            else
                loaded = load_material_from_resource(name, m);
        }

		if (!loaded) 
		{
			destroy_material(m);
			SHMERRORV("Failed to load material '%s'.", config->name);
			return 0;
		}

        return m;
    }

    Material* acquire(const char* name, bool8 auto_unload) 
    {
        return acquire_(0, name, auto_unload);
    }

    Material* acquire(const MaterialConfig* config, bool8 auto_unload) 
    {
        return acquire_(config, config->name, auto_unload);
    }

    void release(const char* name) {
        // Ignore release requests for the default material.
        if (CString::equal_i(name, SystemConfig::default_name) || 
            CString::equal_i(name, SystemConfig::default_ui_name) || 
            CString::equal_i(name, SystemConfig::default_terrain_name))
            return;

		MaterialId destroy_id = remove_reference(name);
		if (destroy_id.is_valid())
		{
			Material* m = &system_state->materials[destroy_id];
			destroy_material(m);
		}
    }

    void dump_system_stats()
    {
        for (uint32 i = 0; i < system_state->materials.capacity; i++) 
        {
            Material* m = &system_state->materials[i];
            if (!m->id.is_valid())
                continue;

            const MaterialReference* r = system_state->lookup_table.get(m->name);
            if (r) {
				SHMTRACEV("Material name: %s", m->name);
				SHMTRACEV("Material ref (handle/refCount): (%u/%u)", r->id, r->reference_count);
            }
        }
    }

    static bool8 assign_map(TextureMap* map, const TextureMapConfig* config, const char* material_name, Texture* default_texture)
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

    static bool8 load_material_from_resource(const char* name, Material* m)
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
        bool8 success = load_material(&config, m);

        map_configs.free_data();
        ResourceSystem::material_loader_unload(&resource);
        return success;
    }

    static bool8 load_material(const MaterialConfig* config, Material* m)
    {
        m->shader_id = ShaderSystem::get_shader_id(config->shader_name);
        m->type = config->type;

        if (m->type == MaterialType::PHONG)
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
                    if (!assign_map(&m->maps[0], &config->maps[i], m->name, TextureSystem::get_default_diffuse_texture()))
                        return false;
                    diffuse_assigned = true;
                }
                else if (CString::equal_i(config->maps[i].name, "specular"))
                {
                    if (!assign_map(&m->maps[1], &config->maps[i], m->name, TextureSystem::get_default_specular_texture()))
                        return false;
                    specular_assigned = true;
                }
                else if (CString::equal_i(config->maps[i].name, "normal"))
                {
                    if (!assign_map(&m->maps[2], &config->maps[i], m->name, TextureSystem::get_default_normal_texture()))
                        return false;
                    normal_assigned = true;
                }
            }

            TextureMapConfig default_map_config = { 0 };
            default_map_config.filter_mag = default_map_config.filter_min = TextureFilter::LINEAR;
            default_map_config.repeat_u = default_map_config.repeat_v = default_map_config.repeat_w = TextureRepeat::MIRRORED_REPEAT;
            default_map_config.texture_name = 0;

            if (!diffuse_assigned) {              
                default_map_config.name = "diffuse";
                if (!assign_map(&m->maps[0], &default_map_config, m->name, TextureSystem::get_default_diffuse_texture())) {
                    return false;
                }
            }
            if (!specular_assigned) {
                default_map_config.name = "specular";
                if (!assign_map(&m->maps[1], &default_map_config, m->name, TextureSystem::get_default_specular_texture())) {
                    return false;
                }
            }
            if (!normal_assigned) {
                default_map_config.name = "normal";
                if (!assign_map(&m->maps[2], &default_map_config, m->name, TextureSystem::get_default_normal_texture())) {
                    return false;
                }
            }
        }
        else if (m->type == MaterialType::UI)
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

            if (!assign_map(&m->maps[0], &config->maps[0], m->name, TextureSystem::get_default_diffuse_texture())) {
                return false;
            }
        }
        else
        {
            SHMERROR("Failed to load material. Type either unknown or not supported yet!");
            return false;
        }

        Shader* shader = 0;
        if (m->type == MaterialType::PHONG)
        {
            shader = ShaderSystem::get_shader(*config->shader_name ? config->shader_name : Renderer::RendererConfig::builtin_shader_name_material_phong);
        }
        if (m->type == MaterialType::UI)
        {
            shader = ShaderSystem::get_shader(*config->shader_name ? config->shader_name : Renderer::RendererConfig::builtin_shader_name_ui);
        }
        if (m->type == MaterialType::CUSTOM)
        {
            if (!*config->shader_name)
            {
                SHMERROR("Failed to load shader. Shader name is required for custom materials!");
                return false;
            }

            shader = ShaderSystem::get_shader(config->shader_name);
        }
        else
        {
            if (!*config->shader_name)
            {
                SHMERROR("Failed to load material. Type either unknown or not supported yet!");
                return false;
            }
        }

        if (!shader)
        {
            SHMERROR("Failed to load material. No valid shader found!");
            return false;
        }

        bool8 res = Renderer::shader_acquire_instance_resources(shader, m->maps.capacity, &m->shader_instance_id);
        if (!res)
            SHMERRORV("Failed to acquire renderer resources for material '%s'.", m->name);

        return res;
    }

    static void destroy_material(Material* m)
    {
        system_state->lookup_table.remove_entry(m->name);

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
    
    static bool8 create_default_material() {

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

        Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_material_phong);
        if (!Renderer::shader_acquire_instance_resources(s, maps_count, &mat->shader_instance_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        mat->shader_id = s->id;

        return true;
    }

    static bool8 create_default_ui_material() {

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

        Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui);
        if (!Renderer::shader_acquire_instance_resources(s, maps_count, &mat->shader_instance_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        mat->shader_id = s->id;

        return true;
    }

	static bool8 add_reference(const char* name, bool8 auto_unload, MaterialId* out_material_id, bool8* out_load)
	{
		*out_material_id = MaterialId::invalid_value;
		*out_load = false;
		MaterialReference* ref = system_state->lookup_table.get(name);
		if (ref)
		{
			ref->reference_count++;
			*out_material_id = ref->id;
			return true;
		}

		MaterialId new_id = MaterialId::invalid_value;
		for (uint16 i = 0; i < system_state->materials.capacity; i++)
		{
			if (!system_state->materials[i].id.is_valid())
			{
				new_id = i;
				break;
			}
		}

		if (!new_id.is_valid())
		{
			SHMFATAL("Could not acquire new texture to texture system since no free slots are left!");
			return false;
		}

		Material* m = &system_state->materials[new_id];

		m->id = new_id;
        CString::copy(name, m->name, Constants::max_material_name_length);
		MaterialReference new_ref = { .id = new_id, .reference_count = 1, .auto_unload = auto_unload };
		ref = system_state->lookup_table.set_value(m->name, new_ref);
		*out_material_id = ref->id;
        *out_load = true;

		return true;
	}

	static MaterialId remove_reference(const char* name)
	{
		MaterialReference* ref = system_state->lookup_table.get(name);
		if (!ref)
		{
			SHMWARNV("Tried to release non-existent texture: '%s'", name);
			return MaterialId::invalid_value;
		}
		else if (!ref->reference_count)
		{
			SHMWARN("Tried to release a texture where autorelease=false, but references was already 0.");
			return MaterialId::invalid_value;
		}

		ref->reference_count--;
		if (ref->reference_count == 0 && ref->auto_unload)
		{
			return ref->id;
		}

		return MaterialId::invalid_value;
	}
}