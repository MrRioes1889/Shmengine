#include "MaterialSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
#include "containers/Hashtable.hpp"
#include "utility/Math.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/ResourceSystem.hpp"
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
		Config config;

		Material default_material;

		Material* registered_materials;
		Hashtable<MaterialReference> registered_material_table;
	};

	static SystemState* system_state = 0;

	static bool32 load_material(ResourceDataMaterial config, Material* m);
	static void destroy_material(Material* m);

	static bool32 create_default_material();

    bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config) {

        out_state = allocator_callback(sizeof(SystemState));
        system_state = (SystemState*)out_state;

        system_state->config = config;

        uint64 texture_array_size = sizeof(Material) * config.max_material_count;
        system_state->registered_materials = (Material*)allocator_callback(texture_array_size);

        uint64 hashtable_data_size = sizeof(MaterialReference) * config.max_material_count;
        void* hashtable_data = allocator_callback(hashtable_data_size);
        system_state->registered_material_table.init(config.max_material_count, AllocationTag::UNKNOWN, hashtable_data);

        // Fill the hashtable with invalid references to use as a default.
        MaterialReference invalid_ref;
        invalid_ref.auto_release = false;
        invalid_ref.handle = INVALID_OBJECT_ID;  // Primary reason for needing default values.
        invalid_ref.reference_count = 0;
        system_state->registered_material_table.floodfill(invalid_ref);

        // Invalidate all materials in the array.
        uint32 count = system_state->config.max_material_count;
        for (uint32 i = 0; i < count; ++i) {
            system_state->registered_materials[i].id = INVALID_OBJECT_ID;
            system_state->registered_materials[i].generation = INVALID_OBJECT_ID;
            system_state->registered_materials[i].internal_id = INVALID_OBJECT_ID;
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
                if (system_state->registered_materials[i].id != INVALID_OBJECT_ID) {
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
        ResourceDataMaterial config;

        Resource mt_resource;
        if (!ResourceSystem::load(name, ResourceType::MATERIAL, &mt_resource))
        {
            SHMERRORV("load_mt_file - Failed to load material resources for material '%s'", name);
            return 0;
        }

        config = *((ResourceDataMaterial*)mt_resource.data);
        ResourceSystem::unload(&mt_resource);

        // Now acquire from loaded config.
        return acquire_from_config(config);
    }

    Material* acquire_from_config(const ResourceDataMaterial& config) {
        // Return default material.
        if (CString::equal_i(config.name, Config::default_name)) {
            return &system_state->default_material;
        }

        MaterialReference ref = system_state->registered_material_table.get_value(config.name);

        // This can only be changed the first time a material is loaded.
        if (ref.reference_count == 0) {
            ref.auto_release = config.auto_release;
        }

        ref.reference_count++;
        if (ref.handle == INVALID_OBJECT_ID) {
            // This means no material exists here. Find a free index first.
            uint32 count = system_state->config.max_material_count;
            Material* m = 0;
            for (uint32 i = 0; i < count; ++i) {
                if (system_state->registered_materials[i].id == INVALID_OBJECT_ID) {
                    // A free slot has been found. Use its index as the handle.
                    ref.handle = i;
                    m = &system_state->registered_materials[i];
                    break;
                }
            }

            // Make sure an empty slot was actually found.
            if (!m || ref.handle == INVALID_OBJECT_ID) {
                SHMFATAL("material_system_acquire - Material system cannot hold anymore materials. Adjust configuration to allow more.");
                return 0;
            }

            // Create new material.
            if (!load_material(config, m)) {
                SHMERRORV("Failed to load material '%s'.", config.name);
                return 0;
            }

            if (m->generation == INVALID_OBJECT_ID) {
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

        // Update the entry.
       system_state->registered_material_table.set_value(config.name, ref);
       return &system_state->registered_materials[ref.handle];

    }

    void release(const char* name) {
        // Ignore release requests for the default material.
        if (CString::equal_i(name, Config::default_name)) {
            return;
        }
        MaterialReference ref = system_state->registered_material_table.get_value(name);

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
            ref.handle = INVALID_OBJECT_ID;
            ref.auto_release = false;
            SHMTRACEV("Released material '%s'., Material unloaded because reference count=0 and auto_release=true.", name);
        }
        else {
            SHMTRACEV("Released material '%s', now has a reference count of '%i' (auto_release=%s).", name, ref.reference_count, ref.auto_release ? "true" : "false");
        }

        // Update the entry.
        system_state->registered_material_table.set_value(name, ref);

    }

    static bool32 load_material(ResourceDataMaterial config, Material* m)
    {
        Memory::zero_memory(m, sizeof(Material));

        // name
        CString::copy(Material::max_name_length ,m->name, config.name);

        m->type = config.type;
        m->diffuse_color = config.diffuse_color;

        // Diffuse map
        if (CString::length(config.diffuse_map_name) > 0) {
            m->diffuse_map.use = TextureUse::MAP_DIFFUSE;
            m->diffuse_map.texture = TextureSystem::acquire(config.diffuse_map_name, true);
            if (!m->diffuse_map.texture) {
                SHMWARNV("Unable to load texture '%s' for material '%s', using default.", config.diffuse_map_name, m->name);
                m->diffuse_map.texture = TextureSystem::get_default_texture();
            }
        }
        else {
            // NOTE: Only set for clarity, as call to kzero_memory above does this already.
            m->diffuse_map.use = TextureUse::UNKNOWN;
            m->diffuse_map.texture = 0;
        }

        // TODO: other maps

        // Send it off to the renderer to acquire resources.
        if (!Renderer::create_material(m)) {
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

        // Release renderer resources.
        Renderer::destroy_material(m);

        // Zero it out, invalidate IDs.
        Memory::zero_memory(m, sizeof(Material));
        m->id = INVALID_OBJECT_ID;
        m->generation = INVALID_OBJECT_ID;
        m->internal_id = INVALID_OBJECT_ID;
    }

    static bool32 create_default_material() {
        Memory::zero_memory(&system_state->default_material, sizeof(Material));
        system_state->default_material.id = INVALID_OBJECT_ID;
        system_state->default_material.generation = INVALID_OBJECT_ID;
        system_state->default_material.type = MaterialType::WORLD;
        CString::copy(Material::max_name_length, system_state->default_material.name, Config::default_name);
        system_state->default_material.diffuse_color = VEC4F_ONE;  // white
        system_state->default_material.diffuse_map.use = TextureUse::MAP_DIFFUSE;
        system_state->default_material.diffuse_map.texture = TextureSystem::get_default_texture();

        if (!Renderer::create_material(&system_state->default_material)) {
            SHMFATAL("Failed to acquire renderer resources for default texture. Application cannot continue.");
            return false;
        }

        return true;
    }

    Material* get_default_material()
    {
        return &system_state->default_material;
    }


}