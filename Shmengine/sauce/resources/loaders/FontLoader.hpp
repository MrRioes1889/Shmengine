#pragma once

#include "Defines.hpp"
#include "containers/Sarray.hpp"
#include "utility/String.hpp"
#include "systems/FontSystem.hpp"

struct FontResourceData 
{
	FontType font_type;
	uint16 font_size;
	uint16 line_height;
	int16 baseline;
	uint16 atlas_size_x;
	uint16 atlas_size_y;
	Sarray<FontGlyph> glyphs;
	Sarray<FontKerning> kernings;
	String texture_name;
	Sarray<uint32> texture_buffer;
};

namespace ResourceSystem
{
	//ResourceLoader bitmap_font_resource_loader_create();
	bool32 font_loader_load(const char* name, uint16 font_size, FontResourceData* out_resource);
	void font_loader_unload(FontResourceData* resource);

	FontConfig font_loader_get_config_from_resource(FontResourceData* resource);
}