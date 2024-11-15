#pragma once

#include "Defines.hpp"
#include "containers/Sarray.hpp"
#include "systems/FontSystem.hpp"

struct BitmapFontPage {
	uint32 id;
	char file[256];
};

struct BitmapFontResourceData {
	FontAtlas atlas;
	Sarray<BitmapFontPage> pages;
};

namespace ResourceSystem
{
	//ResourceLoader bitmap_font_resource_loader_create();
	bool32 bitmap_font_loader_load(const char* name, void* params, BitmapFontResourceData* out_resource);
	void bitmap_font_loader_unload(BitmapFontResourceData* resource);
}