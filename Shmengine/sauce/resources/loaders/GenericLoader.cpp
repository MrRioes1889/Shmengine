#include "GenericLoader.hpp"

#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "systems/ResourceSystem.hpp"
#include "containers/Buffer.hpp"
#include "utility/CString.hpp"
#include "platform/FileSystem.hpp"

namespace ResourceSystem
{

    static const char* loader_type_path = "";

    bool32 generic_loader_load(const char* name, void* params, Buffer* out_buffer)
    {

        const char* format = "%s%s%s%s";
        char full_filepath[Constants::MAX_FILEPATH_LENGTH];

        CString::safe_print_s<const char*, const char*, const char*, const char*>
            (full_filepath, Constants::MAX_FILEPATH_LENGTH, format, get_base_path(), loader_type_path, name, "");

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath, FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("binary_loader_load - Failed to open file for loading material '%s'", full_filepath);
            return false;
        }

        uint32 file_size = FileSystem::get_file_size32(&f);
        out_buffer->init(file_size, 0, AllocationTag::RESOURCE);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(&f, out_buffer->data, (uint32)out_buffer->size, &bytes_read))
        {
            out_buffer->free_data();
            SHMERRORV("binary_loader_load - failed to read from file: '%s'.", full_filepath);
            return false;
        }

        FileSystem::file_close(&f);

        return true;

    }

    void generic_loader_unload(Buffer* buffer)
    {      
        buffer->free_data();
    }

}