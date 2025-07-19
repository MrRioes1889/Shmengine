#pragma once

#include "Defines.hpp"
#include "containers/Sarray.hpp"
#include "utility/String.hpp"

struct FontGlyph;
struct FontKerning;
struct FontConfig;

struct BitmapFontResourceData 
{
	String face_name;
	uint16 font_size;
	uint16 line_height;
	int16 baseline;
	uint16 atlas_size_x;
	uint16 atlas_size_y;
	float32 tab_x_advance;
	Sarray<FontGlyph> glyphs;
	Sarray<FontKerning> kernings;
	String texture_name;
};

namespace ResourceSystem
{
	//ResourceLoader bitmap_font_resource_loader_create();
	bool32 bitmap_font_loader_load(const char* name, BitmapFontResourceData* out_resource);
	void bitmap_font_loader_unload(BitmapFontResourceData* resource);

	FontConfig bitmap_font_loader_get_config_from_resource(BitmapFontResourceData* resource);
}