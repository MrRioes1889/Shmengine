#pragma once

#include "Defines.hpp"
#include "containers/Sarray.hpp"
#include "containers/Darray.hpp"
#include "systems/MaterialSystem.hpp"
#include "core/Subsystems.hpp"

struct UIText;

struct FontKerning {
	int32 codepoint_0;
	int32 codepoint_1;
	int16 advance;

	SHMINLINE bool8 operator<=(const FontKerning& other) { return codepoint_0 != other.codepoint_0 ? codepoint_0 <= other.codepoint_0 : codepoint_1 <= other.codepoint_1; }
	SHMINLINE bool8 operator>=(const FontKerning& other) { return codepoint_0 != other.codepoint_0 ? codepoint_0 >= other.codepoint_0 : codepoint_1 >= other.codepoint_1; }
};

struct FontGlyph {
	int32 codepoint;
	uint16 x;
	uint16 y;
	uint16 width;
	uint16 height;
	int16 x_offset;
	int16 y_offset;
	int16 x_advance;
	uint8 page_id;

	uint32 kernings_offset;
};

enum class FontType {
	BITMAP,
	TRUETYPE
};

struct FontAtlas {
	FontType type;
	char face[256];
	uint32 font_size;
	uint32 line_height;
	int32 baseline;
	uint32 atlas_size_x;
	uint32 atlas_size_y;
	float32 tab_x_advance;
	TextureMap map;
	Sarray<FontGlyph> glyphs;
	Darray<FontKerning> kernings;
};

namespace FontSystem
{

	struct TruetypeFontConfig
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

	struct SystemConfig
	{
		bool8 auto_release;

		uint8 default_bitmap_font_count;
		uint8 default_truetype_font_count;

		uint8 truetype_font_config_count;
		uint8 bitmap_font_config_count;
		uint8 max_truetype_font_config_count;
		uint8 max_bitmap_font_config_count;

		TruetypeFontConfig* truetype_font_configs;
		BitmapFontConfig* bitmap_font_configs;
	};	

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool32 load_truetype_font(const TruetypeFontConfig& config);
	SHMAPI bool32 load_bitmap_font(const BitmapFontConfig& config);

	bool32 acquire(const char* font_name, uint16 font_size, UIText* text);
	bool32 release(UIText* text);

	bool32 verify_atlas(FontAtlas* font, const char* text);

	uint32 utf8_string_length(const char* str, uint32 char_length, bool32 ignore_control_characters);
	bool32 utf8_bytes_to_codepoint(const char* bytes, uint32 offset, int32* out_codepoint, uint8* out_advance);

}