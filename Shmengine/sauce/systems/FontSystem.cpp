#include "FontSystem.hpp"

#include "systems/TextureSystem.hpp"
#include "resources/UIText.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/loaders/FontLoader.hpp"
#include "utility/Sort.hpp"


namespace FontSystem
{
	struct SystemState
	{
		Sarray<FontAtlas> fonts;
		HashtableRH<FontId, Constants::max_font_name_length> font_lookup;
	};

	static bool8 create_font_data_new(FontConfig* config, FontAtlas* out_font);
	static void destroy_font_data(FontAtlas* font);

	static SystemState* system_state = 0;

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		uint64 font_array_size = system_state->fonts.get_external_size_requirement(sys_config->max_font_count);
		void* font_array_data = allocator_callback(allocator, font_array_size);
		system_state->fonts.init(sys_config->max_font_count, 0, AllocationTag::Font, font_array_data);

		uint64 hashtable_data_size = system_state->font_lookup.get_external_size_requirement(sys_config->max_font_count);
		void* hashtable_data = allocator_callback(allocator, hashtable_data_size);
		system_state->font_lookup.init(sys_config->max_font_count, HashtableOAFlag::ExternalMemory, AllocationTag::Unknown, hashtable_data);

		return true;
	}

	void system_shutdown(void* state)
	{
		for (uint16 i = 0; i < system_state->fonts.capacity; i++)
		{
			if (system_state->fonts[i].type == FontType::None)
				continue;

			FontAtlas* font = &system_state->fonts[i];
			destroy_font_data(font);
		}
	}

	bool8 load_font(const char* name, const char* resource_name, uint16 font_size)
	{
		FontId id = FontId::invalid_value;
		if (system_state->font_lookup.get(name))
		{
			SHMWARNV("load_truetype_font - Font named '%s' already exists!", name);
			return true;
		}

		for (uint16 i = 0; i < system_state->fonts.capacity; i++)
		{
			if (system_state->fonts[i].type == FontType::None)
			{
				id = i;
				break;
			}
		}

		if (!id.is_valid())
		{
			SHMERROR("load_truetype_font - No space left to allocate bitmap font!");
			return false;
		}

		FontResourceData resource = {};
		if (!ResourceSystem::font_loader_load(resource_name, font_size, &resource))
		{
			SHMERROR("load_truetype_font - Failed to load resource!");
			return false;
		}	

		FontConfig config = ResourceSystem::font_loader_get_config_from_resource(&resource);
		config.name = name;
		FontAtlas* font = &system_state->fonts[id];

		if (create_font_data_new(&config, font))
			system_state->font_lookup.set_value(font->name, id);

		ResourceSystem::font_loader_unload(&resource);
		return true;
	}

	FontId acquire(const char* font_name)
	{
		FontId* id = system_state->font_lookup.get(font_name);
		if (!id)
			return FontId::invalid_value;

		return *id;
	}
	
    FontAtlas* get_atlas(FontId id, uint16 font_size)
	{
		if (!id.is_valid() || system_state->fonts[id].type == FontType::None || system_state->fonts[id].font_size != font_size)
			return 0;

		return &system_state->fonts[id];
	}

	static bool8 create_font_data_new(FontConfig* config, FontAtlas* out_font)
	{
		CString::copy(config->name, out_font->name, Constants::max_font_name_length);
		
		out_font->type = config->type;
		out_font->font_size = config->font_size;
		out_font->atlas_size_x = config->atlas_size_x;
		out_font->atlas_size_y = config->atlas_size_y;
		out_font->baseline = config->baseline;
		out_font->line_height = config->line_height;

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
			CString::print_s(font_texture_name, sizeof(font_texture_name), "_font_%s_sz%u_", out_font->name, out_font->font_size);
			out_font->map.texture = TextureSystem::acquire_writable(font_texture_name, out_font->atlas_size_x, out_font->atlas_size_y, 4, true);
			if (out_font->map.texture)
				TextureSystem::write_to_texture(out_font->map.texture, 0, config->texture_buffer_size, (uint8*)config->texture_buffer);
		}
		goto_on_fail_log(out_font->map.texture, failure, "Unable to acquire texture for font atlas.");

		out_font->map.filter_magnify = out_font->map.filter_minify = TextureFilter::LINEAR;
		out_font->map.repeat_u = out_font->map.repeat_v = out_font->map.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
		goto_on_fail_log(Renderer::texture_map_acquire_resources(&out_font->map), failure, "Unable to acquire resources for font atlas texture map.");

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
		font->type = FontType::None;
	}

}