#pragma once

#include "Defines.hpp"
#include "containers/Buffer.hpp"

struct TruetypeFontResourceData {
	char face[256];
	Buffer binary_data;
};

namespace ResourceSystem
{
	//ResourceLoader truetype_font_resource_loader_create();
	bool32 truetype_font_loader_load(const char* name, TruetypeFontResourceData* out_data);
	void truetype_font_loader_unload(TruetypeFontResourceData* data);
}