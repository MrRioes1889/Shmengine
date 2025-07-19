#pragma once

#include "Defines.hpp"

struct ImageConfig
{
	uint8 channel_count;
	uint32 width;
	uint32 height;
	uint8* pixels;
};

namespace ResourceSystem
{
	bool32 image_loader_load(const char* name, bool8 flip_y, ImageConfig* out_config);
	void image_loader_unload(ImageConfig* config);
}

