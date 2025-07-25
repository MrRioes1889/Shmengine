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
	enum : uint8
	{
		Vertex = 1,
		Geometry = 1 << 1,
		Fragment = 1 << 2,
		Compute = 1 << 3,
	};
	typedef uint8 Value;
}

enum class ShaderAttributeType : uint8
{
	Float32,
	Float32_2,
	Float32_3,
	Float32_4,
	Mat4,
	Int8,
	UInt8,
	Int16,
	UInt16,
	Int32,
	UInt32,
};

enum class ShaderUniformType : uint8
{
	Float32,
	Float32_2,
	Float32_3,
	Float32_4,
	Int8,
	UInt8,
	Int16,
	UInt16,
	Int32,
	UInt32,
	Mat4,
	Sampler,
	Custom = 255
};

enum class ShaderScope : uint8
{
	Global,
	Instance,
	Local
};

enum class ShaderState : uint8
{
	Uninitialized,
	Initializing,
	Initialized
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

typedef Id16 ShaderUniformId;

struct ShaderUniform
{
	uint32 offset;
	uint16 location;
	ShaderUniformId index;
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
	enum : uint8
	{
		DepthTest = 1 << 0,
		DepthWrite = 1 << 1
	};
	typedef uint8 Value;
}

struct ShaderInstance
{
	RenderBufferAllocationReference alloc_ref;
	uint8 last_update_frame_number;

	Sarray<TextureMap*> instance_texture_maps;
};

typedef Id16 ShaderId;

struct Shader
{
	ShaderId id;
	ShaderFlags::Value shader_flags;
	ShaderState state;
	Renderer::RenderTopologyTypeFlags::Value topologies;
	ShaderScope bound_scope;

	String name;

	uint8 global_uniform_count;
	uint8 global_uniform_sampler_count;
	uint8 instance_uniform_count;
	uint8 instance_uniform_sampler_count;
	uint8 local_uniform_count;

	uint64 required_ubo_alignment;
	uint32 global_ubo_size;
	uint32 global_ubo_stride;
	RenderBufferAllocationReference global_ubo_alloc_ref;

	uint32 ubo_size;
	uint32 ubo_stride;

	uint32 push_constant_size;
	uint32 push_constant_stride;

	Darray<TextureMap*> global_texture_maps;

	uint32 instance_texture_count;
	uint32 bound_instance_id;
	uint64 bound_ubo_offset;

	HashtableOA<ShaderUniformId> uniform_lookup;
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
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId cube_map;
};

struct UIShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId diffuse_texture;
	ShaderUniformId model;

	ShaderUniformId properties;
};

struct MaterialPhongShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId model;
	ShaderUniformId ambient_color;
	ShaderUniformId camera_position;
	ShaderUniformId diffuse_texture;
	ShaderUniformId specular_texture;
	ShaderUniformId normal_texture;
	ShaderUniformId render_mode;
	ShaderUniformId dir_light;
	ShaderUniformId p_lights;
	ShaderUniformId p_lights_count;

	ShaderUniformId properties;
};

struct TerrainShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId model;
	ShaderUniformId ambient_color;
	ShaderUniformId camera_position;
	ShaderUniformId render_mode;
	ShaderUniformId dir_light;
	ShaderUniformId p_lights;
	ShaderUniformId p_lights_count;

	ShaderUniformId properties;
	ShaderUniformId samplers[Constants::max_terrain_materials_count * 3];
};

struct Color3DShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId model;
};

struct CoordinateGridShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId near;
	ShaderUniformId far;
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

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool8 create_shader(const Renderer::RenderPass* renderpass,const ShaderConfig* config);
	SHMAPI bool8 create_shader_from_resource(const char* resource_name, Renderer::RenderPass* renderpass);

	SHMAPI void destroy_shader(ShaderId shader_id);
	SHMAPI void destroy_shader(const char* shader_name);

	SHMAPI ShaderId get_shader_id(const char* shader_name);

	SHMAPI Shader* get_shader(ShaderId shader_id);
	SHMAPI Shader* get_shader(const char* shader_name);

	SHMAPI void bind_shader(ShaderId shader_id);

	SHMAPI bool8 use_shader(ShaderId shader_id);
	SHMAPI bool8 use_shader(const char* shader_name);

	SHMAPI ShaderUniformId get_uniform_index(Shader* shader, const char* uniform_name);

	SHMAPI bool8 set_uniform(const char* uniform_name, const void* value);
	SHMAPI bool8 set_uniform(ShaderUniformId index, const void* value);

	SHMAPI bool8 bind_globals();
	SHMAPI bool8 bind_instance(uint32 instance_id);

	SHMAPI ShaderId get_material_phong_shader_id();
	SHMAPI ShaderId get_terrain_shader_id();
	SHMAPI ShaderId get_ui_shader_id();
	SHMAPI ShaderId get_skybox_shader_id();
	SHMAPI ShaderId get_color3D_shader_id();

}