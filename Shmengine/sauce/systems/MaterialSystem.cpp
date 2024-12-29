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
		Material default_terrain_material;

		Material* registered_materials;
		Hashtable<MaterialReference> registered_material_table;
	};

	static SystemState* system_state = 0;

	static bool32 load_material(const MaterialConfig* config, Material* m);
	static void destroy_material(Material* m);

	static bool32 create_default_material();
	static bool32 create_default_ui_material();
	static bool32 create_default_terrain_material();

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
            system_state->registered_materials[i].internal_id = INVALID_ID;
            system_state->registered_materials[i].render_frame_number = INVALID_ID;
        }

        if (!create_default_material()) {
            SHMFATAL("Failed to create default material. Application cannot continue.");
            return false;
        }

        if (!create_default_terrain_material()) {
            SHMFATAL("Failed to create default terrain material. Application cannot continue.");
            return false;
        }

        if (!create_default_ui_material()) {
            SHMFATAL("Failed to create default ui material. Application cannot continue.");
            return false;
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
        destroy_material(&system_state->default_ui_material);
        destroy_material(&system_state->default_terrain_material);
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

    Material* acquire_terrain_material(const char* material_name, uint32 sub_material_count, const char** sub_material_names, bool32 auto_release)
    {

        if (CString::equal_i(material_name, SystemConfig::default_terrain_name))
            return &system_state->default_material;

        bool32 exists = false;
        Material* m = acquire_reference(material_name, auto_release, &exists);

        if (!exists)
        {
            
            Material* sub_materials[max_terrain_materials_count] = {};
            for (uint32 i = 0; i < sub_material_count; i++)
                sub_materials[i] = acquire(sub_material_names[i]);

            TextureSystem::sleep_until_all_textures_loaded();

            *m = {};

            CString::copy(material_name, m->name, max_material_name_length);

            Renderer::Shader* shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_terrain);
            m->shader_id = shader->id;
            m->type = MaterialType::TERRAIN;

            m->properties_size = sizeof(MaterialTerrainProperties);
            m->properties = Memory::allocate(m->properties_size, AllocationTag::MATERIAL_INSTANCE);
            MaterialTerrainProperties* properties = (MaterialTerrainProperties*)m->properties;
            properties->materials_count = sub_material_count;

            const uint32 max_map_count = max_terrain_materials_count * 3;
            m->maps.init(max_map_count, 0);

            const char* map_names[3] = { "diffuse", "specular", "normal" };
            Texture* default_textures[3] = 
            {
                TextureSystem::get_default_diffuse_texture(),
                TextureSystem::get_default_specular_texture(),
                TextureSystem::get_default_normal_texture()
            };

            Material* default_material = get_default_material();

            // Phong properties and maps for each material.
            for (uint32 material_idx = 0; material_idx < max_terrain_materials_count; material_idx++) {
                // Properties.
                MaterialPhongProperties* mat_props = &properties->materials[material_idx];
                // Use default material unless within the material count.
                Material* sub_mat = default_material;
                if (material_idx < sub_material_count)
                    sub_mat = sub_materials[material_idx];

                MaterialPhongProperties* sub_mat_properties = (MaterialPhongProperties*)sub_mat->properties;
                mat_props->diffuse_color = sub_mat_properties->diffuse_color;
                mat_props->shininess = sub_mat_properties->shininess;

                // Maps, 3 for phong. Diffuse, spec, normal.
                for (uint32 map_i = 0; map_i < 3; ++map_i) {
                    TextureMapConfig map_config = { 0 };
                    map_config.name = map_names[map_i];
                    map_config.texture_name = sub_mat->maps[map_i].texture->name;
                    map_config.repeat_u = sub_mat->maps[map_i].repeat_u;
                    map_config.repeat_v = sub_mat->maps[map_i].repeat_v;
                    map_config.repeat_w = sub_mat->maps[map_i].repeat_w;
                    map_config.filter_min = sub_mat->maps[map_i].filter_minify;
                    map_config.filter_mag = sub_mat->maps[map_i].filter_magnify;
                    if (!assign_map(&m->maps[(material_idx * 3) + map_i], &map_config, m->name, default_textures[map_i])) {
                        SHMERRORV("Failed to assign '%s' texture map for terrain material index %u", map_names[map_i], material_idx);
                        return 0;
                    }
                }
            }

            for (uint32 i = 0; i < sub_material_count; i++)
                MaterialSystem::release(sub_material_names[i]);

            TextureMap* maps[max_map_count] = {};
            for (uint32 i = 0; i < max_map_count; i++)
                maps[i] = &m->maps[i];

            if (!Renderer::shader_acquire_instance_resources(shader, max_map_count, maps, &m->internal_id))
                SHMERRORV("Failed to acquire renderer resources for material '%s'.", m->name);

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

#define MATERIAL_APPLY_OR_FAIL(expr)                  \
    if ((!expr)) {                                      \
        SHMERRORV("Failed to apply material: %s", expr); \
        return false;                                 \
    }

    bool32 apply_globals(uint32 shader_id, LightingInfo lighting, uint64 renderer_frame_number, const Math::Mat4* projection, const Math::Mat4* view, const Math::Vec4f* ambient_color, const Math::Vec3f* camera_position, uint32 render_mode)
    {
        Renderer::Shader* s = ShaderSystem::get_shader(shader_id);
        if (!s)
            return false;

        if (s->renderer_frame_number == renderer_frame_number)
            return true;

        if (shader_id == ShaderSystem::get_material_shader_id()) 
        {
            ShaderSystem::MaterialShaderUniformLocations u_locations = ShaderSystem::get_material_shader_uniform_locations();
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, projection));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, view));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.ambient_color, ambient_color));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.camera_position, camera_position));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.render_mode, &render_mode));
        }
        else if (shader_id == ShaderSystem::get_terrain_shader_id())
        {
            ShaderSystem::TerrainShaderUniformLocations u_locations = ShaderSystem::get_terrain_shader_uniform_locations();
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, projection));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, view));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.ambient_color, ambient_color));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.camera_position, camera_position));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.render_mode, &render_mode));

            if (lighting.dir_light)
            {
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.dir_light, lighting.dir_light));
            }
        }
        else if (shader_id == ShaderSystem::get_ui_shader_id()) 
        {
            ShaderSystem::UIShaderUniformLocations u_locations = ShaderSystem::get_ui_shader_uniform_locations();
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, projection));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, view));
        }
        else 
        {
            SHMERRORV("material_system_apply_global - Unrecognized shader id '%i' ", shader_id);
            return false;
        }
        MATERIAL_APPLY_OR_FAIL(ShaderSystem::apply_global());

        s->renderer_frame_number = renderer_frame_number;
        return true;
    }

    bool32 apply_instance(Material* m, LightingInfo lighting, bool32 needs_update)
    {
        MATERIAL_APPLY_OR_FAIL(ShaderSystem::bind_instance(m->internal_id));
        if (needs_update)
        {
            if (m->shader_id == ShaderSystem::get_material_shader_id()) 
            {
                ShaderSystem::MaterialShaderUniformLocations u_locations = ShaderSystem::get_material_shader_uniform_locations();
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.properties, m->properties));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.diffuse_texture, &m->maps[0]));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.specular_texture, &m->maps[1]));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.normal_texture, &m->maps[2]));

                if (lighting.dir_light)
                {
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.dir_light, lighting.dir_light));
                }
                    
                if (lighting.p_lights)
                {
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, &lighting.p_lights_count));
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights, lighting.p_lights));
                }
                else
                {
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, 0));
                }
                
            }
            else if (m->shader_id == ShaderSystem::get_terrain_shader_id())
            {
                ShaderSystem::TerrainShaderUniformLocations u_locations = ShaderSystem::get_terrain_shader_uniform_locations();
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.properties, m->properties));
                for (uint32 i = 0; i < m->maps.capacity; i++)
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.samplers[i], &m->maps[i]));

                if (lighting.p_lights)
                {
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, &lighting.p_lights_count));
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights, lighting.p_lights));
                }
                else
                {
                    MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, 0));
                }

            }
            else if (m->shader_id == ShaderSystem::get_ui_shader_id()) {
                ShaderSystem::UIShaderUniformLocations u_locations = ShaderSystem::get_ui_shader_uniform_locations();
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.properties, m->properties));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.diffuse_texture, &m->maps[0]));
            }
            else {
                SHMERRORV("material_system_apply_instance - Unrecognized shader id '%i' on shader '%s'.", m->shader_id, m->name);
                return false;
            }
        }     
        MATERIAL_APPLY_OR_FAIL(ShaderSystem::apply_instance(needs_update));

        return true;
    }

    bool32 apply_local(Material* m, const Math::Mat4& model)
    {
        if (m->shader_id == ShaderSystem::get_material_shader_id()) 
        {
            return ShaderSystem::set_uniform(ShaderSystem::get_material_shader_uniform_locations().model, &model);
        }
        else if (m->shader_id == ShaderSystem::get_terrain_shader_id())
        {
            return ShaderSystem::set_uniform(ShaderSystem::get_terrain_shader_uniform_locations().model, &model);
        }
        else if (m->shader_id == ShaderSystem::get_ui_shader_id()) 
        {
            return ShaderSystem::set_uniform(ShaderSystem::get_ui_shader_uniform_locations().model, &model);
        }

        SHMERRORV("Unrecognized shader id '%i'", m->shader_id);
        return false;
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

        if (m->type == MaterialType::TERRAIN)
        {
            SHMERROR("Terrain Material type soon to be deprecated and not loadable as single material!");
            return false;
        }

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

        Renderer::Shader* shader = 0;
        if (m->type == MaterialType::PHONG)
        {
            shader = ShaderSystem::get_shader(*config->shader_name ? config->shader_name : Renderer::RendererConfig::builtin_shader_name_material);
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

        bool32 res = Renderer::shader_acquire_instance_resources(shader, maps.capacity, maps.data, &m->internal_id);
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
        if (m->shader_id != INVALID_ID && m->internal_id != INVALID_ID)
        {
            Renderer::shader_release_instance_resources(ShaderSystem::get_shader(m->shader_id), m->internal_id);
            m->shader_id = INVALID_ID;
        }

        if (m->properties)
            Memory::free_memory(m->properties);

        // Zero it out, invalidate IDs.
        Memory::zero_memory(m, sizeof(Material));
        m->id = INVALID_ID;
        m->generation = INVALID_ID;
        m->internal_id = INVALID_ID;
        m->render_frame_number = INVALID_ID;
    }

    Material* get_default_material()
    {
        return &system_state->default_material;
    }

    Material* get_default_terrain_material()
    {
        return &system_state->default_terrain_material;
    }

    Material* get_default_ui_material()
    {
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

        Renderer::Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_material);
        TextureMap* maps[maps_count] = {};
        for (uint32 i = 0; i < maps_count; i++)
            maps[i] = &mat->maps[i];

        if (!Renderer::shader_acquire_instance_resources(s, maps_count, maps, &mat->internal_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        mat->shader_id = s->id;

        return true;
    }

    static bool32 create_default_terrain_material() {

        Material* mat = &system_state->default_terrain_material;

        Memory::zero_memory(mat, sizeof(Material));
        mat->id = INVALID_ID;
        mat->type = MaterialType::TERRAIN;
        mat->generation = INVALID_ID;
        CString::copy(SystemConfig::default_terrain_name, mat->name, max_material_name_length);

        mat->properties_size = sizeof(MaterialTerrainProperties);
        static MaterialTerrainProperties default_properties = {};
        default_properties.materials_count = 1;
        default_properties.materials[0].diffuse_color = { 1.0f, 1.0f, 1.0f, 1.0f };
        default_properties.materials[0].shininess = 8.0f;
        mat->properties = &default_properties;

        Texture* default_textures[3] = { TextureSystem::get_default_diffuse_texture(), TextureSystem::get_default_specular_texture(), TextureSystem::get_default_normal_texture() };
        const uint32 maps_count = max_terrain_materials_count * 3;
        static TextureMap default_maps[maps_count] = {};
        mat->maps.init(maps_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::MATERIAL_INSTANCE, default_maps);
        for (uint32 i = 0; i < mat->maps.capacity; i++)
        {
            mat->maps[i].filter_minify = mat->maps[i].filter_magnify = TextureFilter::LINEAR;
            mat->maps[i].repeat_u = mat->maps[i].repeat_v = mat->maps[i].repeat_w = TextureRepeat::REPEAT;
            mat->maps[i].texture = default_textures[i % 3];
        }

        Renderer::Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_terrain);
        TextureMap* maps[maps_count] = {};
        for (uint32 i = 0; i < maps_count; i++)
            maps[i] = &mat->maps[i];

        if (!Renderer::shader_acquire_instance_resources(s, maps_count, maps, &mat->internal_id))
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

        Renderer::Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui);
        TextureMap* maps[maps_count] = {};
        for (uint32 i = 0; i < maps_count; i++)
            maps[i] = &mat->maps[i];

        if (!Renderer::shader_acquire_instance_resources(s, maps_count, maps, &mat->internal_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        mat->shader_id = s->id;

        return true;
    }

}