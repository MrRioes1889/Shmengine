#pragma once

#include "containers/Buffer.hpp"

#include "containers/Hashtable.hpp"
#include "utility/MathTypes.hpp"
#include "utility/String.hpp"
#include "utility/Utility.hpp"

enum class ResourceType
{
	GENERIC,
	IMAGE,
	MATERIAL,
	STATIC_MESH,
	SHADER,
	MESH,
	CUSTOM
};

struct Resource
{
	uint32 loader_id;
	uint32 data_size;
	const char* name;
	String full_path;	
	void* data;
};

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

namespace TextureFlags
{
	enum Value
	{
		HAS_TRANSPARENCY = 1 << 0,
		IS_WRITABLE = 1 << 1,
		IS_WRAPPED = 1 << 2,
		FLIP_Y = 1 << 3
	};
}

enum class TextureType
{
	TYPE_2D,
	TYPE_CUBE
};

struct Texture
{

	static const uint32 max_name_length = 128;

	Buffer internal_data = {};

	char name[max_name_length];
	uint32 id;
	TextureType type;
	uint32 width;
	uint32 height;
	uint32 generation;
	uint32 channel_count;
	uint32 flags;

};

enum class TextureUse
{
	UNKNOWN = 0,
	MAP_DIFFUSE = 1,
	MAP_SPECULAR = 2,
	MAP_NORMAL = 3,
	MAP_CUBEMAP = 3,
};

enum class TextureFilter
{
	NEAREST = 0,
	LINEAR = 1,
};

enum class TextureRepeat
{
	REPEAT = 0,
	MIRRORED_REPEAT = 1,
	CLAMP_TO_EDGE = 2,
	CLAMP_TO_BORDER = 3
};

struct TextureMap
{
	void* internal_data;
	Texture* texture;
	TextureUse use;

	TextureFilter filter_minify;
	TextureFilter filter_magnify;

	TextureRepeat repeat_u;
	TextureRepeat repeat_v;
	TextureRepeat repeat_w;


};

struct Material
{

	static const uint32 max_name_length = 128;

	uint32 id;
	uint32 generation;
	uint32 internal_id;
	uint32 shader_id;
	char name[max_name_length];
	uint32 render_frame_number;
	float32 shininess;	
	Math::Vec4f diffuse_color;
	TextureMap diffuse_map;	
	TextureMap specular_map;	
	TextureMap normal_map;	

	

};

struct MaterialConfig
{
	char name[Material::max_name_length];
	char diffuse_map_name[Texture::max_name_length];
	char specular_map_name[Texture::max_name_length];
	char normal_map_name[Texture::max_name_length];
	String shader_name;
	Math::Vec4f diffuse_color;
	bool32 auto_release;

	float32 shininess;
};

struct Geometry
{

	static const uint32 max_name_length = 128;

	uint32 id;
	uint32 generation;
	uint32 internal_id;
	Math::Vec3f center;
	Math::Extents3D extents;
	char name[max_name_length];
	Material* material;

};

struct Mesh
{
	uint8 generation;
	Darray<Geometry*> geometries;
	Math::Transform transform;
};

struct Skybox
{
	TextureMap cubemap;
	Geometry* g;
	uint64 renderer_frame_number;
	uint32 instance_id;	
};

