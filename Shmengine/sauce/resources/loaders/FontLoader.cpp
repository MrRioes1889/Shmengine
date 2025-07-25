#include "FontLoader.hpp"

#include "core/Engine.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "systems/FontSystem.hpp"
#include "utility/Sort.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb/stb_truetype.h"

enum class FontFileType 
{
    NOT_FOUND,
    SHMBMF,
    FNT,
    TTF
};

struct SupportedFontFileType 
{
    const char* extension;
    FontFileType type;
};

struct ShmbmfFileHeader
{
    uint16 version;
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

    static bool32 import_fnt_file(FileSystem::FileHandle* fnt_file, const char* resource_name, const char* shmbmf_filepath, FontResourceData* out_data);
    static bool32 write_shmbmf_file(const char* shmbmf_filepath, const char* name, const FontResourceData* out_data);
    static bool32 load_shmbmf_file(FileSystem::FileHandle* shmbmf_file, const char* shmbmf_filepath, FontResourceData* out_data);

    static bool8 import_ttf_file(FileSystem::FileHandle* ttf_file, const char* resource_name, const char* shmttf_filepath, Buffer* out_binary_buffer);
    static bool8 parse_ttf_binary_data(const char* name, uint16 font_size, const Buffer* binary_buffer, FontResourceData* out_data);

    bool32 font_loader_load(const char* name, uint16 font_size, FontResourceData* out_resource)
    {
        Buffer binary_buffer = {};
        const char* format = "%s%s%s";
        String full_filepath_wo_extension(Constants::max_filepath_length);

        full_filepath_wo_extension.safe_print_s
            (format, Engine::get_assets_base_path(), loader_type_path, name);

        const uint32 supported_file_type_count = 3;
        SupportedFontFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".shmbmf", FontFileType::SHMBMF};
        supported_file_types[1] = { ".fnt", FontFileType::FNT};
        supported_file_types[2] = { ".ttf", FontFileType::TTF};

        String full_filepath(Constants::max_filepath_length);
        FontFileType file_type = FontFileType::NOT_FOUND;
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

        if (file_type == FontFileType::NOT_FOUND) 
        {
            SHMERRORV("Font resource loader failed to find file '%s' with any valid extensions.", full_filepath_wo_extension.c_str());
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
        case FontFileType::FNT:
        {
            String shmbmf_filepath(Constants::max_filepath_length);
            shmbmf_filepath = full_filepath_wo_extension;
            shmbmf_filepath.append(".shmbmf");
            goto_on_fail(import_fnt_file(&f, name, shmbmf_filepath.c_str(), out_resource), failure);
            break;
        }
        case FontFileType::SHMBMF:
        {
            goto_on_fail(load_shmbmf_file(&f, full_filepath.c_str(), out_resource), failure);
            break;
        }
        case FontFileType::TTF:
        {
            String ttf_filepath(Constants::max_filepath_length);
            ttf_filepath = full_filepath_wo_extension;
            ttf_filepath.append(".shmttf");
            goto_on_fail(import_ttf_file(&f, name, ttf_filepath.c_str(), &binary_buffer), failure);
            goto_on_fail_log(parse_ttf_binary_data(name, font_size, &binary_buffer, out_resource), failure, "Failed parse binary data for ttf file '%s'!", full_filepath.c_str());
            break;
        }
        default:
        {
            return false;
        }
        }

        goto_on_fail_log(out_resource->font_size == font_size, failure, "Resource font size does not match expected font size!");
        FileSystem::file_close(&f);
        binary_buffer.free_data();
        return true;

    failure:
        FileSystem::file_close(&f);
        binary_buffer.free_data();
        font_loader_unload(out_resource);
        return false;
    }

    void font_loader_unload(FontResourceData* resource)
    {
        resource->glyphs.free_data();
        resource->kernings.free_data();
        resource->texture_name.free_data();
        resource->texture_buffer.free_data();
    }

    FontConfig font_loader_get_config_from_resource(FontResourceData* resource)
    {
        FontConfig config =
        {
            .type = resource->font_type,
            .font_size = resource->font_size,
            .line_height = resource->line_height,
            .baseline = resource->baseline,
            .atlas_size_x = resource->atlas_size_x,
            .atlas_size_y = resource->atlas_size_y,
            .glyphs_count = resource->glyphs.capacity,
            .kernings_count = resource->kernings.capacity,
            .glyphs = resource->glyphs.data,
            .kernings = resource->kernings.data,
            .texture_name = resource->font_type == FontType::Bitmap ? resource->texture_name.c_str() : 0,
            .texture_buffer_size = resource->font_type == FontType::Truetype ? resource->texture_buffer.capacity * sizeof(resource->texture_buffer[0]) : 0 ,
            .texture_buffer = resource->font_type == FontType::Truetype ? resource->texture_buffer.data : 0
        };

        return config;
    }

    static bool32 import_fnt_file(FileSystem::FileHandle* fnt_file, const char* resource_name, const char* shmbmf_filepath, FontResourceData* out_data)
    {

        uint32 file_size = FileSystem::get_file_size32(fnt_file);
        String file_content(file_size + 1);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(fnt_file, file_content, &bytes_read))
        {
            SHMERROR("import_fnt_file - failed to read from file.");
            return false;
        }

        out_data->font_type = FontType::Bitmap;

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
                String face_name;
                CString::safe_scan(values.c_str(), 
                    "face=\"%s\" size=%hu ", 
                    &face_name, 
                    &out_data->font_size);
                face_name.free_data();
            }
            else if (line_identifier == "common")
            {
                uint32 page_count = 0;
                CString::safe_scan(values.c_str(), 
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
                CString::safe_scan(values.c_str(),
                    "id=%u file=\"%s.",
                    &page_id,
                    &out_data->texture_name);

                if (page_id != 0)
                    out_data->texture_name = "";
            }
            else if (line_identifier == "chars")
            {
                uint32 glyph_count = 0;
                CString::safe_scan(values.c_str(),
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
					out_data->glyphs[i].codepoint = -1;
					out_data->glyphs[i].kernings_offset = Constants::max_u32;
				}
            }
            else if (line_identifier == "kernings")
            {
                uint32 kerning_count = 0;
                CString::safe_scan(values.c_str(),
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

				CString::safe_scan(values.c_str(),
					"id=%i x=%hu y=%hu width=%hu height=%hu xoffset=%hi yoffset=%hi xadvance=%hi page=",
					&glyph.codepoint,
					&glyph.x,
					&glyph.y,
					&glyph.width,
					&glyph.height,
					&glyph.x_offset,
					&glyph.y_offset,
					&glyph.x_advance);
            }
            else if (line_identifier == "kerning")
            {
				FontKerning& kerning = out_data->kernings[imported_kerning_count++];

                CString::safe_scan(values.c_str(),
                    "first=%i second=%i amount=%hi",
                    &kerning.codepoint_0,
                    &kerning.codepoint_1,
                    &kerning.advance);
            }

            line_number++;
        }
    
        if (out_data->texture_name.is_empty() || !out_data->glyphs.data)
        {
            SHMERRORV("Failed to import bitmap font '%s' correctly.", resource_name);
            return false;
        }

        // Output a shmesh file, which will be loaded in the future.
        return write_shmbmf_file(shmbmf_filepath, resource_name, out_data);

    }

    static bool32 write_shmbmf_file(const char* shmbmf_filepath, const char* resource_name, const FontResourceData* out_data)
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
        file_header.texture_name_length = (uint16)out_data->texture_name.len();
        file_header.glyphs_count = out_data->glyphs.capacity;
        file_header.kernings_count = out_data->kernings.capacity;
   
        file_header.texture_name_offset = sizeof(ShmbmfFileHeader);
        file_header.glyphs_offset = file_header.texture_name_offset + file_header.texture_name_length;
        file_header.kernings_offset = file_header.glyphs_offset + (file_header.glyphs_count * sizeof(FontGlyph));

        file_header.line_height = out_data->line_height;
        file_header.baseline = out_data->baseline;
        file_header.font_size = out_data->font_size;
        file_header.atlas_size_x = out_data->atlas_size_x;
        file_header.atlas_size_y = out_data->atlas_size_y;

        FileSystem::write(&f, sizeof(file_header), &file_header, &written);
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

    static bool32 load_shmbmf_file(FileSystem::FileHandle* shmbmf_file, const char* shmbmf_filepath, FontResourceData* out_data)
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

        out_data->font_type = FontType::Bitmap;
        out_data->line_height = file_header->line_height;
        out_data->baseline = file_header->baseline;
        out_data->font_size = file_header->font_size;
        out_data->atlas_size_x = file_header->atlas_size_x;
        out_data->atlas_size_y = file_header->atlas_size_y;

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

    static bool8 import_ttf_file(FileSystem::FileHandle* ttf_file, const char* resource_name, const char* shmttf_filepath, Buffer* out_binary_buffer)
    {
        uint32 file_size = FileSystem::get_file_size32(ttf_file);
        out_binary_buffer->init(file_size, 0, AllocationTag::Resource);
        uint32 bytes_read = 0;
        if (!FileSystem::read_all_bytes(ttf_file, out_binary_buffer->data, (uint32)out_binary_buffer->size, &bytes_read))
        {
            SHMERROR("import_ttf_file - failed to read from file.");
            return false;
        }

        return true;
    }

    static bool8 parse_ttf_binary_data(const char* name, uint16 font_size, const Buffer* binary_buffer, FontResourceData* out_data)
    {
        stbtt_fontinfo info;
        if (!stbtt_InitFont(&info, (unsigned char*)binary_buffer->data, 0))
        {
            SHMERRORV("Failed to parse truetype font '%s'", name);
            return false;
        }

        Sarray<int32> codepoints = {};
        codepoints.init(256, 0, AllocationTag::Font);
        for (int32 i = 0; i < (int32)codepoints.capacity; i++)
            codepoints[i] = i;

        out_data->font_type = FontType::Truetype;
        out_data->font_size = font_size;
        uint16 atlas_size = SHMAX(1024, out_data->font_size * 16);
        out_data->atlas_size_x = atlas_size;
        out_data->atlas_size_y = atlas_size;

        float32 scale = stbtt_ScaleForPixelHeight(&info, (float32)font_size);
        int32 ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
        out_data->line_height = (uint16)((ascent - descent + line_gap) * scale);

        uint32 pack_image_size = out_data->atlas_size_x * out_data->atlas_size_y * sizeof(uint8);
        Sarray<uint8> pixels(pack_image_size, 0);
        Sarray<stbtt_packedchar> packed_chars(codepoints.capacity, 0);

        stbtt_pack_context context;
        if (!stbtt_PackBegin(&context, pixels.data, out_data->atlas_size_x, out_data->atlas_size_y, 0, 1, 0))
        {
            SHMERROR("stbtt_PackBegin failed");
            return false;
        }

        stbtt_pack_range range;
        range.first_unicode_codepoint_in_range = 0;
        range.font_size = (float32)out_data->font_size;
        range.num_chars = codepoints.capacity;
        range.chardata_for_range = packed_chars.data;
        range.array_of_unicode_codepoints = codepoints.data;
        if (!stbtt_PackFontRanges(&context, (unsigned char*)binary_buffer->data, 0, &range, 1))
        {
            SHMERROR("stbtt_PackFontRanges failed");
            return false;
        }
        stbtt_PackEnd(&context);

        out_data->texture_buffer.init(pack_image_size, 0, AllocationTag::Resource);
        for (uint32 i = 0; i < pack_image_size; i++)
            out_data->texture_buffer[i] = ((uint32)pixels[i] << 24) + ((uint32)pixels[i] << 16) + ((uint32)pixels[i] << 8) + (uint32)pixels[i];
        pixels.free_data();

        out_data->glyphs.free_data();
        out_data->glyphs.init(codepoints.capacity, 0, AllocationTag::Font);
        for (uint32 i = 0; i < out_data->glyphs.capacity; i++)
        {
            stbtt_packedchar* pc = &packed_chars[i];
            FontGlyph* g = &out_data->glyphs[i];
            g->codepoint = codepoints[i];
            g->x_offset = (int16)pc->xoff;
            g->y_offset = (int16)pc->yoff;
            g->width = pc->x1 - pc->x0;
            g->height = pc->y1 - pc->y0;
            g->x = pc->x0;  // xmin;
            g->y = pc->y0; // Flipping y coordinates
            g->x_advance = (int16)pc->xadvance;
        }
        packed_chars.free_data();

        uint32 kerning_count = stbtt_GetKerningTableLength(&info);
        if (kerning_count)
        {
            SHMERROR("Truetype fonts with kerning are not implemented properly yet!"); 
            return false;
        }

        /*
		if (kerning_count)
		{
			out_data->kernings.init(kerning_count, 0, AllocationTag::Font);

			Sarray<stbtt_kerningentry> kerning_entries(kerning_count, 0);
			int32 res = stbtt_GetKerningTable(&info, kerning_entries.data, (int32)kerning_entries.capacity);
			if (res != (int32)kerning_entries.capacity)
			{
				SHMERROR("stbtt_GetKerningTable failed");
				return false;
			}

			for (uint32 i = 0; i < kerning_count; i++)
			{
				FontKerning* k = &out_data->kernings[i];
				k->codepoint_0 = kerning_entries[i].glyph1;
				k->codepoint_1 = kerning_entries[i].glyph2;
				k->advance = (int16)kerning_entries[i].advance;
			}

			kerning_entries.free_data();
		}
        */

		return true;
    }
}
