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
	AllocationTag allocation_tag;
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

namespace TextureFlags
{
	enum Value
	{
		HAS_TRANSPARENCY,
		IS_WRITABLE,
		IS_WRAPPED
	};
}

struct Texture
{

	static const uint32 max_name_length = 128;

	Buffer internal_data = {};

	char name[max_name_length];
	uint32 id;
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
	MAP_NORMAL = 3
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

namespace ShaderStage
{
	enum Value
	{
		VERTEX = 1,
		GEOMETRY = 1 << 1,
		FRAGMENT = 1 << 2,
		COMPUTE = 1 << 3,
	};
}

enum class ShaderAttributeType
{
	FLOAT32,
	FLOAT32_2,
	FLOAT32_3,
	FLOAT32_4,
	MAT4,
	INT8,
	UINT8,
	INT16,
	UINT16,
	INT32,
	UINT32,
};

enum class ShaderUniformType
{
	FLOAT32,
	FLOAT32_2,
	FLOAT32_3,
	FLOAT32_4,
	INT8,
	UINT8,
	INT16,
	UINT16,
	INT32,
	UINT32,
	MAT4,
	SAMPLER,
	CUSTOM = 255
};

enum class ShaderScope
{
	GLOBAL,
	INSTANCE,
	LOCAL
};

struct ShaderAttributeConfig
{
	String name;
	uint8 size;
	ShaderAttributeType type;
};

struct ShaderUniformConfig
{
	String name;
	uint8 size;
	uint32 location;
	ShaderUniformType type;
	ShaderScope scope;
};

struct ShaderConfig
{
	String name;	

	String renderpass_name;
	Darray<ShaderAttributeConfig> attributes;
	Darray<ShaderUniformConfig> uniforms;
	Darray<ShaderStage::Value> stages;
	Darray<String> stage_names;
	Darray<String> stage_filenames;

	bool32 use_instances;
	bool32 use_local;
};

enum class ShaderState
{
	NOT_CREATED,
	UNINITIALIZED,
	INITIALIZED
};

struct ShaderUniform
{
	uint32 offset;
	uint16 location;
	uint16 index;
	uint16 size;
	uint8 set_index;

	ShaderScope scope;
	ShaderUniformType type;
};

struct ShaderAttribute
{
	String name;
	ShaderAttributeType type;
	uint32 size;
};

struct Shader
{
	uint32 id;
	bool32 use_instances;
	bool32 use_locals;	
	uint64 required_ubo_alignment;

	uint32 global_ubo_size;
	uint32 global_ubo_stride;
	uint64 global_ubo_offset;

	uint32 ubo_size;
	uint32 ubo_stride;

	uint32 push_constant_size;
	uint32 push_constant_stride;

	String name;

	Darray<TextureMap*> global_texture_maps;

	ShaderScope bound_scope;

	uint32 bound_instance_id;
	uint64 bound_ubo_offset;

	ShaderState state;
	uint32 instance_texture_count;

	Hashtable<uint16> uniform_lookup;
	Darray<ShaderUniform> uniforms;
	Darray<ShaderAttribute> attributes;

	uint16 attribute_stride;
	uint32 push_constant_range_count;
	Range push_constant_ranges[32];

	void* internal_data;

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
	char name[max_name_length];
	Material* material;

};

struct Mesh
{
	Darray<Geometry*> geometries;
	Math::Transform transform;
};

