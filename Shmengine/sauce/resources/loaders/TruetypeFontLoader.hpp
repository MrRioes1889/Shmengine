#pragma once

#include "Defines.hpp"
#include "containers/Sarray.hpp"

struct FontGlyph;
struct FontKerning;
struct FontConfig;

struct TruetypeFontResourceData 
{
	uint16 font_size;
	uint16 line_height;
	int16 baseline;
	uint16 atlas_size_x;
	uint16 atlas_size_y;
	float32 tab_x_advance;
	Sarray<FontGlyph> glyphs;
	Sarray<FontKerning> kernings;
	Sarray<uint32> texture_buffer;
};

namespace ResourceSystem
{
	bool8 truetype_font_loader_load(const char* name, uint16 font_size, TruetypeFontResourceData* out_data);
	void truetype_font_loader_unload(TruetypeFontResourceData* data);

	FontConfig truetype_font_loader_get_config_from_resource(TruetypeFontResourceData* resource);
}