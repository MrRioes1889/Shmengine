#include "FontSystem.hpp"

#include "systems/TextureSystem.hpp"
#include "containers/LinearStorage.hpp"
#include "resources/UIText.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/loaders/FontLoader.hpp"
#include "utility/Sort.hpp"


namespace FontSystem
{
	struct SystemState
	{
		LinearHashedStorage<FontAtlas, FontId, Constants::max_font_name_length> font_storage;
	};

	static bool8 _create_font(FontConfig* config, FontAtlas* out_font);
	static void _destroy_font(FontAtlas* font);

	static SystemState* system_state = 0;

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
	{
		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

		uint64 font_storage_size = system_state->font_storage.get_external_size_requirement(sys_config->max_font_count);
		void* font_storage_data = allocator_callback(allocator, font_storage_size);
		system_state->font_storage.init(sys_config->max_font_count, AllocationTag::Font, font_storage_data);

		return true;
	}

	void system_shutdown(void* state)
	{
		auto iter = system_state->font_storage.get_iterator();
		while (FontAtlas* font = iter.get_next())
		{
			FontId font_id;
			system_state->font_storage.release(font->name, &font_id, &font);
			_destroy_font(font);
			system_state->font_storage.verify_write(font_id);
		}
		system_state->font_storage.destroy();
	}

	bool8 load_font(const char* name, const char* resource_name, uint16 font_size)
	{
		FontId id;
		FontAtlas* font;
		FontResourceData resource;
		FontConfig config;

		StorageReturnCode ret_code = system_state->font_storage.acquire(name, &id, &font);
		switch (ret_code)
		{
		case StorageReturnCode::OutOfMemory:
		{
			SHMERROR("No space to allocate font left!");
			return false;
		}
		case StorageReturnCode::AlreadyExisted:
		{
			SHMWARNV("Font named '%s' already exists!", name);
			return true;
		}
		}

		resource = {};
		goto_if_log(!ResourceSystem::font_loader_load(resource_name, font_size, &resource), fail, "Failed to load font resource");

		config = ResourceSystem::font_loader_get_config_from_resource(&resource);
		config.name = name;

		goto_if_log(!_create_font(&config, font), fail, "Failed to create font object");

		system_state->font_storage.verify_write(id);
		ResourceSystem::font_loader_unload(&resource);
		return true;

	fail:
		system_state->font_storage.revert_write(id);
		return false;
	}

	FontId acquire(const char* font_name)
	{
		return system_state->font_storage.get_id(font_name);
	}
	
    FontAtlas* get_atlas(FontId id, uint16 font_size)
	{	
		FontAtlas* atlas = system_state->font_storage.get_object(id);
		if (atlas->font_size != font_size)
			return 0;

		return atlas;
	}

	static bool8 _create_font(FontConfig* config, FontAtlas* out_font)
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
			out_font->map.texture = TextureSystem::acquire(config->texture_name, TextureType::Plane, true);
		}
		else if (config->texture_buffer && config->texture_buffer_size)
		{
			char font_texture_name[Constants::max_texture_name_length];
			CString::print_s(font_texture_name, sizeof(font_texture_name), "_font_%s_sz%u_", out_font->name, out_font->font_size);
			out_font->map.texture = TextureSystem::acquire_writable(font_texture_name, out_font->atlas_size_x, out_font->atlas_size_y, 4, true);
			if (out_font->map.texture)
				TextureSystem::write_to_texture(out_font->map.texture, 0, config->texture_buffer_size, (uint8*)config->texture_buffer);
		}
		goto_if_log(!out_font->map.texture, fail, "Unable to acquire texture for font atlas.");

		out_font->map.filter_magnify = out_font->map.filter_minify = TextureFilter::LINEAR;
		out_font->map.repeat_u = out_font->map.repeat_v = out_font->map.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
		goto_if_log(!Renderer::texture_map_acquire_resources(&out_font->map), fail, "Unable to acquire resources for font atlas texture map.");

		return true;

	fail:
		_destroy_font(out_font);
		return false;
	}

	static void _destroy_font(FontAtlas* font)
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