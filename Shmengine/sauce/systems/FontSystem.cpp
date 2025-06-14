#include "FontSystem.hpp"

#include "systems/ResourceSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "resources/UIText.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/loaders/TruetypeFontLoader.hpp"
#include "resources/loaders/BitmapFontLoader.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb/stb_truetype.h"

namespace FontSystem
{

	struct BitmapFontLookup
	{
		uint16 id;
		uint16 reference_count;
		BitmapFontResourceData font_data;
	};

	/*struct TruetypeFontInvariantData
	{	
		float32 scale;
	};*/

	struct TruetypeFontLookup
	{
		uint16 id;
		uint16 reference_count;	
		TruetypeFontResourceData resource_data;
		Darray<int32> codepoints;
		Darray<FontAtlas> size_variants;	
		stbtt_fontinfo info;
	};

	struct SystemState
	{
		SystemConfig config;

		BitmapFontLookup* registered_bitmap_fonts;
		Hashtable<uint16> registered_bitmap_font_table;

		TruetypeFontLookup* registered_truetype_fonts;
		Hashtable<uint16> registered_truetype_font_table;

	};

	static bool32 create_font_data(FontAtlas* font);

	static bool32 create_truetype_font_variant(TruetypeFontLookup* lookup, uint16 size, const char* font_name, FontAtlas* out_variant);
	static bool32 rebuild_truetype_font_variant(TruetypeFontLookup* lookup, FontAtlas* variant);

	static void destroy_font_data(FontAtlas* font);

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		system_state->config = *sys_config;

		uint64 bitmap_font_array_size = sizeof(BitmapFontLookup) * sys_config->max_bitmap_font_config_count;
		system_state->registered_bitmap_fonts = (BitmapFontLookup*)allocator_callback(allocator, bitmap_font_array_size);

		uint64 bitmap_hashtable_data_size = sizeof(uint16) * sys_config->max_bitmap_font_config_count;
		void* bitmap_hashtable_data = allocator_callback(allocator, bitmap_hashtable_data_size);
		system_state->registered_bitmap_font_table.init(sys_config->max_bitmap_font_config_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, bitmap_hashtable_data);

		system_state->registered_bitmap_font_table.floodfill(Constants::max_u16);

		for (uint32 i = 0; i < sys_config->max_bitmap_font_config_count; ++i)
		{
			system_state->registered_bitmap_fonts[i].id = Constants::max_u16;
			system_state->registered_bitmap_fonts[i].reference_count = 0;
		}

		uint64 truetype_font_array_size = sizeof(TruetypeFontLookup) * sys_config->max_truetype_font_config_count;
		system_state->registered_truetype_fonts = (TruetypeFontLookup*)allocator_callback(allocator, truetype_font_array_size);

		uint64 truetype_hashtable_data_size = sizeof(uint16) * sys_config->max_truetype_font_config_count;
		void* truetype_hashtable_data = allocator_callback(allocator, truetype_hashtable_data_size);
		system_state->registered_truetype_font_table.init(sys_config->max_truetype_font_config_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, truetype_hashtable_data);

		system_state->registered_truetype_font_table.floodfill(Constants::max_u16);

		for (uint32 i = 0; i < sys_config->max_truetype_font_config_count; ++i)
		{
			system_state->registered_truetype_fonts[i].id = Constants::max_u16;
			system_state->registered_truetype_fonts[i].reference_count = 0;
		}


		for (uint32 i = 0; i < system_state->config.default_bitmap_font_count; i++)
		{
			if (!load_bitmap_font(system_state->config.bitmap_font_configs[i]))
				SHMERRORV("Failed to load default bitmap font: %s", system_state->config.bitmap_font_configs[i].name);
		}

		for (uint32 i = 0; i < system_state->config.default_truetype_font_count; i++)
		{
			if (!load_truetype_font(system_state->config.truetype_font_configs[i]))
				SHMERRORV("Failed to load default truetype font: %s", system_state->config.truetype_font_configs[i].name);
		}

		return true;
	}

	void system_shutdown(void* state)
	{
		for (uint16 i = 0; i < system_state->config.max_bitmap_font_config_count; i++)
		{
			if (system_state->registered_bitmap_fonts[i].id != Constants::max_u16)
			{
				FontAtlas* font = &system_state->registered_bitmap_fonts[i].font_data.atlas;
				destroy_font_data(font);
				system_state->registered_bitmap_fonts[i].id = Constants::max_u16;

				ResourceSystem::bitmap_font_loader_unload(&system_state->registered_bitmap_fonts[i].font_data);
			}
		}

		for (uint16 i = 0; i < system_state->config.max_truetype_font_config_count; i++)
		{
			if (system_state->registered_truetype_fonts[i].id == Constants::max_u16)
				continue;

			for (uint32 j = 0; j < system_state->registered_truetype_fonts[i].size_variants.count; j++)
				destroy_font_data(&system_state->registered_truetype_fonts[i].size_variants[j]);

			system_state->registered_truetype_fonts[i].codepoints.free_data();
			system_state->registered_truetype_fonts[i].size_variants.free_data();
			system_state->registered_truetype_fonts[i].id = Constants::max_u16;
			ResourceSystem::truetype_font_loader_unload(&system_state->registered_truetype_fonts[i].resource_data);
		}
	}

	bool32 load_truetype_font(const TruetypeFontConfig& config)
	{
		
		uint16 id = system_state->registered_truetype_font_table.get_value(config.name);

		if (id != Constants::max_u16)
		{
			SHMWARNV("load_truetype_font - Font named '%s' already exists!", config.name);
			return true;
		}

		for (uint16 i = 0; i < system_state->config.max_truetype_font_config_count; i++)
		{
			if (system_state->registered_truetype_fonts[i].id == Constants::max_u16)
			{
				id = i;
				break;
			}
		}

		if (id == Constants::max_u16)
		{
			SHMERROR("load_truetype_font - No space left to allocate bitmap font!");
			return false;
		}

		TruetypeFontLookup* lookup = &system_state->registered_truetype_fonts[id];
		if (!ResourceSystem::truetype_font_loader_load(config.resource_name, &lookup->resource_data))
		{
			SHMERROR("load_truetype_font - Failed to load resource!");
			return false;
		}	

		if (!stbtt_InitFont(&lookup->info, (unsigned char*)lookup->resource_data.binary_data.data, 0))
		{
			SHMERRORV("load_truetype_font - Failed to init truetype font '%s'", config.name);
			return false;
		}

		lookup->size_variants.init(1, 0, AllocationTag::TRUETYPE_FONT);
		lookup->codepoints.init(96, 0, AllocationTag::TRUETYPE_FONT);
		lookup->codepoints.push(-1);
		for (uint32 i = 0; i < 95; i++)
			lookup->codepoints.push(i + 32);

		FontAtlas* variant = &lookup->size_variants[lookup->size_variants.emplace()];
		if (!create_truetype_font_variant(lookup, config.default_size, config.name, variant)) 
		{
			SHMERRORV("load_truetype_font - Failed to create variant: %s", lookup->resource_data.face);
			return false;
		}	

		if (!create_font_data(variant))
		{
			SHMERRORV("load_truetype_font - Failed to create font data for variant: %s", lookup->resource_data.face);
			return false;
		}
		
		system_state->registered_truetype_font_table.set_value(config.name, id);
		lookup->id = id;

		return true;

	}

	bool32 load_bitmap_font(const BitmapFontConfig& config)
	{

		uint16 id = system_state->registered_bitmap_font_table.get_value(config.name);

		if (id != Constants::max_u16)
		{
			SHMWARNV("load_bitmap_font - Font named '%s' already exists!", config.name);
			return true;
		}

		for (uint16 i = 0; i < system_state->config.max_bitmap_font_config_count; i++)
		{
			if (system_state->registered_bitmap_fonts[i].id == Constants::max_u16)
			{
				id = i;
				break;
			}
		}

		if (id == Constants::max_u16)
		{
			SHMERROR("load_bitmap_font - No space left to allocate bitmap font!");
			return false;
		}

		BitmapFontLookup* lookup = &system_state->registered_bitmap_fonts[id];
		if (!ResourceSystem::bitmap_font_loader_load(config.resource_name, 0, &lookup->font_data))
		{
			SHMERROR("load_bitmap_font - Failed to load resource!");
			return false;
		}

		lookup->font_data.atlas.map.texture = TextureSystem::acquire(lookup->font_data.pages[0].file, true);

		system_state->registered_bitmap_font_table.set_value(config.name, id);
		lookup->id = id;

		return create_font_data(&lookup->font_data.atlas);

	}

	bool32 acquire(const char* font_name, uint16 font_size, UIText* text)
	{

		if (text->type == UITextType::BITMAP)
		{
			uint16 id = system_state->registered_bitmap_font_table.get_value(font_name);

			if (id == Constants::max_u16)
			{
				SHMERRORV("acquire - No bitmap font named '%s' found!", font_name);
				return false;
			}

			BitmapFontLookup* lookup = &system_state->registered_bitmap_fonts[id];
			lookup->reference_count++;
			text->font_atlas = &lookup->font_data.atlas;		

			return true;
		}
		else if (text->type == UITextType::TRUETYPE)
		{
			uint16 id = system_state->registered_truetype_font_table.get_value(font_name);

			if (id == Constants::max_u16)
			{
				SHMERRORV("acquire - No truetype font named '%s' found!", font_name);
				return false;
			}

			TruetypeFontLookup* lookup = &system_state->registered_truetype_fonts[id];			
			FontAtlas* variant = 0;
			for (uint32 i = 0; i < lookup->size_variants.count; i++)
			{
				if (lookup->size_variants[i].font_size == font_size)
					variant = &lookup->size_variants[i];
			}

			if (variant)
			{
				text->font_atlas = variant;
				lookup->reference_count++;
				return true;
			}
			
			variant = &lookup->size_variants[lookup->size_variants.emplace()];
			if (!create_truetype_font_variant(lookup, font_size, font_name, variant)) {
				SHMERRORV("Failed to create variant: %s", font_name);
				return false;
			}

			if (!create_font_data(variant))
			{
				SHMERRORV("Failed to create font data for variant: %s", lookup->resource_data.face);
				return false;
			}
			text->font_atlas = variant;
			lookup->reference_count++;

			return true;
		}

		SHMERROR("acquire - unrecognized font type!");
		return false;

	}

	bool32 release(UIText* text)
	{
		return true;
	}

	bool32 verify_atlas(FontAtlas* font, const char* text)
	{
		if (font->type == FontType::BITMAP)
		{
			return true;
		}

		uint16 id = system_state->registered_truetype_font_table.get_value(font->face);

		if (id == Constants::max_u16)
		{
			SHMERRORV("verify_atlas - No truetype font named '%s' found!", font->face);
			return false;
		}

		TruetypeFontLookup* lookup = &system_state->registered_truetype_fonts[id];

		uint32 char_length = CString::length(text);
		uint32 added_codepoint_count = 0;
		for (uint32 i = 0; i < char_length;) {
			int32 codepoint;
			uint8 advance;
			if (!utf8_bytes_to_codepoint(text, i, &codepoint, &advance)) {
				SHMERROR("bytes_to_codepoint failed to get codepoint.");
				++i;
				continue;
			}
			else {
				// Check if the codepoint is already contained. Note that ascii
				// codepoints are always included, so checking those may be skipped.
				i += advance;
				if (codepoint < 128) {
					continue;
				}

				bool32 found = false;
				for (uint32 j = 95; j < lookup->codepoints.count; ++j) {
					if (lookup->codepoints[j] == codepoint) {
						found = true;
						break;
					}
				}
				if (!found) {
					lookup->codepoints.emplace(codepoint);
					added_codepoint_count++;
				}
			}
		}

		// If codepoints were added, rebuild the atlas.
		if (added_codepoint_count > 0) {
			return rebuild_truetype_font_variant(lookup, font);
		}

		// Otherwise, proceed as normal.
		return true;

	}

	static bool32 create_font_data(FontAtlas* font)
	{

		font->map.filter_magnify = font->map.filter_minify = TextureFilter::LINEAR;
		font->map.repeat_u = font->map.repeat_v = font->map.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
		if (!Renderer::texture_map_acquire_resources(&font->map)) {
			SHMERROR("setup_font_data - Unable to acquire resources for font atlas texture map.");
			return false;
		}

		if (!font->tab_x_advance) {
			for (uint32 i = 0; i < font->glyphs.capacity; ++i) {
				if (font->glyphs[i].codepoint == '\t') {
					font->tab_x_advance = font->glyphs[i].x_advance;
					break;
				}
			}

			if (!font->tab_x_advance) {
				for (uint32 i = 0; i < font->glyphs.capacity; ++i) {
					if (font->glyphs[i].codepoint == ' ') {
						font->tab_x_advance = font->glyphs[i].x_advance * 4.0f;
						break;
					}
				}
				if (!font->tab_x_advance) {
					font->tab_x_advance = font->font_size * 4.0f;
				}
			}
		}

		return true;

	}

	static void destroy_font_data(FontAtlas* font)
	{
		Renderer::texture_map_release_resources(&font->map);

		if (font->type == FontType::BITMAP && font->map.texture)
			TextureSystem::release(font->map.texture->name);
		font->map.texture = 0;
	}

	static bool32 create_truetype_font_variant(TruetypeFontLookup* lookup, uint16 size, const char* font_name, FontAtlas* out_variant)
	{

		out_variant->atlas_size_x = 1024;
		out_variant->atlas_size_y = 1024;
		out_variant->font_size = size;
		out_variant->type = FontType::TRUETYPE;
		CString::copy(font_name, out_variant->face, sizeof(out_variant->face));

		char font_texture_name[255];
		CString::print_s(font_texture_name, sizeof(font_texture_name), "__truetype_fontatlas_%s_sz%hi__", font_name, size);
		out_variant->map.texture = TextureSystem::acquire_writable(font_texture_name, out_variant->atlas_size_x, out_variant->atlas_size_y, 4, true);

		float32 scale = stbtt_ScaleForPixelHeight(&lookup->info, (float32)size);
		int32 ascent, descent, line_gap;
		stbtt_GetFontVMetrics(&lookup->info, &ascent, &descent, &line_gap);
		out_variant->line_height = (uint32)((ascent - descent + line_gap) * scale);

		return rebuild_truetype_font_variant(lookup, out_variant);

	}

	static bool32 rebuild_truetype_font_variant(TruetypeFontLookup* lookup, FontAtlas* variant)
	{

		uint32 pack_image_size = variant->atlas_size_x * variant->atlas_size_y * sizeof(uint8);
		Sarray<uint8> pixels(pack_image_size, 0);
		Sarray<stbtt_packedchar> packed_chars(lookup->codepoints.count, 0);

		stbtt_pack_context context;
		if (!stbtt_PackBegin(&context, pixels.data, variant->atlas_size_x, variant->atlas_size_y, 0, 1, 0))
		{
			SHMERROR("stbtt_PackBegin failed");
			return false;
		}

		stbtt_pack_range range;
		range.first_unicode_codepoint_in_range = 0;
		range.font_size = (float32)variant->font_size;
		range.num_chars = lookup->codepoints.count;
		range.chardata_for_range = packed_chars.data;
		range.array_of_unicode_codepoints = lookup->codepoints.data;
		if (!stbtt_PackFontRanges(&context, (unsigned char*)lookup->resource_data.binary_data.data, 0, &range, 1))
		{
			SHMERROR("stbtt_PackFontRanges failed");
			return false;
		}

		stbtt_PackEnd(&context);

		Sarray<uint32> rgba_pixels(pack_image_size, 0);
		for (uint32 i = 0; i < pack_image_size; i++)
			rgba_pixels[i] = ((uint32)pixels[i] << 24) + ((uint32)pixels[i] << 16) + ((uint32)pixels[i] << 8) + (uint32)pixels[i];

		TextureSystem::write_to_texture(variant->map.texture, 0, rgba_pixels.capacity * sizeof(uint32), (uint8*)rgba_pixels.data);

		pixels.free_data();
		rgba_pixels.free_data();

		variant->glyphs.free_data();
		variant->glyphs.init(lookup->codepoints.count, 0, AllocationTag::TRUETYPE_FONT);
		for (uint32 i = 0; i < variant->glyphs.capacity; i++)
		{
			stbtt_packedchar* pc = &packed_chars[i];
			FontGlyph* g = &variant->glyphs[i];
			g->codepoint = lookup->codepoints[i];
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

		variant->kernings.free_data();
		uint32 kerning_count = stbtt_GetKerningTableLength(&lookup->info);
		if (kerning_count)
		{
			variant->kernings.init(kerning_count, 0, AllocationTag::TRUETYPE_FONT);

			Sarray<stbtt_kerningentry> kerning_entries(kerning_count, 0);
			int32 res = stbtt_GetKerningTable(&lookup->info, kerning_entries.data, (int32)kerning_entries.capacity);
			if (res != (int32)kerning_entries.capacity)
			{
				SHMERROR("stbtt_GetKerningTable failed");
				return false;
			}

			for (uint32 i = 0; i < kerning_count; i++)
			{
				FontKerning* k = &variant->kernings[variant->kernings.emplace()];
				k->codepoint_0 = kerning_entries[i].glyph1;
				k->codepoint_1 = kerning_entries[i].glyph2;
				k->advance = (int16)kerning_entries[i].advance;
			}

			kerning_entries.free_data();
		}

		return true;

	}

	uint32 utf8_string_length(const char* str , bool32 ignore_control_characters)
	{
		uint32 length = 0;
		for (uint32 i = 0; str[i]; i++)
		{
			uint32 c = (uint32)str[i];
			if (c == 0) 
			{
				break;
			}
			else if (c >= 0 && c < 127) 
			{
				if (ignore_control_characters && c < 32)
					continue;
			}
			else if ((c & 0xE0) == 0xC0) 
			{
				// Double-byte character, increment once more.
				i += 1;
			}
			else if ((c & 0xF0) == 0xE0) 
			{
				// Triple-byte character, increment twice more.
				i += 2;
			}
			else if ((c & 0xF8) == 0xF0) 
			{
				// 4-byte character, increment thrice more.
				i += 3;
			}
			else 
			{
				// NOTE: Not supporting 5 and 6-byte characters; return as invalid UTF-8.
				SHMERROR("utf8_string_length - Not supporting 5 and 6-byte characters; Invalid UTF-8.");
				return 0;
			}

			length++;
		}

		return length;
	}

	bool32 utf8_bytes_to_codepoint(const char* bytes, uint32 offset, int32* out_codepoint, uint8* out_advance) 
	{
		int32 codepoint = (uint32)bytes[offset];
		if (codepoint >= 0 && codepoint < 0x7F)
		{
			// Normal single-byte ascii character.
			*out_advance = 1;
			*out_codepoint = codepoint;
			return true;
		}
		else if ((codepoint & 0xE0) == 0xC0)
		{
			// Double-byte character
			codepoint = ((bytes[offset + 0] & 0b00011111) << 6) +
				(bytes[offset + 1] & 0b00111111);
			*out_advance = 2;
			*out_codepoint = codepoint;
			return true;
		}
		else if ((codepoint & 0xF0) == 0xE0)
		{
			// Triple-byte character
			codepoint = ((bytes[offset + 0] & 0b00001111) << 12) +
				((bytes[offset + 1] & 0b00111111) << 6) +
				(bytes[offset + 2] & 0b00111111);
			*out_advance = 3;
			*out_codepoint = codepoint;
			return true;
		}
		else if ((codepoint & 0xF8) == 0xF0)
		{
			// 4-byte character
			codepoint = ((bytes[offset + 0] & 0b00000111) << 18) +
				((bytes[offset + 1] & 0b00111111) << 12) +
				((bytes[offset + 2] & 0b00111111) << 6) +
				(bytes[offset + 3] & 0b00111111);
			*out_advance = 4;
			*out_codepoint = codepoint;
			return true;
		}
		else
		{
			// NOTE: Not supporting 5 and 6-byte characters; return as invalid UTF-8.
			*out_advance = 0;
			*out_codepoint = 0;
			SHMERROR("utf8_bytes_to_codepoint - Not supporting 5 and 6-byte characters; Invalid UTF-8.");
			return false;
		}
	}

}