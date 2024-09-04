#include "ShaderLoader.hpp"

#include "resources/ResourceTypes.hpp"
#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "utility/Sort.hpp"

enum class TruetypeFontFileType {
    NOT_FOUND,
    SHMTTF,
    TTF
};

struct SupportedTruetypeFontFileType {
    const char* extension;
    TruetypeFontFileType type;
};

struct ShmttfFileHeader
{
    uint16 version;  
    uint64 binary_size;
    char face[256];
};

namespace ResourceSystem
{

    static const char* loader_type_path = "fonts/";
   
    static bool32 import_ttf_file(FileSystem::FileHandle* ttf_file, const char* resource_name, const char* shmttf_filepath, TruetypeFontResourceData* out_data);
    static bool32 write_shmttf_file(const char* shmttf_filepath, const char* name, const TruetypeFontResourceData* out_data);
    static bool32 load_shmttf_file(FileSystem::FileHandle* shmttf_file, const char* shmttf_filepath, TruetypeFontResourceData* out_data);

    bool32 truetype_font_loader_load(const char* name, TruetypeFontResourceData* out_data)
    {

        const char* format = "%s%s%s";
        String full_filepath_wo_extension(MAX_FILEPATH_LENGTH);

        safe_print_s<const char*, const char*, const char*>
            (full_filepath_wo_extension, format, get_base_path(), loader_type_path, name);

        const uint32 supported_file_type_count = 2;
        SupportedTruetypeFontFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".shmttf", TruetypeFontFileType::SHMTTF };
        supported_file_types[1] = { ".ttf", TruetypeFontFileType::TTF };

        String full_filepath(MAX_FILEPATH_LENGTH);
        TruetypeFontFileType file_type = TruetypeFontFileType::NOT_FOUND;
        for (uint32 i = 0; i < supported_file_type_count; i++)
        {
            full_filepath = full_filepath_wo_extension;
            full_filepath.append(supported_file_types[i].extension);
            if (FileSystem::file_exists(full_filepath.c_str()))
            {
                file_type = supported_file_types[i].type;
                break;
            }
        }

        if (file_type == TruetypeFontFileType::NOT_FOUND) {
            SHMERRORV("truetype_font_loader_load - Truetype font resource loader failed to find file '%s' with any valid extensions.", full_filepath_wo_extension.c_str());
            return false;
        }

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath.c_str(), FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("truetype_font_loader_load - Failed to open file for loading truetype font '%s'", full_filepath.c_str());
            return false;
        }

        bool32 res = false;
        switch (file_type)
        {
        case TruetypeFontFileType::TTF:
        {
            String shmttf_filepath(MAX_FILEPATH_LENGTH);
            shmttf_filepath = full_filepath_wo_extension;
            shmttf_filepath.append(".shmttf");
            res = import_ttf_file(&f, name, shmttf_filepath.c_str(), out_data);
            break;
        }
        case TruetypeFontFileType::SHMTTF:
        {
            res = load_shmttf_file(&f, full_filepath.c_str(), out_data);
            break;
        }
        }

        FileSystem::file_close(&f);

        if (!res)
        {
            SHMERRORV("Failed to process ttf file '%s'!", full_filepath.c_str());
            //truetype_font_loader_unload(out_data);
        }

        return res;

    }

   void truetype_font_loader_unload(TruetypeFontResourceData* data)
    {
        if (data)
            data->~TruetypeFontResourceData();
    }

    /*static bool32 truetype_font_loader_load(ResourceLoader* loader, const char* name, void* params, Resource* out_resource)
    {

        const char* format = "%s%s%s";
        String full_filepath_wo_extension(MAX_FILEPATH_LENGTH);

        safe_print_s<const char*, const char*, const char*>
            (full_filepath_wo_extension, format, get_base_path(), loader->type_path, name);

        const uint32 supported_file_type_count = 2;
        SupportedTruetypeFontFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".shmttf", TruetypeFontFileType::SHMTTF };
        supported_file_types[1] = { ".ttf", TruetypeFontFileType::TTF };

        String full_filepath(MAX_FILEPATH_LENGTH);
        TruetypeFontFileType file_type = TruetypeFontFileType::NOT_FOUND;
        for (uint32 i = 0; i < supported_file_type_count; i++)
        {
            full_filepath = full_filepath_wo_extension;
            full_filepath.append(supported_file_types[i].extension);
            if (FileSystem::file_exists(full_filepath.c_str()))
            {
                file_type = supported_file_types[i].type;
                break;
            }
        }

        if (file_type == TruetypeFontFileType::NOT_FOUND) {
            SHMERRORV("truetype_font_loader_load - Truetype font resource loader failed to find file '%s' with any valid extensions.", full_filepath_wo_extension.c_str());
            return false;
        }

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath.c_str(), FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("truetype_font_loader_load - Failed to open file for loading truetype font '%s'", full_filepath.c_str());
            return false;
        }

        out_resource->data_size = sizeof(TruetypeFontResourceData);
        out_resource->data = Memory::allocate(out_resource->data_size, AllocationTag::RESOURCE);
        TruetypeFontResourceData* resource_data = (TruetypeFontResourceData*)out_resource->data;
        out_resource->name = name;

        bool32 res = false;
        switch (file_type)
        {
        case TruetypeFontFileType::TTF:
        {
            String shmttf_filepath(MAX_FILEPATH_LENGTH);
            shmttf_filepath = full_filepath_wo_extension;
            shmttf_filepath.append(".shmttf");
            res = import_ttf_file(&f, name, shmttf_filepath.c_str(), resource_data);
            break;
        }
        case TruetypeFontFileType::SHMTTF:
        {
            res = load_shmttf_file(&f, full_filepath.c_str(), resource_data);
            break;
        }
        }

        FileSystem::file_close(&f);
        
        if (!res)
        {
            SHMERRORV("Failed to process ttf file '%s'!", full_filepath.c_str());
            truetype_font_loader_unload(loader, out_resource);
        }

        return res;

    }

    static void truetype_font_loader_unload(ResourceLoader* loader, Resource* resource)
    {
        TruetypeFontResourceData* data = (TruetypeFontResourceData*)resource->data;
        if (data)
            data->~TruetypeFontResourceData();

        resource_unload(loader, resource);
    }*/

    static bool32 import_ttf_file(FileSystem::FileHandle* ttf_file, const char* resource_name, const char* shmttf_filepath, TruetypeFontResourceData* out_data)
    {

        uint32 file_size = FileSystem::get_file_size32(ttf_file);
        out_data->binary_data.init(file_size, 0, AllocationTag::TRUETYPE_FONT);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(ttf_file, out_data->binary_data.data, (uint32)out_data->binary_data.size, &bytes_read))
        {
            SHMERROR("import_ttf_file - failed to read from file.");
            return false;
        }

        CString::copy(256, out_data->face, resource_name);

        return write_shmttf_file(shmttf_filepath, resource_name, out_data);

    }

    static bool32 write_shmttf_file(const char* shmttf_filepath, const char* resource_name, const TruetypeFontResourceData* out_data)
    {

        FileSystem::FileHandle f;

        if (!FileSystem::file_open(shmttf_filepath, FILE_MODE_WRITE, &f)) {
            SHMERRORV("Error opening ttf file for writing: '%s'", shmttf_filepath);
            return false;
        }
        SHMDEBUGV("Writing .shmttf file '%s'...", shmttf_filepath);

        uint64 written_total = 0;
        uint32 written = 0;

        ShmttfFileHeader file_header = {};
        file_header.version = 1;
        CString::copy(256, file_header.face, out_data->face);
        file_header.binary_size = out_data->binary_data.size;

        FileSystem::write(&f, sizeof(file_header), &file_header, &written);
        written_total += written;

        FileSystem::write(&f, (uint32)out_data->binary_data.size, out_data->binary_data.data, &written);
        written_total += written;

        FileSystem::file_close(&f);

        return true;

    }

    static bool32 load_shmttf_file(FileSystem::FileHandle* shmttf_file, const char* shmttf_filepath, TruetypeFontResourceData* out_data)
    {

        uint32 file_size = FileSystem::get_file_size32(shmttf_file);
        Buffer file_content(file_size, 0);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(shmttf_file, file_content.data, (uint32)file_content.size, &bytes_read))
        {
            SHMERRORV("load_shmttf_file - failed to read from file: '%s'.", shmttf_filepath);
            return false;
        }

        SHMTRACEV("Importing shmttf file: '%s'.", shmttf_filepath);

        uint8* read_ptr = (uint8*)file_content.data;
        uint64 read_bytes = 0;

        auto check_buffer_size = [&read_bytes, &file_content](uint64 requested_size)
            {
                SHMASSERT_MSG(read_bytes + requested_size <= file_content.size, "Tried to read outside of buffers memory! File formatting might be corrupted.")
            };

        check_buffer_size(sizeof(ShmttfFileHeader));
        ShmttfFileHeader* file_header = (ShmttfFileHeader*)&read_ptr[read_bytes];
        read_bytes += sizeof(ShmttfFileHeader);

        CString::copy(sizeof(out_data->face), out_data->face, file_header->face);

        check_buffer_size(file_header->binary_size);
        out_data->binary_data.init(file_header->binary_size, 0, AllocationTag::TRUETYPE_FONT);
        out_data->binary_data.copy_memory(&read_ptr[read_bytes], file_header->binary_size, 0);
        read_bytes += file_header->binary_size;

        return true;

    }


    /*ResourceLoader truetype_font_resource_loader_create()
    {
        ResourceLoader loader;
        loader.type = ResourceType::TRUETYPE_FONT;
        loader.custom_type = 0;
        loader.load = truetype_font_loader_load;
        loader.unload = truetype_font_loader_unload;
        loader.type_path = "fonts/";

        return loader;
    }*/
}
