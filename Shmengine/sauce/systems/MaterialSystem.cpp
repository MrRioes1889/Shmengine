#include "MaterialSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "utility/Math.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "containers/Sarray.hpp"


namespace MaterialSystem
{

    struct MaterialShaderUniformLocations
    {
        uint16 projection;
        uint16 view;
        uint16 ambient_color;
        uint16 camera_position;
        uint16 shininess;
        uint16 diffuse_color;
        uint16 diffuse_texture;
        uint16 specular_texture;
        uint16 normal_texture;
        uint16 model;
        uint16 render_mode;
    };

    struct UIShaderUniformLocations
    {
        uint16 projection;
        uint16 view;
        uint16 diffuse_color;
        uint16 diffuse_texture;
        uint16 model;
    };

	struct MaterialReference
	{
		uint32 reference_count;
		uint32 handle;
		bool32 auto_release;
	};

	struct SystemState
	{
		Config config;

		Material default_material;

		Material* registered_materials;
		Hashtable<MaterialReference> registered_material_table;

        MaterialShaderUniformLocations material_locations;
        uint32 material_shader_id;

        UIShaderUniformLocations ui_locations;
        uint32 ui_shader_id;
	};

	static SystemState* system_state = 0;

	static bool32 load_material(MaterialConfig config, Material* m);
	static void destroy_material(Material* m);

	static bool32 create_default_material();

    bool32 system_init(FP_allocator_allocate_callback allocator_callback, void*& out_state, Config config) {

        out_state = allocator_callback(sizeof(SystemState));
        system_state = (SystemState*)out_state;

        system_state->config = config;

        system_state->material_shader_id = INVALID_ID;
        system_state->material_locations.view = INVALID_ID16;
        system_state->material_locations.projection = INVALID_ID16;
        system_state->material_locations.diffuse_color = INVALID_ID16;
        system_state->material_locations.diffuse_texture = INVALID_ID16;
        system_state->material_locations.specular_texture = INVALID_ID16;
        system_state->material_locations.normal_texture = INVALID_ID16;
        system_state->material_locations.camera_position = INVALID_ID16;
        system_state->material_locations.ambient_color = INVALID_ID16;
        system_state->material_locations.shininess = INVALID_ID16;
        system_state->material_locations.model = INVALID_ID16;
        system_state->material_locations.render_mode = INVALID_ID16;

        system_state->ui_shader_id = INVALID_ID;
        system_state->ui_locations.diffuse_color = INVALID_ID16;
        system_state->ui_locations.diffuse_texture = INVALID_ID16;
        system_state->ui_locations.view = INVALID_ID16;
        system_state->ui_locations.projection = INVALID_ID16;
        system_state->ui_locations.model = INVALID_ID16;

        uint64 texture_array_size = sizeof(Material) * config.max_material_count;
        system_state->registered_materials = (Material*)allocator_callback(texture_array_size);

        uint64 hashtable_data_size = sizeof(MaterialReference) * config.max_material_count;
        void* hashtable_data = allocator_callback(hashtable_data_size);
        system_state->registered_material_table.init(config.max_material_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

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

        return true;
    }

    void system_shutdown() {

        if (system_state) {
            // Invalidate all materials in the array.
            uint32 count = system_state->config.max_material_count;
            for (uint32 i = 0; i < count; ++i) {
                if (system_state->registered_materials[i].id != INVALID_ID) {
                    destroy_material(&system_state->registered_materials[i]);
                }
            }

            // Destroy the default material.
            destroy_material(&system_state->default_material);
        }

        system_state = 0;

    }

    Material* acquire(const char* name) {
        // Load the given material configuration from disk.
        MaterialConfig config;

        Resource mt_resource;
        if (!ResourceSystem::load(name, ResourceType::MATERIAL, 0, &mt_resource))
        {
            SHMERRORV("load_mt_file - Failed to load material resources for material '%s'", name);
            return 0;
        }

        config = *((MaterialConfig*)mt_resource.data);
        ResourceSystem::unload(&mt_resource);

        // Now acquire from loaded config.
        return acquire_from_config(config);
    }

    Material* acquire_from_config(const MaterialConfig& config) {
        // Return default material.
        if (CString::equal_i(config.name, Config::default_name)) {
            return &system_state->default_material;
        }

        MaterialReference& ref = system_state->registered_material_table.get_ref(config.name);

        // This can only be changed the first time a material is loaded.
        if (ref.reference_count == 0) {
            ref.auto_release = config.auto_release;
        }

        ref.reference_count++;
        if (ref.handle == INVALID_ID) {
            // This means no material exists here. Find a free index first.
            uint32 count = system_state->config.max_material_count;
            Material* m = 0;
            for (uint32 i = 0; i < count; ++i) {
                if (system_state->registered_materials[i].id == INVALID_ID) {
                    // A free slot has been found. Use its index as the handle.
                    ref.handle = i;
                    m = &system_state->registered_materials[i];
                    break;
                }
            }

            // Make sure an empty slot was actually found.
            if (!m || ref.handle == INVALID_ID) {
                SHMFATAL("material_system_acquire - Material system cannot hold anymore materials. Adjust configuration to allow more.");
                return 0;
            }

            // Create new material.
            if (!load_material(config, m)) {
                SHMERRORV("Failed to load material '%s'.", config.name);
                return 0;
            }

            Renderer::Shader* s = ShaderSystem::get_shader(m->shader_id);
            if (system_state->material_shader_id == INVALID_ID && CString::equal(config.shader_name.c_str(), Renderer::RendererConfig::builtin_shader_name_material)) {
                system_state->material_shader_id = s->id;
                system_state->material_locations.projection = ShaderSystem::get_uniform_index(s, "projection");
                system_state->material_locations.view = ShaderSystem::get_uniform_index(s, "view");
                system_state->material_locations.ambient_color = ShaderSystem::get_uniform_index(s, "ambient_color");
                system_state->material_locations.camera_position = ShaderSystem::get_uniform_index(s, "camera_position");
                system_state->material_locations.diffuse_color = ShaderSystem::get_uniform_index(s, "diffuse_color");
                system_state->material_locations.diffuse_texture = ShaderSystem::get_uniform_index(s, "diffuse_texture");
                system_state->material_locations.specular_texture = ShaderSystem::get_uniform_index(s, "specular_texture");
                system_state->material_locations.normal_texture = ShaderSystem::get_uniform_index(s, "normal_texture");
                system_state->material_locations.shininess = ShaderSystem::get_uniform_index(s, "shininess");
                system_state->material_locations.model = ShaderSystem::get_uniform_index(s, "model");
                system_state->material_locations.render_mode = ShaderSystem::get_uniform_index(s, "mode");
            }
            else if (system_state->ui_shader_id == INVALID_ID && CString::equal(config.shader_name.c_str(), Renderer::RendererConfig::builtin_shader_name_ui)) {
                system_state->ui_shader_id = s->id;
                system_state->ui_locations.projection = ShaderSystem::get_uniform_index(s, "projection");
                system_state->ui_locations.view = ShaderSystem::get_uniform_index(s, "view");
                system_state->ui_locations.diffuse_color = ShaderSystem::get_uniform_index(s, "diffuse_color");
                system_state->ui_locations.diffuse_texture = ShaderSystem::get_uniform_index(s, "diffuse_texture");
                system_state->ui_locations.model = ShaderSystem::get_uniform_index(s, "model");
            }

            if (m->generation == INVALID_ID) {
                m->generation = 0;
            }
            else {
                m->generation++;
            }

            // Also use the handle as the material id.
            m->id = ref.handle;
            SHMTRACEV("Material '%s' does not yet exist. Created, and ref_count is now %i.", config.name, ref.reference_count);
        }
        else {
            SHMTRACEV("Material '%s' already exists, ref_count increased to %i.", config.name, ref.reference_count);
        }

       return &system_state->registered_materials[ref.handle];

    }

    void release(const char* name) {
        // Ignore release requests for the default material.
        if (CString::equal_i(name, Config::default_name)) {
            return;
        }
        MaterialReference& ref = system_state->registered_material_table.get_ref(name);

        if (ref.reference_count == 0) {
            SHMWARNV("Tried to release non-existent material: '%s'", name);
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
            SHMTRACEV("Released material '%s'., Material unloaded because reference count=0 and auto_release=true.", name);
        }
        else {
            SHMTRACEV("Released material '%s', now has a reference count of '%i' (auto_release=%s).", name, ref.reference_count, ref.auto_release ? "true" : "false");
        }

    }

#define MATERIAL_APPLY_OR_FAIL(expr)                  \
    if ((!expr)) {                                      \
        SHMERRORV("Failed to apply material: %s", expr); \
        return false;                                 \
    }

    bool32 apply_globals(uint32 shader_id, uint64 renderer_frame_number, const Math::Mat4* projection, const Math::Mat4* view, const Math::Vec4f* ambient_color, const Math::Vec3f* camera_position, uint32 render_mode)
    {
        Renderer::Shader* s = ShaderSystem::get_shader(shader_id);
        if (!s)
            return false;

        if (s->renderer_frame_number == renderer_frame_number)
            return true;

        if (shader_id == system_state->material_shader_id) {
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.projection, projection));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.view, view));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.ambient_color, ambient_color));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.camera_position, camera_position));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.render_mode, &render_mode));
        }
        else if (shader_id == system_state->ui_shader_id) {
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->ui_locations.projection, projection));
            MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->ui_locations.view, view));
        }
        else {
            SHMERRORV("material_system_apply_global - Unrecognized shader id '%i' ", shader_id);
            return false;
        }
        MATERIAL_APPLY_OR_FAIL(ShaderSystem::apply_global());

        s->renderer_frame_number = renderer_frame_number;
        return true;
    }

    bool32 apply_instance(Material* m, bool32 needs_update)
    {
        MATERIAL_APPLY_OR_FAIL(ShaderSystem::bind_instance(m->internal_id));
        if (needs_update)
        {
            if (m->shader_id == system_state->material_shader_id) {
                // Material shader
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.diffuse_color, &m->diffuse_color));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.diffuse_texture, &m->diffuse_map));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.specular_texture, &m->specular_map));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.normal_texture, &m->normal_map));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->material_locations.shininess, &m->shininess));
            }
            else if (m->shader_id == system_state->ui_shader_id) {
                // UI shader
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->ui_locations.diffuse_color, &m->diffuse_color));
                MATERIAL_APPLY_OR_FAIL(ShaderSystem::set_uniform(system_state->ui_locations.diffuse_texture, &m->diffuse_map));
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
        if (m->shader_id == system_state->material_shader_id) {
            return ShaderSystem::set_uniform(system_state->material_locations.model, &model);
        }
        else if (m->shader_id == system_state->ui_shader_id) {
            return ShaderSystem::set_uniform(system_state->ui_locations.model, &model);
        }

        SHMERRORV("Unrecognized shader id '%i'", m->shader_id);
        return false;
    }

    static bool32 load_material(MaterialConfig config, Material* m)
    {
        Memory::zero_memory(m, sizeof(Material));

        // name
        CString::copy(Material::max_name_length ,m->name, config.name);

        m->shader_id = ShaderSystem::get_id(config.shader_name.c_str());
        m->diffuse_color = config.diffuse_color;
        m->shininess = config.shininess;

        auto init_texture_map = [](const char* map_name, const char* material_name, TextureUse use, TextureMap& out_map, Texture* default_texture = 0) -> bool32
            {
                out_map.use = use;
                out_map.filter_minify = TextureFilter::LINEAR;
                out_map.filter_magnify = TextureFilter::LINEAR;
                out_map.repeat_u = TextureRepeat::REPEAT;
                out_map.repeat_v = TextureRepeat::REPEAT;
                out_map.repeat_w = TextureRepeat::REPEAT;
                out_map.repeat_u = TextureRepeat::REPEAT;
                if (!Renderer::texture_map_acquire_resources(&out_map))
                {
                    SHMERRORV("load_material - Failed to acquire resources for texture map: '%s'.", map_name);
                    return false;
                }

                if (map_name[0]) {
                    out_map.texture = TextureSystem::acquire(map_name, true);
                    if (!out_map.texture) {
                        SHMWARNV("load_material - Unable to load texture '%s' for material '%s', using default.", material_name);
                        out_map.texture = TextureSystem::get_default_texture();
                    }           
                }
                else {
                    out_map.texture = default_texture;
                }
                return true;
            };

        if (!init_texture_map(config.diffuse_map_name, m->name, TextureUse::MAP_DIFFUSE, m->diffuse_map, TextureSystem::get_default_diffuse_texture()))
            return false;
        if (!init_texture_map(config.specular_map_name, m->name, TextureUse::MAP_SPECULAR, m->specular_map, TextureSystem::get_default_specular_texture()))
            return false;
        if (!init_texture_map(config.normal_map_name, m->name, TextureUse::MAP_NORMAL, m->normal_map, TextureSystem::get_default_normal_texture()))
            return false;

        // Send it off to the renderer to acquire resources.
        Renderer::Shader* s = ShaderSystem::get_shader(config.shader_name.c_str());
        if (!s) {
            SHMERRORV("Unable to load material because its shader was not found: '%s'. This is likely a problem with the material asset.", config.shader_name.c_str());
            return false;
        }

        TextureMap* maps[3] = { &m->diffuse_map, &m->specular_map, &m->normal_map };
        if (!Renderer::shader_acquire_instance_resources(s, maps, &m->internal_id)) {
            SHMERRORV("Failed to acquire resources for material '%s'.", m->name);
            return false;
        }

        return true;
    }

    static void destroy_material(Material* m)
    {
        SHMTRACEV("Destroying material '%s'...", m->name);

        // Release texture references.
        if (m->diffuse_map.texture) {
            TextureSystem::release(m->diffuse_map.texture->name);
        }

        if (m->specular_map.texture) {
            TextureSystem::release(m->specular_map.texture->name);
        }

        if (m->normal_map.texture) {
            TextureSystem::release(m->normal_map.texture->name);
        }

        Renderer::texture_map_release_resources(&m->diffuse_map);
        Renderer::texture_map_release_resources(&m->specular_map);
        Renderer::texture_map_release_resources(&m->normal_map);

        // Release renderer resources.
        if (m->shader_id != INVALID_ID && m->internal_id != INVALID_ID)
        {
            Renderer::shader_release_instance_resources(ShaderSystem::get_shader(m->shader_id), m->internal_id);
            m->shader_id = INVALID_ID;
        }

        // Zero it out, invalidate IDs.
        Memory::zero_memory(m, sizeof(Material));
        m->id = INVALID_ID;
        m->generation = INVALID_ID;
        m->internal_id = INVALID_ID;
        m->render_frame_number = INVALID_ID;
    }

    static bool32 create_default_material() {
        Memory::zero_memory(&system_state->default_material, sizeof(Material));
        system_state->default_material.id = INVALID_ID;
        system_state->default_material.generation = INVALID_ID;
        CString::copy(Material::max_name_length, system_state->default_material.name, Config::default_name);
        system_state->default_material.diffuse_color = VEC4F_ONE;  // white

        system_state->default_material.diffuse_map.use = TextureUse::MAP_DIFFUSE;
        system_state->default_material.diffuse_map.texture = TextureSystem::get_default_texture();

        system_state->default_material.specular_map.use = TextureUse::MAP_SPECULAR;
        system_state->default_material.specular_map.texture = TextureSystem::get_default_specular_texture();

        system_state->default_material.normal_map.use = TextureUse::MAP_NORMAL;
        system_state->default_material.normal_map.texture = TextureSystem::get_default_normal_texture();

        Renderer::Shader* s = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_material);
        TextureMap* maps[3] = { &system_state->default_material.diffuse_map, &system_state->default_material.specular_map, &system_state->default_material.normal_map };
        if (!Renderer::shader_acquire_instance_resources(s, maps, &system_state->default_material.internal_id))
        {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        system_state->default_material.shader_id = s->id;

        return true;
    }

    Material* get_default_material()
    {
        return &system_state->default_material;
    }

    


}