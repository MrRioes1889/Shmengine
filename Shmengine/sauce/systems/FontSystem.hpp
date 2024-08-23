#pragma once

#include "Defines.hpp"
#include "resources/ResourceTypes.hpp"

struct UIText;

namespace FontSystem
{

	struct SystemFontConfig
	{
		const char* name;
		const char* resource_name;
		uint16 default_size;
	};

	struct BitmapFontConfig
	{
		const char* name;
		const char* resource_name;
		uint16 size;
	};

	struct Config
	{
		bool8 auto_release;

		uint8 default_bitmap_font_count;
		uint8 default_system_font_count;

		uint8 system_font_config_count;
		uint8 bitmap_font_config_count;
		uint8 max_system_font_config_count;
		uint8 max_bitmap_font_config_count;

		SystemFontConfig* system_font_configs;
		BitmapFontConfig* bitmap_font_configs;
	};	

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, const Config& config);
	void system_shutdown();

	bool32 load_system_font(const SystemFontConfig& config);
	bool32 load_bitmap_font(const BitmapFontConfig& config);

	bool32 acquire(const char* font_name, uint16 font_size, UIText* text);
	bool32 release(UIText* text);

	bool32 verify_atlas(FontAtlas* font, const char* text);

}