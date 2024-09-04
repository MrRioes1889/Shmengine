#pragma once

#include "systems/ResourceSystem.hpp"

namespace ResourceSystem
{
	//ResourceLoader truetype_font_resource_loader_create();
	bool32 truetype_font_loader_load(const char* name, TruetypeFontResourceData* out_data);
	void truetype_font_loader_unload(TruetypeFontResourceData* data);
}