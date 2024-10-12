#include "ResourceSystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"

#include "resources/loaders/GenericLoader.hpp"
#include "resources/loaders/ImageLoader.hpp"
#include "resources/loaders/MaterialLoader.hpp"
#include "resources/loaders/ShaderLoader.hpp"
#include "resources/loaders/MeshLoader.hpp"
#include "resources/loaders/BitmapFontLoader.hpp"


namespace ResourceSystem
{

	struct SystemState
	{
		Config config;
		ResourceLoader* registered_loaders;
	};

	static SystemState* system_state = 0;

	static bool32 load(const char* name, ResourceLoader* loader, void* params, Resource* out_resource);

    bool32 system_init(FP_allocator_allocate_callback allocator_callback, void*& out_state, Config config)
    {
        out_state = allocator_callback(sizeof(SystemState));
        system_state = (SystemState*)out_state;

        system_state->config = config;

        uint64 loader_array_size = sizeof(ResourceLoader) * config.max_loader_count;
        system_state->registered_loaders = (ResourceLoader*)allocator_callback(loader_array_size);

        // Invalidate all geometries in the array.
        for (uint32 i = 0; i < system_state->config.max_loader_count; ++i) {
            system_state->registered_loaders[i].id = INVALID_ID;
        }

        register_loader(generic_resource_loader_create());
        register_loader(image_resource_loader_create());
        register_loader(material_resource_loader_create());     
        register_loader(shader_resource_loader_create());     
        register_loader(mesh_resource_loader_create());     
        register_loader(bitmap_font_resource_loader_create());     

        SHMINFOV("Resource system initialized with base path: %s", config.asset_base_path);

        return true;

    }

    void system_shutdown()
    {
        system_state = 0;
    }

    bool32 register_loader(ResourceLoader loader)
    {

        for (uint32 i = 0; i < system_state->config.max_loader_count; i++)
        {
            ResourceLoader& reg_loader = system_state->registered_loaders[i];
            if (reg_loader.id != INVALID_ID)
            {
                if (reg_loader.type == loader.type)
                {
                    SHMERRORV("register_loader - Loader of type %u already exists!", loader.type);
                    return false;
                }
                else if (loader.custom_type && CString::equal_i(loader.custom_type, reg_loader.custom_type))
                {
                    SHMERRORV("register_loader - Loader of custom type %s already exists!", loader.custom_type);
                    return false;
                }
            }
        }

        for (uint32 i = 0; i < system_state->config.max_loader_count; i++)
        {
            ResourceLoader& reg_loader = system_state->registered_loaders[i];
            if (reg_loader.id == INVALID_ID)
            {
                reg_loader = loader;
                reg_loader.id = i;
                SHMTRACE("Loader registered.");
                return true;
            }
        }

        return false;

    }

    bool32 load(const char* name, ResourceType type, void* params, Resource* out_resource)
    {
        if (type == ResourceType::CUSTOM)
        {
            SHMERROR("load - Could not load custom type, use dedicated function 'load_custom()' instead!");
            return false;
        }

        for (uint32 i = 0; i < system_state->config.max_loader_count; i++)
        {
            ResourceLoader& reg_loader = system_state->registered_loaders[i];
            if (reg_loader.id != INVALID_ID && reg_loader.type == type)
            {
                return load(name, &reg_loader, params, out_resource);
            }
        }

        SHMERRORV("load - Could not find any registered resource loader for type %u!", type);
        return false;
    }

    bool32 load_custom(const char* name, const char* custom_type, void* params, Resource* out_resource)
    {
        for (uint32 i = 0; i < system_state->config.max_loader_count; i++)
        {
            ResourceLoader& reg_loader = system_state->registered_loaders[i];
            if (reg_loader.id != INVALID_ID && reg_loader.type == ResourceType::CUSTOM && CString::equal_i(custom_type, reg_loader.custom_type))
            {
                return load(name, &reg_loader, params, out_resource);
            }
        }

        SHMERRORV("load - Could not find any registered resource loader for type %s!", custom_type);
        return false;
    }

    void unload(Resource* resource)
    {
        if (resource->loader_id != INVALID_ID)
        {
            ResourceLoader& reg_loader = system_state->registered_loaders[resource->loader_id];
            if (reg_loader.id != INVALID_ID)
                reg_loader.unload(&reg_loader, resource);
        }
    }

    const char* get_base_path()
    {
        return system_state->config.asset_base_path;
    }

    static bool32 load(const char* name, ResourceLoader* loader, void* params, Resource* out_resource)
    {
        out_resource->loader_id = loader->id;
        return loader->load(loader, name, params, out_resource);
    }

}