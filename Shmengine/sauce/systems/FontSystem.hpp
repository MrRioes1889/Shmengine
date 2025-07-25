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

	uint32 kernings_offset;
};

enum class FontType : uint8
{
	None = 0,
	Bitmap,
	Truetype
};

struct FontConfig
{
	const char* name;
	FontType type;
	uint16 font_size;
	uint16 line_height;
	int16 baseline;
	uint16 atlas_size_x;
	uint16 atlas_size_y;
	uint32 glyphs_count;
	uint32 kernings_count;
	FontGlyph* glyphs;
	FontKerning* kernings;
	const char* texture_name;
	uint32 texture_buffer_size;
	uint32* texture_buffer;
};

typedef Id16 FontId;

struct FontAtlas 
{
	char name[Constants::max_font_name_length];
	FontType type;
	uint16 font_size;
	uint16 line_height;
	int16 baseline;
	uint16 atlas_size_x;
	uint16 atlas_size_y;
	TextureMap map;
	Sarray<FontGlyph> glyphs;
	Darray<FontKerning> kernings;
};

namespace FontSystem
{
	struct SystemConfig
	{
		uint8 max_font_count;
	};	

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool8 load_font(const char* name, const char* resource_name, uint16 font_size);

	FontId acquire(const char* font_name);
	FontAtlas* get_atlas(FontId id, uint16 font_size);
}