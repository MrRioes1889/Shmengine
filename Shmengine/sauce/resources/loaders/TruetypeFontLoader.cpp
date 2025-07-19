#include "TruetypeFontLoader.hpp"

#include "core/Engine.hpp"
#include "core/Logging.hpp"
#include "core/Memory.hpp"
#include "utility/String.hpp"
#include "platform/FileSystem.hpp"
#include "utility/Sort.hpp"
#include "systems/FontSystem.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb/stb_truetype.h"

enum class TruetypeFontFileType {
    NOT_FOUND,
    TTF
};

struct SupportedTruetypeFontFileType {
    const char* extension;
    TruetypeFontFileType type;
};

namespace ResourceSystem
{

    static const char* loader_type_path = "fonts/";
   
    static bool8 import_ttf_file(FileSystem::FileHandle* ttf_file, const char* resource_name, const char* shmttf_filepath, Buffer* out_binary_buffer);
    static bool8 write_shmttf_file(const char* shmttf_filepath, const char* name, const Buffer* binary_buffer);
    static bool8 load_shmttf_file(FileSystem::FileHandle* shmttf_file, const char* shmttf_filepath, Buffer* out_binary_buffer);

    static bool8 parse_binary_data(const char* name, uint16 font_size, const Buffer* binary_buffer, TruetypeFontResourceData* out_data);

    bool8 truetype_font_loader_load(const char* name, uint16 font_size, TruetypeFontResourceData* out_data)
    {
        Buffer binary_buffer = {};
        const char* format = "%s%s%s";
        String full_filepath_wo_extension(Constants::max_filepath_length);

        full_filepath_wo_extension.safe_print_s<const char*, const char*, const char*>
            (format, Engine::get_assets_base_path(), loader_type_path, name);

        const uint32 supported_file_type_count = 1;
        SupportedTruetypeFontFileType supported_file_types[supported_file_type_count] = {};
        supported_file_types[0] = { ".ttf", TruetypeFontFileType::TTF };

        String full_filepath(Constants::max_filepath_length);
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
            goto failure;
        }

        FileSystem::FileHandle f;
        if (!FileSystem::file_open(full_filepath.c_str(), FileMode::FILE_MODE_READ, &f))
        {
            SHMERRORV("truetype_font_loader_load - Failed to open file for loading truetype font '%s'", full_filepath.c_str());
            goto failure;
        }

        switch (file_type)
        {
        case TruetypeFontFileType::TTF:
        {
            String shmttf_filepath(Constants::max_filepath_length);
            shmttf_filepath = full_filepath_wo_extension;
            shmttf_filepath.append(".shmttf");
            if (!import_ttf_file(&f, name, shmttf_filepath.c_str(), &binary_buffer))
                goto failure;
            break;
        }
        default:
        {
            return false;
        }
        }

        FileSystem::file_close(&f);

        if (!parse_binary_data(name, font_size, &binary_buffer, out_data))
        {
            SHMERRORV("Failed parse binary data for ttf file '%s'!", full_filepath.c_str());
            goto failure;
        }

        binary_buffer.free_data();
        return true;

    failure:
        binary_buffer.free_data();
        truetype_font_loader_unload(out_data);
        return false;
    }

   void truetype_font_loader_unload(TruetypeFontResourceData* data)
    {
       data->glyphs.free_data();
       data->kernings.free_data();
       data->texture_buffer.free_data();
    }

   FontConfig truetype_font_loader_get_config_from_resource(TruetypeFontResourceData* resource)
   {
        FontConfig config =
        {
            .type = FontType::Truetype,
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
            .texture_buffer_size = resource->texture_buffer.capacity * sizeof(resource->texture_buffer[0]),
            .texture_buffer = resource->texture_buffer.data
        };

        return config;
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

    static bool8 parse_binary_data(const char* name, uint16 font_size, const Buffer* binary_buffer, TruetypeFontResourceData* out_data)
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
            g->page_id = 0;
            g->x_offset = (int16)pc->xoff;
            g->y_offset = (int16)pc->yoff;
            g->x = pc->x0;  // xmin;
            g->y = pc->y0;
            g->width = pc->x1 - pc->x0;
            g->height = pc->y1 - pc->y0;
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
