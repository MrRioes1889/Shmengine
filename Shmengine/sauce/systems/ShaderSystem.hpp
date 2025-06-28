#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"
#include "core/Subsystems.hpp"
#include "core/Identifier.hpp"

#define UNIFORM_APPLY_OR_FAIL(expr)                  \
    if ((!expr)) {                                      \
        SHMERRORV("Failed to apply uniform: %s", expr); \
        return false;                                 \
    }

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

enum class ShaderState
{
	NOT_CREATED,
	UNINITIALIZED,
	INITIALIZED
};

struct ShaderAttributeConfig
{
	char name[Constants::max_shader_attribute_name_length];
	uint32 size;
	ShaderAttributeType type;
};

struct ShaderUniformConfig
{
	char name[Constants::max_shader_uniform_name_length];
	uint32 size;
	uint32 location;
	ShaderUniformType type;
	ShaderScope scope;
};

struct ShaderStageConfig
{
	ShaderStage::Value stage;
	char filename[Constants::max_filename_length];
};

struct ShaderConfig
{
	const char* name;

	Renderer::RenderCullMode cull_mode;
	Renderer::RenderTopologyTypeFlags::Value topologies;

	uint32 stages_count;
	uint32 attributes_count;
	uint32 uniforms_count;

	ShaderAttributeConfig* attributes;
	ShaderUniformConfig* uniforms;
	ShaderStageConfig* stages;

	bool8 depth_test;
	bool8 depth_write;
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

namespace ShaderFlags
{
	enum
	{
		DEPTH_TEST = 1 << 0,
		DEPTH_WRITE = 1 << 1
	};
	typedef uint32 Value;
}

struct Shader;

struct ShaderInstance
{
	uint64 offset;
	uint8 last_update_frame_number;

	Sarray<TextureMap*> instance_texture_maps;
};

struct Shader
{
	uint32 id;
	ShaderFlags::Value shader_flags;
	Renderer::RenderTopologyTypeFlags::Value topologies;

	uint8 global_uniform_count;
	uint8 global_uniform_sampler_count;
	uint8 instance_uniform_count;
	uint8 instance_uniform_sampler_count;
	uint8 local_uniform_count;

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
	uint8 last_update_frame_number;

	uint32 push_constant_range_count;
	Range push_constant_ranges[32];

	uint32 instance_count;
	ShaderInstance instances[Renderer::RendererConfig::shader_max_instances];

	Renderer::RenderBuffer uniform_buffer;

	void* internal_data;

};

struct SkyboxShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 cube_map;
};

struct UIShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 diffuse_texture;
	uint16 model;

	uint16 properties;
};

struct MaterialPhongShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 model;
	uint16 ambient_color;
	uint16 camera_position;
	uint16 diffuse_texture;
	uint16 specular_texture;
	uint16 normal_texture;
	uint16 render_mode;
	uint16 dir_light;
	uint16 p_lights;
	uint16 p_lights_count;

	uint16 properties;
};

struct TerrainShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 model;
	uint16 ambient_color;
	uint16 camera_position;
	uint16 render_mode;
	uint16 dir_light;
	uint16 p_lights;
	uint16 p_lights_count;

	uint16 properties;
	uint16 samplers[Constants::max_terrain_materials_count * 3];
};

struct Color3DShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 model;
};

struct CoordinateGridShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 near;
	uint16 far;
};

namespace ShaderSystem
{

	struct SystemConfig
	{
		uint16 max_shader_count;
		uint16 max_uniform_count;
		uint16 max_global_textures;
		uint16 max_instance_textures;
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool32 create_shader(const Renderer::RenderPass* renderpass,const ShaderConfig* config);
	SHMAPI bool32 create_shader_from_resource(const char* resource_name, Renderer::RenderPass* renderpass);

	SHMAPI uint32 get_id(const char* shader_name);

	SHMAPI Shader* get_shader(uint32 shader_id);
	SHMAPI Shader* get_shader(const char* shader_name);

	SHMAPI void bind_shader(uint32 shader_id);

	SHMAPI bool32 use_shader(uint32 shader_id);
	SHMAPI bool32 use_shader(const char* shader_name);

	SHMAPI uint16 get_uniform_index(Shader* shader, const char* uniform_name);

	SHMAPI bool32 set_uniform(const char* uniform_name, const void* value);
	SHMAPI bool32 set_uniform(uint16 index, const void* value);

	SHMAPI bool32 bind_globals();
	SHMAPI bool32 bind_instance(uint32 instance_id);

	SHMAPI uint32 get_material_phong_shader_id();
	SHMAPI uint32 get_terrain_shader_id();
	SHMAPI uint32 get_ui_shader_id();
	SHMAPI uint32 get_skybox_shader_id();
	SHMAPI uint32 get_color3D_shader_id();

}