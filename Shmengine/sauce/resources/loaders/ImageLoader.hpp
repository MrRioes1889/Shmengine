#pragma once

#include "Defines.hpp"

struct ImageConfig
{
	uint32 channel_count;
	uint32 width;
	uint32 height;
	uint8* pixels;
};

struct ImageResourceParams {
	bool8 flip_y;
};

namespace ResourceSystem
{
	bool32 image_loader_load(const char* name, void* params, ImageConfig* out_config);
	void image_loader_unload(ImageConfig* config);
}

