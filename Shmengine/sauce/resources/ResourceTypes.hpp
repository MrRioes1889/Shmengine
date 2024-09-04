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
	BITMAP_FONT,
	//TRUETYPE_FONT,
	CUSTOM
};

struct ResourceHeader {
	char signature[4];
	uint8 resource_type;
	uint8 version;
	uint16 reserved;
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

struct FontKerning {
	int32 codepoint_0;
	int32 codepoint_1;
	int16 advance;

	SHMINLINE bool8 operator<=(const FontKerning& other) { return codepoint_0 != other.codepoint_0 ? codepoint_0 <= other.codepoint_0 : codepoint_1 <= other.codepoint_1; }
	SHMINLINE bool8 operator>=(const FontKerning& other) { return codepoint_0 != other.codepoint_0 ? codepoint_0 >= other.codepoint_0 : codepoint_1 >= other.codepoint_1; }
};

struct FontGlyph {
	int32 codepoint;
	uint16 x;
	uint16 y;
	uint16 width;
	uint16 height;
	int16 x_offset;
	int16 y_offset;
	int16 x_advance;
	uint8 page_id;

	uint32 kernings_offset;
};

enum class FontType {
	BITMAP,
	TRUETYPE
};

struct FontAtlas {
	FontType type;
	char face[256];
	uint32 font_size;
	uint32 line_height;
	int32 baseline;
	uint32 atlas_size_x;
	uint32 atlas_size_y;
	float32 tab_x_advance;
	TextureMap map;
	Sarray<FontGlyph> glyphs;
	Darray<FontKerning> kernings;	
};

struct BitmapFontPage {
	uint32 id;
	char file[256];
};

struct BitmapFontResourceData {
	FontAtlas data;
	Sarray<BitmapFontPage> pages;
};

//struct TruetypeFontFace {
//	char file[256];
//};

struct TruetypeFontResourceData {
	char face[256];
	Buffer binary_data;
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

