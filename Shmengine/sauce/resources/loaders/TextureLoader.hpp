#pragma once

#include "Defines.hpp"
#include "containers/Buffer.hpp"

struct TextureConfig;

struct TextureResourceData
{
	uint8 channel_count;
	bool8 flipped_y;
	uint32 width;
	uint32 height;
	Buffer pixels;
};

namespace ResourceSystem
{
	bool8 texture_loader_load(const char* resource_name, bool8 flip_y, TextureResourceData* out_resource);
	void texture_loader_unload(TextureResourceData* resource);
}

