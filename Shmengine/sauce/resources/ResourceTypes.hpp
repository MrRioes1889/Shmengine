#pragma once

#include "containers/Buffer.hpp"

#include "utility/Math.hpp"

struct Texture
{

	static const uint32 max_name_length = 128;

	Buffer buffer = {};

	char name[max_name_length];
	uint32 id;
	uint32 width;
	uint32 height;
	uint32 generation;
	uint32 channel_count;
	bool32 has_transparency;

	SHMINLINE void move(Texture& other)
	{
		Memory::copy_memory(&other, this, sizeof(Texture));

		buffer.move(other.buffer);
	}

};

enum class TextureUse
{
	UNKNOWN = 0,
	MAP_DIFFUSE = 1
};

struct TextureMap
{
	Texture* texture;
	TextureUse use;
};

struct Material
{

	static const uint32 max_name_length = 128;

	uint32 id;
	uint32 generation;
	uint32 internal_id;
	char name[max_name_length];
	Math::Vec4f diffuse_color;
	TextureMap diffuse_map;

};

struct Geometry
{

	static const uint32 max_name_length = 128;

	uint32 id;
	uint32 generation;
	uint32 internal_id;
	char name[max_name_length];
	Material* material;

};

