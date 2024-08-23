#include "FontSystem.hpp"

#include "resources/ResourceTypes.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "resources/UIText.hpp"
#include "renderer/RendererFrontend.hpp"

namespace FontSystem
{

	struct BitmapFontInternalData
	{
		Resource loaded_resource;
		BitmapFontResourceData* resource_data;
	};

	struct BitmapFontLookup
	{
		uint16 id;
		uint16 reference_count;
		BitmapFontInternalData font_data;
	};

	struct SystemState
	{
		Config config;

		BitmapFontLookup* registered_bitmap_fonts;
		Hashtable<uint16> registered_bitmap_font_table;

	};

	static bool32 setup_font_data(FontAtlas* font);
	static void cleanup_font_data(FontAtlas* font);

	static SystemState* system_state = 0;

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const Config& config)
	{
		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		system_state->config = config;

		uint64 bitmap_font_array_size = sizeof(BitmapFontLookup) * config.max_bitmap_font_config_count;
		system_state->registered_bitmap_fonts = (BitmapFontLookup*)allocator_callback(bitmap_font_array_size);

		uint64 hashtable_data_size = sizeof(uint16) * config.max_bitmap_font_config_count;
		void* hashtable_data = allocator_callback(hashtable_data_size);
		system_state->registered_bitmap_font_table.init(config.max_bitmap_font_config_count, HashtableFlag::EXTERNAL_MEMORY, AllocationTag::UNKNOWN, hashtable_data);

		system_state->registered_bitmap_font_table.floodfill(INVALID_ID16);

		for (uint32 i = 0; i < config.max_bitmap_font_config_count; ++i) 
		{
			system_state->registered_bitmap_fonts[i].id = INVALID_ID16;
			system_state->registered_bitmap_fonts[i].reference_count = 0;
		}

		for (uint32 i = 0; i < system_state->config.default_bitmap_font_count; i++)
		{
			if (!load_bitmap_font(system_state->config.bitmap_font_configs[i]))
				SHMERRORV("Failed to load default bitmap font: %s", system_state->config.bitmap_font_configs[i].name);
		}

		return true;
	}

	void system_shutdown()
	{
		for (uint16 i = 0; i < system_state->config.max_bitmap_font_config_count; i++)
		{
			if (system_state->registered_bitmap_fonts[i].id != INVALID_ID16)
			{
				FontAtlas* font = &system_state->registered_bitmap_fonts[i].font_data.resource_data->data;
				cleanup_font_data(font);
				system_state->registered_bitmap_fonts[i].id = INVALID_ID16;
			}
		}
	}

	bool32 load_system_font(const SystemFontConfig& config)
	{
		return true;
	}

	bool32 load_bitmap_font(const BitmapFontConfig& config)
	{

		uint16 id = system_state->registered_bitmap_font_table.get_value(config.name);

		if (id != INVALID_ID16)
		{
			SHMWARNV("load_bitmap_font - Font named '%s' already exists!", config.name);
			return true;
		}

		for (uint16 i = 0; i < system_state->config.max_bitmap_font_config_count; i++)
		{
			if (system_state->registered_bitmap_fonts[i].id == INVALID_ID16)
			{
				id = i;
				break;
			}
		}

		if (id == INVALID_ID16)
		{
			SHMERROR("load_bitmap_font - No space left to allocate bitmap font!");
			return false;
		}

		BitmapFontLookup* lookup = &system_state->registered_bitmap_fonts[id];
		if (!ResourceSystem::load(config.resource_name, ResourceType::BITMAP_FONT, 0, &lookup->font_data.loaded_resource))
		{
			SHMERROR("load_bitmap_font - Failed to load resource!");
			return false;
		}

		lookup->font_data.resource_data = (BitmapFontResourceData*)lookup->font_data.loaded_resource.data;
		lookup->font_data.resource_data->data.map.texture = TextureSystem::acquire(lookup->font_data.resource_data->pages[0].file, true);

		system_state->registered_bitmap_font_table.set_value(config.name, id);
		lookup->id = id;

		return setup_font_data(&lookup->font_data.resource_data->data);

	}

	bool32 acquire(const char* font_name, uint16 font_size, UIText* text)
	{

		if (text->type == UITextType::BITMAP)
		{
			uint16 id = system_state->registered_bitmap_font_table.get_value(font_name);

			if (id == INVALID_ID16)
			{
				SHMERRORV("acquire - No bitmap font named '%s' found!", font_name);
				return false;
			}

			BitmapFontLookup* lookup = &system_state->registered_bitmap_fonts[id];
			text->font_atlas = &lookup->font_data.resource_data->data;
			lookup->reference_count++;

			return true;
		}
		else if (text->type == UITextType::SYSTEM)
		{
			return false;
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

		SHMERROR("verify_atlas - unrecognized font type!");
		return false;
	}

	static bool32 setup_font_data(FontAtlas* font)
	{

		font->map.filter_magnify = font->map.filter_minify = TextureFilter::LINEAR;
		font->map.repeat_u = font->map.repeat_v = font->map.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
		font->map.use = TextureUse::MAP_DIFFUSE;
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
					font->tab_x_advance = font->size * 4.0f;
				}
			}
		}

		return true;

	}

	static void cleanup_font_data(FontAtlas* font)
	{
		Renderer::texture_map_release_resources(&font->map);

		if (font->type == FontType::BITMAP && font->map.texture)
			TextureSystem::release(font->map.texture->name);
		font->map.texture = 0;
	}

}