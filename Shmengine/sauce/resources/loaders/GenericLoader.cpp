#include "GenericLoader.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "containers/Sarray.hpp"

namespace ResourceSystem
{

    static bool32 generic_loader_load(ResourceLoader* loader, const char* name, Resource* out_resource)
    {

        const char* format = "%s%s%s%s";
        char full_filepath[MAX_FILEPATH_LENGTH];

        String::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader->type_path, name, "");

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("binary_loader_load - Failed to open file for loading material '%s'", full_filepath);
            return false;
        }

        uint32 file_size = FileSystem::get_file_size32(&f);
        out_resource->data_size = file_size;
        out_resource->data = Memory::allocate(out_resource->data_size, true, AllocationTag::MAIN);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, out_resource->data, out_resource->data_size, &bytes_read))
        {
            Memory::free_memory(out_resource->data, true, AllocationTag::MAIN);
            out_resource->data = 0;
            out_resource->data_size = 0;
            SHMERRORV("binary_loader_load - failed to read from file: '%s'.", full_filepath);
            return false;
        }

        FileSystem::file_close(&f);

        uint32 full_path_size = String::length(full_filepath) + 1;
        out_resource->full_path = (char*)Memory::allocate(full_path_size, true, AllocationTag::MAIN);
        String::copy(full_path_size, out_resource->full_path, full_filepath);

        out_resource->name = name;

        return true;

    }

    static void generic_loader_unload(ResourceLoader* loader, Resource* resource)
    {
        if (resource->data)
            Memory::free_memory(resource->data, true, AllocationTag::MAIN);
        if (resource->full_path)
            Memory::free_memory(resource->full_path, true, AllocationTag::MAIN);

        resource->data = 0;
        resource->data_size = 0;
        resource->full_path = 0;
        resource->loader_id = INVALID_OBJECT_ID;
    }

    ResourceLoader generic_resource_loader_create()
    {
        ResourceLoader loader;
        loader.type = ResourceType::GENERIC;
        loader.custom_type = 0;
        loader.load = generic_loader_load;
        loader.unload = generic_loader_unload;
        loader.type_path = "";

        return loader;
    }

}