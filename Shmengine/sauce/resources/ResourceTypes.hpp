#pragma once

#include "containers/Buffer.hpp"

#include "utility/Math.hpp"

enum class ResourceType
{
	GENERIC,
	IMAGE,
	MATERIAL,
	STATIC_MESH,
	CUSTOM
};

struct Resource
{
	uint32 loader_id;
	uint32 data_size;
	const char* name;
	char* full_path;	
	void* data;
};

struct ResourceDataImage
{
	uint32 channel_count;
	uint32 width;
	uint32 height;
	uint8* pixels;
};

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

enum class MaterialType
{
	WORLD = 1,
	UI = 2
};

struct Material
{

	static const uint32 max_name_length = 128;

	uint32 id;
	uint32 generation;
	uint32 internal_id;
	MaterialType type;
	char name[max_name_length];
	Math::Vec4f diffuse_color;
	TextureMap diffuse_map;

};

struct ResourceDataMaterial
{
	char name[Material::max_name_length];
	char diffuse_map_name[Texture::max_name_length];
	MaterialType type;
	Math::Vec4f diffuse_color;
	bool32 auto_release;
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

