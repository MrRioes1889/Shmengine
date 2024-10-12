#include "ShaderLoader.hpp"

#include "resources/ResourceTypes.hpp"
#include "LoaderUtils.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "utility/Sort.hpp"

enum class BitmapFontFileType {
    NOT_FOUND,
    SHMBMF,
    FNT
};

struct SupportedBitmapFontFileType {
    const char* extension;
    BitmapFontFileType type;
};

struct ShmbmfFileHeader
{
    uint16 version;
    uint16 name_length;
    uint32 name_offset;
    uint32 pages_count;
    uint32 pages_offset;
    uint32 glyphs_count;
    uint32 glyphs_offset;
    uint32 kernings_count;
    uint32 kernings_offset;

    char face[256];
    uint32 line_height;
    int32 baseline;
    uint32 atlas_size_x;
    uint32 atlas_size_y;
    uint32 glyph_size;
};

namespace ResourceSystem
{

    static void bitmap_font_loader_unload(ResourceLoader* loader, Resource* resource);
    static bool32 import_fnt_file(FileSystem::FileHandle* fnt_file, const char* resource_name, const char* shmbmf_filepath, BitmapFontResourceData* out_data);
    static bool32 write_shmbmf_file(const char* shmbmf_filepath, const char* name, const BitmapFontResourceData* out_data);
    static bool32 load_shmbmf_file(FileSystem::FileHandle* shmbmf_file, const char* shmbmf_filepath, BitmapFontResourceData* out_data);

    static bool32 bitmap_font_loader_load(ResourceLoader* loader, const char* name, void* params, Resource* out_resource)
    {

        const char* format = "%s%s%s";
        String full_filepath_wo_extension(MAX_FILEPATH_LENGTH);

        safe_print_s<const char*, const char*, const char*>
            (full_filepath_wo_extension, format, get_base_path(), loader->type_path, name);

        const uint32 supported_file_type_count = 2;
        SupportedBitmapFontFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".shmbmf", BitmapFontFileType::SHMBMF};
        supported_file_types[1] = { ".fnt", BitmapFontFileType::FNT};

        String full_filepath(MAX_FILEPATH_LENGTH);
        BitmapFontFileType file_type = BitmapFontFileType::NOT_FOUND;
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

        if (file_type == BitmapFontFileType::NOT_FOUND) {
            SHMERRORV("bitmap_font_loader_load - Bitmap font resource loader failed to find file '%s' with any valid extensions.", full_filepath_wo_extension.c_str());
            return false;
        }

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath.c_str(), FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("bitmap_font_loader_load - Failed to open file for loading bitmap font '%s'", full_filepath.c_str());
            return false;
        }

        out_resource->data_size = sizeof(BitmapFontResourceData);
        out_resource->data = Memory::allocate(out_resource->data_size, AllocationTag::RESOURCE);
        BitmapFontResourceData* resource_data = (BitmapFontResourceData*)out_resource->data;
        out_resource->name = name;

        bool32 res = false;
        switch (file_type)
        {
        case BitmapFontFileType::FNT:
        {
            String shmbmf_filepath(MAX_FILEPATH_LENGTH);
            shmbmf_filepath = full_filepath_wo_extension;
            shmbmf_filepath.append(".shmbmf");
            res = import_fnt_file(&f, name, shmbmf_filepath.c_str(), resource_data);
            break;
        }
        case BitmapFontFileType::SHMBMF:
        {
            res = load_shmbmf_file(&f, full_filepath.c_str(), resource_data);
            break;
        }
        }

        FileSystem::file_close(&f);

        if (!res)
        {
            SHMERRORV("Failed to process mesh file '%s'!", full_filepath.c_str());
            bitmap_font_loader_unload(loader, out_resource);
        }

        return res;

    }

    static void bitmap_font_loader_unload(ResourceLoader* loader, Resource* resource)
    {
        BitmapFontResourceData* data = (BitmapFontResourceData*)resource->data;
        if (data)
            data->~BitmapFontResourceData();

        resource_unload(loader, resource);
    }

    static bool32 import_fnt_file(FileSystem::FileHandle* fnt_file, const char* resource_name, const char* shmbmf_filepath, BitmapFontResourceData* out_data)
    {

        uint32 file_size = FileSystem::get_file_size32(fnt_file);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(fnt_file, file_content, &bytes_read))
        {
            SHMERROR("import_fnt_file - failed to read from file.");
            return false;
        }

        // TODO: Make more configurable. Only supporting UTF-8 codes 0 - 255 for now for quicker lookups.
        out_data->data.glyphs.init(256, 0, AllocationTag::BITMAP_FONT);
        for (uint32 i = 0; i < out_data->data.glyphs.capacity; i++)
        {
            out_data->data.glyphs[i].codepoint = INVALID_ID;
            out_data->data.glyphs[i].kernings_offset = INVALID_ID;
        }

        // Read each line of the file.
        String line(512);
        uint64 line_length = 0;
        uint32 line_number = 1;
        String line_identifier;     
        String values;

        uint32 imported_page_count = 0;
        uint32 imported_kerning_count = 0;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.c_str(), line, &continue_ptr))
        {

            if (line.len() < 1 || line[0] == '#') {
                continue;
            }

            int32 first_space_i = line.index_of(' ');
            if (first_space_i == -1)
                continue;

            mid(line_identifier, line.c_str(), 0, first_space_i);
            mid(values, line.c_str(), first_space_i + 1);
            line_identifier.trim();
            values.trim();

            if (line_identifier.len() == 0)
                continue;

            if (line_identifier == "info")
            {
                CString::safe_scan<char*, uint32*>(values.c_str(), 
                    "face=\"%s\" size=%u ", 
                    out_data->data.face, 
                    &out_data->data.font_size);
            }
            else if (line_identifier == "common")
            {
                imported_page_count = 0;
                uint32 page_count = 0;
                CString::safe_scan<uint32*, int32*, uint32*, uint32*, uint32*>(values.c_str(), 
                    "lineHeight=%u base=%i scaleW=%u scaleH=%u pages=%u ",
                    &out_data->data.line_height,
                    &out_data->data.baseline,
                    &out_data->data.atlas_size_x,
                    &out_data->data.atlas_size_y,
                    &page_count);

                if (page_count == 0)
                {
                    SHMERROR("import_fnt_file - Failed to read font page count or read it as 0!");
                    return false;
                }

                out_data->pages.init(page_count, 0, AllocationTag::BITMAP_FONT);
            }
            else if (line_identifier == "page")
            {
                CString::safe_scan<uint32*, char*>(values.c_str(),
                    "id=%u file=\"%s.",
                    &out_data->pages[imported_page_count].id,
                    out_data->pages[imported_page_count].file);

                imported_page_count++;
            }
            else if (line_identifier == "chars")
            {
                uint32 glyph_count = 0;
                CString::safe_scan<uint32*>(values.c_str(),
                    "count=%u",
                    &glyph_count);

                if (glyph_count == 0)
                {
                    SHMERROR("import_fnt_file - Failed to read font glyph count or read it as 0!");
                    return false;
                }
            }
            else if (line_identifier == "kernings")
            {
                uint32 kerning_count = 0;
                CString::safe_scan<uint32*>(values.c_str(),
                    "count=%u",
                    &kerning_count);

                if (kerning_count == 0)
                {
                    SHMERROR("import_fnt_file - Failed to read font kerning count or read it as 0!");
                    return false;
                }

                out_data->data.kernings.init(kerning_count, DarrayFlags::NON_RESIZABLE, AllocationTag::BITMAP_FONT);
            }
            else if (line_identifier == "char")
            {
                
                uint32 char_id = 0;
                CString::safe_scan<uint32*>(values.c_str(),
                    "id=%u",
                    &char_id);

                if (char_id < 256)
                {
                    FontGlyph& glyph = out_data->data.glyphs[char_id];

                    CString::safe_scan<int32*, uint16*, uint16*, uint16*, uint16*, int16*, int16*, int16*, uint8*>(values.c_str(),
                        "id=%i x=%hu y=%hu width=%hu height=%hu xoffset=%hi yoffset=%hi xadvance=%hi page=%hhu ",
                        &glyph.codepoint,
                        &glyph.x,
                        &glyph.y,
                        &glyph.width,
                        &glyph.height,
                        &glyph.x_offset,
                        &glyph.y_offset,
                        &glyph.x_advance,
                        &glyph.page_id);
                }

            }
            else if (line_identifier == "kerning")
            {
                uint32 first_char_id = 0;
                uint32 second_char_id = 0;
                int16 amount = 0;
                CString::safe_scan<uint32*, uint32*, int16*>(values.c_str(),
                    "first=%u second=%u amount=%hi",
                    &first_char_id,
                    &second_char_id,
                    &amount);

                if (first_char_id < 256 && second_char_id < 256)
                {
                    FontKerning k = { .codepoint_0 = (int32)first_char_id, .codepoint_1 = (int32)second_char_id, .advance = amount };
                    out_data->data.kernings.emplace(k);
                }
            }

            line_number++;
        }

        quick_sort(out_data->data.kernings.data, 0, out_data->data.kernings.count - 1);
        uint32 old_codepoint = INVALID_ID;
        for (uint32 i = 0; i < out_data->data.kernings.count; i++)
        {
            uint32 codepoint = out_data->data.kernings[i].codepoint_0;
            if (codepoint != old_codepoint)
                out_data->data.glyphs[codepoint].kernings_offset = i;
            old_codepoint = codepoint;
        }

        // Output a shmesh file, which will be loaded in the future.
        return write_shmbmf_file(shmbmf_filepath, resource_name, out_data);

    }

    static bool32 write_shmbmf_file(const char* shmbmf_filepath, const char* resource_name, const BitmapFontResourceData* out_data)
    {

        FileSystem::FileHandle f;

        if (!FileSystem::file_open(shmbmf_filepath, FILE_MODE_WRITE, &f)) {
            SHMERRORV("Error opening material file for writing: '%s'", shmbmf_filepath);
            return false;
        }
        SHMDEBUGV("Writing .shmbmf file '%s'...", shmbmf_filepath);

        uint64 written_total = 0;
        uint32 written = 0;

        ShmbmfFileHeader file_header = {};
        file_header.version = 1;
        file_header.name_length = (uint16)CString::length(resource_name);
        file_header.pages_count = out_data->pages.capacity;
        file_header.glyphs_count = out_data->data.glyphs.capacity;
        file_header.kernings_count = out_data->data.kernings.count;
   
        file_header.name_offset = sizeof(ShmbmfFileHeader);
        file_header.pages_offset = file_header.name_offset + file_header.name_length;
        file_header.glyphs_offset = file_header.pages_offset + (file_header.pages_count * sizeof(BitmapFontPage));
        file_header.kernings_offset = file_header.glyphs_offset + (file_header.glyphs_count * sizeof(FontGlyph));

        CString::copy(256, file_header.face, out_data->data.face);
        file_header.line_height = out_data->data.line_height;
        file_header.baseline = out_data->data.baseline;
        file_header.glyph_size = out_data->data.font_size;
        file_header.atlas_size_x = out_data->data.atlas_size_x;
        file_header.atlas_size_y = out_data->data.atlas_size_y;

        FileSystem::write(&f, sizeof(file_header), &file_header, &written);
        written_total += written;

        FileSystem::write(&f, file_header.name_length, resource_name, &written);
        written_total += written;

        for (uint32 i = 0; i < out_data->pages.capacity; i++)
        {
            FileSystem::write(&f, sizeof(BitmapFontPage), &out_data->pages[i], &written);
            written_total += written;
        }     

        FileSystem::write(&f, file_header.glyphs_count * sizeof(FontGlyph), out_data->data.glyphs.data, &written);
        written_total += written;

        FileSystem::write(&f, file_header.kernings_count * sizeof(FontKerning), out_data->data.kernings.data, &written);
        written_total += written;

        FileSystem::file_close(&f);

        return true;

    }

    static bool32 load_shmbmf_file(FileSystem::FileHandle* shmbmf_file, const char* shmbmf_filepath, BitmapFontResourceData* out_data)
    {
        
        uint32 file_size = FileSystem::get_file_size32(shmbmf_file);
        Buffer file_content(file_size, 0);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(shmbmf_file, file_content.data, (uint32)file_content.size, &bytes_read))
        {
            SHMERRORV("load_shmbmf_file - failed to read from file: '%s'.", shmbmf_filepath);
            return false;
        }

        SHMTRACEV("Importing shmbmf file: '%s'.", shmbmf_filepath);

        uint8* read_ptr = (uint8*)file_content.data;
        uint64 read_bytes = 0;

        auto check_buffer_size = [&read_bytes, &file_content](uint64 requested_size)
            {
                SHMASSERT_MSG(read_bytes + requested_size <= file_content.size, "Tried to read outside of buffers memory! File formatting might be corrupted.")
            };

        check_buffer_size(sizeof(ShmbmfFileHeader));
        ShmbmfFileHeader* file_header = (ShmbmfFileHeader*)&read_ptr[read_bytes];
        read_bytes += sizeof(ShmbmfFileHeader);

        CString::copy(256, out_data->data.face, file_header->face);
        out_data->data.line_height = file_header->line_height;
        out_data->data.baseline = file_header->baseline;
        out_data->data.font_size = file_header->glyph_size;
        out_data->data.atlas_size_x = file_header->atlas_size_x;
        out_data->data.atlas_size_y = file_header->atlas_size_y;

        check_buffer_size(file_header->name_length);
        String name;
        name.copy_n((char*)&read_ptr[read_bytes], (uint32)file_header->name_length);
        read_bytes += file_header->name_length;

        check_buffer_size(file_header->pages_count * sizeof(BitmapFontPage));
        out_data->pages.init(file_header->pages_count, 0);
        for (uint32 i = 0; i < file_header->pages_count; i++)
        {         
            out_data->pages[i] = *((BitmapFontPage*)&read_ptr[read_bytes]);
            read_bytes += sizeof(BitmapFontPage);
        }

        if (file_header->glyphs_count)
            out_data->data.glyphs.init(file_header->glyphs_count, 0, AllocationTag::BITMAP_FONT);

        if (file_header->kernings_count)
            out_data->data.kernings.init(file_header->kernings_count, DarrayFlags::NON_RESIZABLE, AllocationTag::BITMAP_FONT);

        uint64 glyphs_size = file_header->glyphs_count * sizeof(FontGlyph);
        check_buffer_size(glyphs_size);
        out_data->data.glyphs.copy_memory(&read_ptr[read_bytes], glyphs_size, 0);
        read_bytes += glyphs_size;

        uint64 kernings_size = file_header->kernings_count * sizeof(FontKerning);
        check_buffer_size(kernings_size);
        out_data->data.kernings.copy_memory(&read_ptr[read_bytes], kernings_size, 0, file_header->kernings_count);
        read_bytes += kernings_size;

        return true;

    }

    

    ResourceLoader bitmap_font_resource_loader_create()
    {
        ResourceLoader loader;
        loader.type = ResourceType::BITMAP_FONT;
        loader.custom_type = 0;
        loader.load = bitmap_font_loader_load;
        loader.unload = bitmap_font_loader_unload;
        loader.type_path = "fonts/";

        return loader;
    }
}
