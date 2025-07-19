#include "FontSystem.hpp"

#include "systems/TextureSystem.hpp"
#include "resources/UIText.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/loaders/TruetypeFontLoader.hpp"
#include "resources/loaders/BitmapFontLoader.hpp"
#include "utility/Sort.hpp"


namespace FontSystem
{

	struct BitmapFontLookup
	{
		FontId id;
		uint16 reference_count;
		FontAtlas atlas;
	};

	struct TruetypeFontLookup
	{
		FontId id;
		uint16 reference_count;	
		FontAtlas atlas;	
	};

	struct SystemState
	{
		Sarray<BitmapFontLookup> registered_bitmap_fonts;
		HashtableOA<uint16> registered_bitmap_font_table;

		Sarray<TruetypeFontLookup> registered_truetype_fonts;
		HashtableOA<uint16> registered_truetype_font_table;
	};

	static bool32 create_font_data(FontAtlas* font);
	static bool8 create_font_data_new(FontConfig* config, FontAtlas* out_font);
	static void destroy_font_data(FontAtlas* font);

	static SystemState* system_state = 0;

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		uint64 bitmap_font_array_size = system_state->registered_bitmap_fonts.get_external_size_requirement(sys_config->max_bitmap_font_count);
		void* bitmap_font_array_data = allocator_callback(allocator, bitmap_font_array_size);
		system_state->registered_bitmap_fonts.init(sys_config->max_bitmap_font_count, 0, AllocationTag::Font, bitmap_font_array_data);

		uint64 bitmap_hashtable_data_size = sizeof(uint16) * sys_config->max_bitmap_font_count;
		void* bitmap_hashtable_data = allocator_callback(allocator, bitmap_hashtable_data_size);
		system_state->registered_bitmap_font_table.init(sys_config->max_bitmap_font_count, HashtableOAFlag::ExternalMemory, AllocationTag::Unknown, bitmap_hashtable_data);

		system_state->registered_bitmap_font_table.floodfill(Constants::max_u16);

		for (uint32 i = 0; i < sys_config->max_bitmap_font_count; ++i)
		{
			system_state->registered_bitmap_fonts[i].id = Constants::max_u16;
			system_state->registered_bitmap_fonts[i].reference_count = 0;
		}

		uint64 truetype_font_array_size = system_state->registered_truetype_fonts.get_external_size_requirement(sys_config->max_truetype_font_count);
		void* truetype_font_array_data = allocator_callback(allocator, truetype_font_array_size);
		system_state->registered_truetype_fonts.init(sys_config->max_truetype_font_count, 0, AllocationTag::Font, truetype_font_array_data);

		uint64 truetype_hashtable_data_size = sizeof(uint16) * sys_config->max_truetype_font_count;
		void* truetype_hashtable_data = allocator_callback(allocator, truetype_hashtable_data_size);
		system_state->registered_truetype_font_table.init(sys_config->max_truetype_font_count, HashtableOAFlag::ExternalMemory, AllocationTag::Unknown, truetype_hashtable_data);

		system_state->registered_truetype_font_table.floodfill(Constants::max_u16);

		for (uint32 i = 0; i < sys_config->max_truetype_font_count; ++i)
		{
			system_state->registered_truetype_fonts[i].id = Constants::max_u16;
			system_state->registered_truetype_fonts[i].reference_count = 0;
		}

		return true;
	}

	void system_shutdown(void* state)
	{
		for (uint16 i = 0; i < system_state->registered_bitmap_fonts.capacity; i++)
		{
			if (system_state->registered_bitmap_fonts[i].id == Constants::max_u16)
				continue;

			FontAtlas* font = &system_state->registered_bitmap_fonts[i].atlas;
			destroy_font_data(font);
			system_state->registered_bitmap_fonts[i].id = Constants::max_u16;
		}

		for (uint16 i = 0; i < system_state->registered_truetype_fonts.capacity; i++)
		{
			if (system_state->registered_truetype_fonts[i].id == Constants::max_u16)
				continue;

			FontAtlas* font = &system_state->registered_truetype_fonts[i].atlas;
			destroy_font_data(font);
			system_state->registered_truetype_fonts[i].id = Constants::max_u16;
		}
	}

	bool32 load_truetype_font(const char* name, const char* resource_name, uint16 font_size)
	{
		uint16 id = system_state->registered_truetype_font_table.get_value(name);

		if (id != Constants::max_u16)
		{
			SHMWARNV("load_truetype_font - Font named '%s' already exists!", name);
			return true;
		}

		for (uint16 i = 0; i < system_state->registered_truetype_fonts.capacity; i++)
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

		TruetypeFontResourceData resource = {};
		if (!ResourceSystem::truetype_font_loader_load(resource_name, font_size, &resource))
		{
			SHMERROR("load_truetype_font - Failed to load resource!");
			return false;
		}	

		FontConfig config = ResourceSystem::truetype_font_loader_get_config_from_resource(&resource);
		TruetypeFontLookup* lookup = &system_state->registered_truetype_fonts[id];

		CString::copy(name, lookup->atlas.name, Constants::max_font_name_length);
		system_state->registered_truetype_font_table.set_value(lookup->atlas.name, id);
		lookup->id = id;

		bool8 res = create_font_data_new(&config, &lookup->atlas);
		ResourceSystem::truetype_font_loader_unload(&resource);
		return true;
	}

	bool8 load_bitmap_font(const char* name, const char* resource_name)
	{
		uint16 id = system_state->registered_bitmap_font_table.get_value(name);

		if (id != Constants::max_u16)
		{
			SHMWARNV("load_bitmap_font - Font named '%s' already exists!", name);
			return true;
		}

		for (uint16 i = 0; i < system_state->registered_bitmap_fonts.capacity; i++)
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

		BitmapFontResourceData resource = {};
		if (!ResourceSystem::bitmap_font_loader_load(resource_name, &resource))
		{
			SHMERROR("load_bitmap_font - Failed to load resource!");
			return false;
		}

		FontConfig config = ResourceSystem::bitmap_font_loader_get_config_from_resource(&resource);
		BitmapFontLookup* lookup = &system_state->registered_bitmap_fonts[id];

		CString::copy(name, lookup->atlas.name, Constants::max_font_name_length);
		system_state->registered_bitmap_font_table.set_value(lookup->atlas.name, id);
		lookup->id = id;

		bool8 res = create_font_data_new(&config, &lookup->atlas);
		ResourceSystem::bitmap_font_loader_unload(&resource);
		return res;

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
			text->font_atlas = &lookup->atlas;		

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
			lookup->reference_count++;
			text->font_atlas = &lookup->atlas;

			return true;
		}

		SHMERROR("acquire - unrecognized font type!");
		return false;

	}

	bool32 release(UIText* text)
	{
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

	static bool8 create_font_data_new(FontConfig* config, FontAtlas* out_font)
	{
		out_font->type = config->type;
		out_font->font_size = config->font_size;
		out_font->atlas_size_x = config->atlas_size_x;
		out_font->atlas_size_y = config->atlas_size_y;
		out_font->baseline = config->baseline;
		out_font->line_height = config->line_height;
		out_font->tab_x_advance = config->tab_x_advance;

		out_font->glyphs.init(256, 0, AllocationTag::Font);
		for (uint32 i = 0; i < config->glyphs_count; i++)
		{
			if (config->glyphs[i].codepoint < (int32)out_font->glyphs.capacity)
				out_font->glyphs[config->glyphs[i].codepoint] = config->glyphs[i];
		}

		out_font->kernings.init(config->kernings_count, 0, AllocationTag::Font);
		for (uint32 i = 0; i < config->kernings_count; i++)
		{
			if (config->kernings[i].codepoint_0 < (int32)out_font->glyphs.capacity && config->kernings[i].codepoint_1 < (int32)out_font->glyphs.capacity)
				out_font->kernings.emplace(config->kernings[i]);
		}

        quick_sort(out_font->kernings.data, 0, out_font->kernings.count - 1);
        int32 old_codepoint = -1;
        for (uint32 i = 0; i < out_font->kernings.count; i++)
        {
            int32 codepoint = out_font->kernings[i].codepoint_0;
            if (codepoint != old_codepoint)
                out_font->glyphs[codepoint].kernings_offset = i;
            old_codepoint = codepoint;
        }

		if (config->texture_name)
		{
			out_font->map.texture = TextureSystem::acquire(config->texture_name, true);
		}
		else if (config->texture_buffer && config->texture_buffer_size)
		{
			char font_texture_name[Constants::max_texture_name_length];
			CString::print_s(font_texture_name, sizeof(font_texture_name), "__truetype_fontatlas_%s_sz%u__", out_font->name, out_font->font_size);
			out_font->map.texture = TextureSystem::acquire_writable(font_texture_name, out_font->atlas_size_x, out_font->atlas_size_y, 4, true);
			if (out_font->map.texture)
				TextureSystem::write_to_texture(out_font->map.texture, 0, config->texture_buffer_size, (uint8*)config->texture_buffer);
		}

		if(!out_font->map.texture)
		{
			SHMERROR("Unable to acquire texture for font atlas.");
			goto failure;
		}

		out_font->map.filter_magnify = out_font->map.filter_minify = TextureFilter::LINEAR;
		out_font->map.repeat_u = out_font->map.repeat_v = out_font->map.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
		if (!Renderer::texture_map_acquire_resources(&out_font->map)) {
			SHMERROR("Unable to acquire resources for font atlas texture map.");
			goto failure;
		}

		if (!out_font->tab_x_advance) {
			for (uint32 i = 0; i < out_font->glyphs.capacity; ++i) {
				if (out_font->glyphs[i].codepoint == '\t') {
					out_font->tab_x_advance = out_font->glyphs[i].x_advance;
					break;
				}
			}

			if (!out_font->tab_x_advance) {
				for (uint32 i = 0; i < out_font->glyphs.capacity; ++i) {
					if (out_font->glyphs[i].codepoint == ' ') {
						out_font->tab_x_advance = out_font->glyphs[i].x_advance * 4.0f;
						break;
					}
				}
				if (!out_font->tab_x_advance) {
					out_font->tab_x_advance = out_font->font_size * 4.0f;
				}
			}
		}

		return true;

	failure:
		destroy_font_data(out_font);
		return false;
	}

	static void destroy_font_data(FontAtlas* font)
	{
		Renderer::texture_map_release_resources(&font->map);

		if (font->type == FontType::Bitmap && font->map.texture)
			TextureSystem::release(font->map.texture->name);
		font->map.texture = 0;
		font->glyphs.free_data();
		font->kernings.free_data();
	}

	uint32 utf8_string_length(const char* str, uint32 char_length, bool32 ignore_control_characters)
	{
		uint32 length = 0;
		uint32 char_i = 0;
		for (char_i = 0; str[char_i] && char_i < char_length; char_i++)
		{
			uint32 c = (uint32)str[char_i];
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
				char_i += 1;
			}
			else if ((c & 0xF0) == 0xE0) 
			{
				// Triple-byte character, increment twice more.
				char_i += 2;
			}
			else if ((c & 0xF8) == 0xF0) 
			{
				// 4-byte character, increment thrice more.
				char_i += 3;
			}
			else 
			{
				SHMERROR("Not supporting 5 and 6-byte characters; Invalid UTF-8.");
				return 0;
			}

			length++;
		}

		if (char_i > char_length)
		{
			SHMERROR("Char buffer does not fit expected UTF-8 format; Invalid UTF-8.");
			return 0;
		}

		return length;
	}

	bool32 utf8_bytes_to_codepoint(const char* bytes, uint32 offset, int32* out_codepoint, uint8* out_advance) 
	{
		int32 codepoint = (int32)bytes[offset];
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