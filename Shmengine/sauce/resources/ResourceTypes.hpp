#pragma once

#include "containers/Buffer.hpp"

#include "utility/Math.hpp"

struct Texture
{
	void move(Texture& other)
	{
		id = other.id;
		width = other.width;
		height = other.height;
		generation = other.generation;
		channel_count = other.channel_count;
		has_transparency = other.has_transparency;

		buffer.move(other.buffer);
	}

	Buffer buffer = {};

	uint32 id;
	uint32 width;
	uint32 height;
	uint32 generation;
	uint32 channel_count;
	bool32 has_transparency;
};
