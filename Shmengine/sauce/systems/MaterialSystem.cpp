#include "MaterialSystem.hpp"

#include "core/Logging.hpp"
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
		uint32 reference_count;
		uint32 handle;
		bool32 auto_release;
	};

	struct SystemState
	{
		SystemConfig config;

		Material default_material;
		Material default_ui_material;

		Material* registered_materials;
		Hashtable<MaterialReference> registered_material_table;
	};

	static SystemState* system_state = 0;

	static bool32 load_material(const MaterialConfig* config, Material* m);
	static void destroy_material(Material* m);

	static bool32 create_default_material();
	static bool32 create_default_ui_material();

    static bool32 assign_map(TextureMap* map, const TextureMapConfig* config, const char* material_name, Texture* default_texture);

    bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config) {

        SystemConfig* sys_config = (SystemConfig*)config;
        system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

        system_state->config = *sys_config;

        uint64 texture_array_size = sizeof(Material) * sys_config->max_material_count;
        system_state->registered_materials = (Material*)allocator_callback(allocator, texture_array_size);

        uint64 hashtable_data_size = sizeof(MaterialReference) * sys_config->max_material_count;
        void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
        system_state->registered_material_table.init(sys_config->max_material_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

        // Fill the hashtable with invalid references to use as a default.
        MaterialReference invalid_ref;
        invalid_ref.auto_release = false;
        invalid_ref.handle = INVALID_ID;  // Primary reason for needing default values.
        invalid_ref.reference_count = 0;
        system_state->registered_material_table.floodfill(invalid_ref);

        // Invalidate all materials in the array.
        uint32 count = system_state->config.max_material_count;
        for (uint32 i = 0; i < count; ++i) {
            system_state->registered_materials[i].id = INVALID_ID;
            system_state->registered_materials[i].generation = INVALID_ID;
            system_state->registered_materials[i].shader_instance_id = INVALID_ID;
            system_state->registered_materials[i].render_frame_number = INVALID_ID;
        }

        return true;

    }

    void system_shutdown(void* state) 
    {

        if (!system_state)
            return;

        // Invalidate all materials in the array.
        uint32 count = system_state->config.max_material_count;
        for (uint32 i = 0; i < count; ++i) 
        {
            if (system_state->registered_materials[i].id != INVALID_ID) 
                destroy_material(&system_state->registered_materials[i]);
        }

        // Destroy the default material.
        if (system_state->default_ui_material.id)
         destroy_material(&system_state->default_ui_material);
        if (system_state->default_material.id)
            destroy_material(&system_state->default_material);

        system_state = 0;

    }

    Material* acquire(const char* name) {
        // Load the given material configuration from disk.
        MaterialResourceData resource;

        if (!ResourceSystem::material_loader_load(name, 0, &resource))
        {
            SHMERRORV("load_mt_file - Failed to load material resources for material '%s'", name);
            return 0;
        }

        MaterialConfig config = {};

        config.name = resource.name;
        config.shader_name = resource.shader_name;
        config.type = resource.type;
        config.auto_release = resource.auto_release;
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
        Material* mat = acquire_from_config(&config);

        map_configs.free_data();
        ResourceSystem::material_loader_unload(&resource);
        return mat;
    }

    static Material* acquire_reference(const char* name, bool32 auto_release, bool32* found)
    {

        MaterialReference& ref = system_state->registered_material_table.get_ref(name);

        // This can only be changed the first time a material is loaded.
        if (ref.reference_count == 0)
            ref.auto_release = auto_release;

        ref.reference_count++;
        if (ref.handle == INVALID_ID) 
        {
            // This means no material exists here. Find a free index first.
            uint32 count = system_state->config.max_material_count;
            Material* m = 0;
            for (uint32 i = 0; i < count; ++i) 
            {
                if (system_state->registered_materials[i].id == INVALID_ID) 
                {
                    // A free slot has been found. Use its index as the handle.
                    ref.handle = i;
                    m = &system_state->registered_materials[i];
                    break;
                }
            }

            // Make sure an empty slot was actually found.
            if (!m || ref.handle == INVALID_ID)
            {
                SHMFATAL("material_system_acquire - Material system cannot hold anymore materials. Adjust configuration to allow more.");
                return 0;
            }
            

            // Also use the handle as the material id.
            m->id = ref.handle;
        }
        else 
        {
            *found = true;
        }

        return &system_state->registered_materials[ref.handle];

    }

    Material* acquire_from_config(const MaterialConfig* config) 
    {

        if (CString::equal_i(config->name, SystemConfig::default_name))
            return &system_state->default_material;    

        if (CString::equal_i(config->name, SystemConfig::default_ui_name))
            return &system_state->default_material;

        if (CString::equal_i(config->name, SystemConfig::default_terrain_name))
            return &system_state->default_material;

        bool32 exists = false;
        Material* m = acquire_reference(config->name, config->auto_release, &exists);

        if (!exists)
        {
            if (!load_material(config, m)) 
            {
                SHMERRORV("Failed to load material '%s'.", config->name);
                return 0;
            }

            if (m->generation == INVALID_ID) 
                m->generation = 0;
            else
                m->generation++;
        }

        return m;

    }

    void release(const char* name) {
        // Ignore release requests for the default material.
        if (CString::equal_i(name, SystemConfig::default_name) || CString::equal_i(name, SystemConfig::default_ui_name) || CString::equal_i(name, SystemConfig::default_terrain_name)) {
            return;
        }
        MaterialReference ref = system_state->registered_material_table.get_value(name);

        char name_copy[max_material_name_length];
        CString::copy(name, name_copy, max_material_name_length);

        if (ref.reference_count == 0) {
            SHMWARNV("Tried to release non-existent material: '%s'", name_copy);
            return;
        }

        ref.reference_count--;
        if (ref.reference_count == 0 && ref.auto_release) {
            Material* m = &system_state->registered_materials[ref.handle];

            // Destroy/reset material.
            destroy_material(m);

            // Reset the reference.
            ref.handle = INVALID_ID;
            ref.auto_release = false;
            //SHMTRACEV("Released material '%s'., Material unloaded because reference count=0 and auto_release=true.", name_copy);
        }
        else {
            //SHMTRACEV("Released material '%s', now has a reference count of '%i' (auto_release=%s).", name_copy, ref.reference_count, ref.auto_release ? "true" : "false");
        }

        system_state->registered_material_table.set_value(name_copy, ref);

    }

    void dump_system_stats()
    {
        for (uint32 i = 0; i < system_state->registered_material_table.element_count; i++) 
        {
            MaterialReference* r = &system_state->registered_material_table.buffer[i];
            if (r->reference_count > 0 || r->handle != INVALID_ID) {
                if (r->handle != INVALID_ID) 
                {
                    SHMTRACEV("Material name: %s", system_state->registered_materials[r->handle].name);
                    SHMTRACEV("Material ref (handle/refCount): (%u/%u)", r->handle, r->reference_count);
                }
                else
                {
                    SHMTRACEV("Found handleless material ref (handle/refCount): (%u/%u)", r->handle, r->reference_count);
                }                         
            }
        }
    }

    static bool32 assign_map(TextureMap* map, const TextureMapConfig* config, const char* material_name, Texture* default_texture)
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

    static bool32 load_material(const MaterialConfig* config, Material* m)
    {
        Memory::zero_memory(m, sizeof(Material));

        // name
        CString::copy(config->name, m->name, max_material_name_length);

        m->shader_id = ShaderSystem::get_id(config->shader_name);
        m->type = config->type;

        if (m->type == MaterialType::PHONG)
        {

            m->properties_size = sizeof(MaterialPhongProperties);
            m->properties = Memory::allocate(m->properties_size, AllocationTag::MATERIAL_INSTANCE);
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

            bool32 diffuse_assigned = false;
            bool32 specular_assigned = false;
            bool32 normal_assigned = false;

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
            m->properties = Memory::allocate(m->properties_size, AllocationTag::MATERIAL_INSTANCE);
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

        Sarray<TextureMap*> maps(m->maps.capacity, 0);
        for (uint32 i = 0; i < m->maps.capacity; i++)
            maps[i] = &m->maps[i];

        bool32 res = Renderer::shader_acquire_instance_resources(shader, maps.capacity, maps.data, &m->shader_instance_id);
        if (!res)
            SHMERRORV("Failed to acquire renderer resources for material '%s'.", m->name);

        maps.free_data();

        return res;
    }

    static void destroy_material(Material* m)
    {
        //SHMTRACEV("Destroying material '%s'...", m->name);

        // Release texture references.
        for (uint32 i = 0; i < m->maps.capacity; i++)
        {
            TextureSystem::release(m->maps[i].texture->name);
            Renderer::texture_map_release_resources(&m->maps[i]);
        }

        // Release renderer resources.
        if (m->shader_id != INVALID_ID && m->shader_instance_id != INVALID_ID)
        {
            Renderer::shader_release_instance_resources(ShaderSystem::get_shader(m->shader_id), m->shader_instance_id);
            m->shader_id = INVALID_ID;
        }

        if (m->properties)
            Memory::free_memory(m->properties);

        // Zero it out, invalidate IDs.
        Memory::zero_memory(m, sizeof(Material));
        m->id = INVALID_ID;
        m->generation = INVALID_ID;
        m->shader_instance_id = INVALID_ID;
        m->render_frame_number = INVALID_ID;
    }

    Material* get_default_material()
    {
        if (!system_state->default_material.id)
            create_default_material();

        return &system_state->default_material;
    }

    Material* get_default_ui_material()
    {
        if (!system_state->default_ui_material.id)
            create_default_ui_material();

        return &system_state->default_ui_material;
    }
    
    static bool32 create_default_material() {

        Material* mat = &system_state->default_material;

        Memory::zero_memory(mat, sizeof(Material));
        mat->id = INVALID_ID;
        mat->type = MaterialType::PHONG;
        mat->generation = INVALID_ID;
        CString::copy(SystemConfig::default_name, mat->name, max_material_name_length);

        mat->properties_size = sizeof(MaterialPhongProperties);
        static MaterialPhongProperties default_properties = {};
        default_properties.diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        default_properties.shininess = 8.0f;
        mat->properties = &default_properties;

        Texture* default_textures[3] = { TextureSystem::get_default_diffuse_texture(), TextureSystem::get_default_specular_texture(), TextureSystem::get_default_normal_texture() };
        const uint32 maps_count = 3;
        static TextureMap default_maps[maps_count] = {};
        mat->maps.init(maps_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::MATERIAL_INSTANCE, default_maps);
        for (uint32 i = 0; i < mat->maps.capacity; i++)
        {
            mat->maps[i].filter_minify = mat->maps[i].filter_magnify = TextureFilter::LINEAR;
            mat->maps[i].repeat_u = mat->maps[i].repeat_v = mat->maps[i].repeat_w = TextureRepeat::REPEAT;
            mat->maps[i].texture = default_textures[i % 3];
        }

        Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_material_phong);
        TextureMap* maps[maps_count] = {};
        for (uint32 i = 0; i < maps_count; i++)
            maps[i] = &mat->maps[i];

        if (!Renderer::shader_acquire_instance_resources(s, maps_count, maps, &mat->shader_instance_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        mat->shader_id = s->id;

        return true;
    }

    static bool32 create_default_ui_material() {

        Material* mat = &system_state->default_ui_material;

        Memory::zero_memory(mat, sizeof(Material));
        mat->id = INVALID_ID;
        mat->type = MaterialType::UI;
        mat->generation = INVALID_ID;
        CString::copy(SystemConfig::default_ui_name, mat->name, max_material_name_length);

        mat->properties_size = sizeof(MaterialUIProperties);
        static MaterialUIProperties default_properties = {};
        default_properties.diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        mat->properties = &default_properties;

        const uint32 maps_count = 1;
        static TextureMap default_maps[maps_count] = {};
        mat->maps.init(maps_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::MATERIAL_INSTANCE, default_maps);
        for (uint32 i = 0; i < mat->maps.capacity; i++)
        {
            mat->maps[i].filter_minify = mat->maps[i].filter_magnify = TextureFilter::LINEAR;
            mat->maps[i].repeat_u = mat->maps[i].repeat_v = mat->maps[i].repeat_w = TextureRepeat::REPEAT;
            mat->maps[i].texture = TextureSystem::get_default_texture();
        }

        Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui);
        TextureMap* maps[maps_count] = {};
        for (uint32 i = 0; i < maps_count; i++)
            maps[i] = &mat->maps[i];

        if (!Renderer::shader_acquire_instance_resources(s, maps_count, maps, &mat->shader_instance_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        mat->shader_id = s->id;

        return true;
    }

    bool32 material_get_instance_render_data(void* in_material, Renderer::InstanceRenderData* out_data)
    {
        Material* mat = (Material*)in_material;

        out_data->instance_properties = mat->properties;
        out_data->texture_maps_count = mat->maps.capacity;
        for (uint32 i = 0; i < mat->maps.capacity; i++)
            out_data->texture_maps[i] = &mat->maps[i];

        out_data->shader_instance_id = mat->shader_instance_id;
        out_data->render_frame_number = &mat->render_frame_number;

        return true;
    }

}