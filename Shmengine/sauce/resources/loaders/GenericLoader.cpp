#include "GenericLoader.hpp"

#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/CString.hpp"
#include "platform/FileSystem.hpp"

namespace ResourceSystem
{

    static bool32 generic_loader_load(ResourceLoader* loader, const char* name, Resource* out_resource)
    {

        const char* format = "%s%s%s%s";
        char full_filepath[MAX_FILEPATH_LENGTH];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, MAX_FILEPATH_LENGTH, format, get_base_path(), loader->type_path, name, "");

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("binary_loader_load - Failed to open file for loading material '%s'", full_filepath);
            return false;
        }

        uint32 file_size = FileSystem::get_file_size32(&f);
        out_resource->data_size = file_size;
        out_resource->allocation_tag = AllocationTag::MAIN;
        out_resource->data = Memory::allocate(out_resource->data_size, true, out_resource->allocation_tag);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, out_resource->data, out_resource->data_size, &bytes_read))
        {
            Memory::free_memory(out_resource->data, true, out_resource->allocation_tag);
            out_resource->data = 0;
            out_resource->data_size = 0;
            SHMERRORV("binary_loader_load - failed to read from file: '%s'.", full_filepath);
            return false;
        }

        FileSystem::file_close(&f);

        out_resource->full_path = full_filepath;

        out_resource->name = name;

        return true;

    }

    static void generic_loader_unload(ResourceLoader* loader, Resource* resource)
    {      
        resource_unload(loader, resource);
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