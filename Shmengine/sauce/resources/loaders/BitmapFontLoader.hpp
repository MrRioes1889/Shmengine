#pragma once

#include "systems/ResourceSystem.hpp"

namespace ResourceSystem
{
	//ResourceLoader bitmap_font_resource_loader_create();
	bool32 bitmap_font_loader_load(const char* name, void* params, BitmapFontResourceData* out_resource);
	void bitmap_font_loader_unload(BitmapFontResourceData* resource);
}