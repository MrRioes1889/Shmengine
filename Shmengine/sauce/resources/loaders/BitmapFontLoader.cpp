#include "BitmapFontLoader.hpp"

#include "core/Engine.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "systems/FontSystem.hpp"
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
    uint16 face_name_length;
    uint32 face_name_offset;
    uint16 texture_name_length;
    uint32 texture_name_offset;
    uint32 glyphs_count;
    uint32 glyphs_offset;
    uint32 kernings_count;
    uint32 kernings_offset;

    uint16 line_height;
    int16 baseline;
    uint16 atlas_size_x;
    uint16 atlas_size_y;
    uint16 font_size;
};

namespace ResourceSystem
{

    static const char* loader_type_path = "fonts/";

    static bool32 import_fnt_file(FileSystem::FileHandle* fnt_file, const char* resource_name, const char* shmbmf_filepath, BitmapFontResourceData* out_data);
    static bool32 write_shmbmf_file(const char* shmbmf_filepath, const char* name, const BitmapFontResourceData* out_data);
    static bool32 load_shmbmf_file(FileSystem::FileHandle* shmbmf_file, const char* shmbmf_filepath, BitmapFontResourceData* out_data);

    bool32 bitmap_font_loader_load(const char* name, BitmapFontResourceData* out_resource)
    {

        const char* format = "%s%s%s";
        String full_filepath_wo_extension(Constants::max_filepath_length);

        full_filepath_wo_extension.safe_print_s<const char*, const char*, const char*>
            (format, Engine::get_assets_base_path(), loader_type_path, name);

        const uint32 supported_file_type_count = 2;
        SupportedBitmapFontFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".shmbmf", BitmapFontFileType::SHMBMF};
        supported_file_types[1] = { ".fnt", BitmapFontFileType::FNT};

        String full_filepath(Constants::max_filepath_length);
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

        bool32 res = false;
        switch (file_type)
        {
        case BitmapFontFileType::FNT:
        {
            String shmbmf_filepath(Constants::max_filepath_length);
            shmbmf_filepath = full_filepath_wo_extension;
            shmbmf_filepath.append(".shmbmf");
            res = import_fnt_file(&f, name, shmbmf_filepath.c_str(), out_resource);
            break;
        }
        case BitmapFontFileType::SHMBMF:
        {
            res = load_shmbmf_file(&f, full_filepath.c_str(), out_resource);
            break;
        }
        default:
        {
            return false;
        }
        }

        FileSystem::file_close(&f);

        if (!res)
        {
            SHMERRORV("Failed to process mesh file '%s'!", full_filepath.c_str());
            bitmap_font_loader_unload(out_resource);
        }

        return res;

    }

    void bitmap_font_loader_unload(BitmapFontResourceData* resource)
    {
        resource->glyphs.free_data();
        resource->kernings.free_data();
        resource->face_name.free_data();
        resource->texture_name.free_data();
    }

    FontConfig bitmap_font_loader_get_config_from_resource(BitmapFontResourceData* resource)
    {
        FontConfig config =
        {
            .type = FontType::Bitmap,
            .font_size = resource->font_size,
            .line_height = resource->line_height,
            .baseline = resource->baseline,
            .atlas_size_x = resource->atlas_size_x,
            .atlas_size_y = resource->atlas_size_y,
            .tab_x_advance = resource->tab_x_advance,
            .glyphs_count = resource->glyphs.capacity,
            .kernings_count = resource->kernings.capacity,
            .glyphs = resource->glyphs.data,
            .kernings = resource->kernings.data,
            .texture_name = resource->texture_name.c_str(),
            .texture_buffer_size = 0,
            .texture_buffer = 0
        };

        return config;
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

        // Read each line of the file.
        String line(512);
        uint64 line_length = 0;
        uint32 line_number = 1;
        String line_identifier;     
        String values;

        uint32 imported_glyph_count = 0;
        uint32 imported_kerning_count = 0;

        const char* continue_ptr = 0;
        while (FileSystem::read_line(file_content.c_str(), line, &continue_ptr))
        {

            if (line.len() < 1 || line[0] == '#') 
            {
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
                CString::safe_scan(values.c_str(), 
                    "face=\"%s\" size=%hu ", 
                    &out_data->face_name, 
                    &out_data->font_size);
            }
            else if (line_identifier == "common")
            {
                uint32 page_count = 0;
                CString::safe_scan<uint16*, int16*, uint16*, uint16*, uint32*>(values.c_str(), 
                    "lineHeight=%hu base=%hi scaleW=%hu scaleH=%hu pages=%u ",
                    &out_data->line_height,
                    &out_data->baseline,
                    &out_data->atlas_size_x,
                    &out_data->atlas_size_y,
                    &page_count);

                if (page_count == 0)
                {
                    SHMERROR("import_fnt_file - Failed to read font page count or read it as 0!");
                    return false;
                }
                else if (page_count > 1)
                {
                    SHMWARNV("Bitmap font '%s' has more than 1 page. Only first one will be imported.", resource_name);
                }
            }
            else if (line_identifier == "page")
            {
                uint32 page_id = Constants::max_u32;
                CString::safe_scan<uint32*, String*>(values.c_str(),
                    "id=%u file=\"%s.",
                    &page_id,
                    &out_data->texture_name);

                if (page_id != 0)
                    out_data->texture_name = "";
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

				out_data->glyphs.init(glyph_count, 0, AllocationTag::Font);
				for (uint32 i = 0; i < out_data->glyphs.capacity; i++)
				{
					out_data->glyphs[i].codepoint = Constants::max_u32;
					out_data->glyphs[i].kernings_offset = Constants::max_u32;
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

                out_data->kernings.init(kerning_count, DarrayFlags::NonResizable, AllocationTag::Font);
            }
            else if (line_identifier == "char")
            {
				FontGlyph& glyph = out_data->glyphs[imported_glyph_count++];

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
            else if (line_identifier == "kerning")
            {
				FontKerning& kerning = out_data->kernings[imported_kerning_count++];

                CString::safe_scan<int32*, int32*, int16*>(values.c_str(),
                    "first=%i second=%i amount=%hi",
                    &kerning.codepoint_0,
                    &kerning.codepoint_1,
                    &kerning.advance);
            }

            line_number++;
        }
    
        if (out_data->face_name.is_empty() || out_data->texture_name.is_empty() || !out_data->glyphs.data)
        {
            SHMERRORV("Failed to import bitmap font '%s' correctly.", resource_name);
            bitmap_font_loader_unload(out_data);
            return false;
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
        file_header.face_name_length = (uint16)out_data->face_name.len();
        file_header.texture_name_length = (uint16)out_data->texture_name.len();
        file_header.glyphs_count = out_data->glyphs.capacity;
        file_header.kernings_count = out_data->kernings.capacity;
   
        file_header.face_name_offset = sizeof(ShmbmfFileHeader);
        file_header.texture_name_offset = file_header.face_name_offset + file_header.texture_name_length;
        file_header.glyphs_offset = file_header.texture_name_offset + file_header.texture_name_length;
        file_header.kernings_offset = file_header.glyphs_offset + (file_header.glyphs_count * sizeof(FontGlyph));

        file_header.line_height = out_data->line_height;
        file_header.baseline = out_data->baseline;
        file_header.font_size = out_data->font_size;
        file_header.atlas_size_x = out_data->atlas_size_x;
        file_header.atlas_size_y = out_data->atlas_size_y;

        FileSystem::write(&f, sizeof(file_header), &file_header, &written);
        written_total += written;

        FileSystem::write(&f, file_header.face_name_length, out_data->face_name.c_str(), &written);
        written_total += written;

        FileSystem::write(&f, file_header.texture_name_length, out_data->texture_name.c_str(), &written);
        written_total += written;

        FileSystem::write(&f, file_header.glyphs_count * sizeof(FontGlyph), out_data->glyphs.data, &written);
        written_total += written;

        FileSystem::write(&f, file_header.kernings_count * sizeof(FontKerning), out_data->kernings.data, &written);
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

        out_data->line_height = file_header->line_height;
        out_data->baseline = file_header->baseline;
        out_data->font_size = file_header->font_size;
        out_data->atlas_size_x = file_header->atlas_size_x;
        out_data->atlas_size_y = file_header->atlas_size_y;

        check_buffer_size(file_header->face_name_length);
	    out_data->face_name.copy_n((const char*)&read_ptr[read_bytes], (uint32)file_header->face_name_length);
        read_bytes += file_header->face_name_length;

        check_buffer_size(file_header->texture_name_length);
        out_data->texture_name.copy_n((const char*)&read_ptr[read_bytes], (uint32)file_header->texture_name_length);
        read_bytes += file_header->texture_name_length;

        if (file_header->glyphs_count)
            out_data->glyphs.init(file_header->glyphs_count, 0, AllocationTag::Font);

        if (file_header->kernings_count)
            out_data->kernings.init(file_header->kernings_count, DarrayFlags::NonResizable, AllocationTag::Font);

        uint64 glyphs_size = file_header->glyphs_count * sizeof(FontGlyph);
        check_buffer_size(glyphs_size);
        out_data->glyphs.copy_memory(&read_ptr[read_bytes], file_header->glyphs_count, 0);
        read_bytes += glyphs_size;

        uint64 kernings_size = file_header->kernings_count * sizeof(FontKerning);
        check_buffer_size(kernings_size);
        out_data->kernings.copy_memory(&read_ptr[read_bytes], file_header->kernings_count, 0);
        read_bytes += kernings_size;

        return true;

    }

}
