#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"
#include "core/Subsystems.hpp"

#define UNIFORM_APPLY_OR_FAIL(expr)                  \
    if ((!expr)) {                                      \
        SHMERRORV("Failed to apply uniform: %s", expr); \
        return false;                                 \
    }

namespace ShaderSystem
{

	struct MaterialShaderUniformLocations
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
		uint16 samplers[max_terrain_materials_count * 3];
	};

	struct UIShaderUniformLocations
	{
		uint16 projection;
		uint16 view;
		uint16 diffuse_texture;
		uint16 model;

		uint16 properties;
	};

	struct SkyboxShaderUniformLocations
	{
		uint16 projection;
		uint16 view;
		uint16 cube_map;
	};

	struct SystemConfig
	{
		uint16 max_shader_count;
		uint16 max_uniform_count;
		uint16 max_global_textures;
		uint16 max_instance_textures;
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool32 create_shader(const Renderer::RenderPass* renderpass,const Renderer::ShaderConfig* config);

	SHMAPI uint32 get_id(const char* shader_name);

	SHMAPI Renderer::Shader* get_shader(uint32 shader_id);
	SHMAPI Renderer::Shader* get_shader(const char* shader_name);

	SHMAPI bool32 use_shader(uint32 shader_id);
	SHMAPI bool32 use_shader(const char* shader_name);

	SHMAPI uint16 get_uniform_index(Renderer::Shader* shader, const char* uniform_name);

	SHMAPI bool32 set_uniform(const char* uniform_name, const void* value);
	SHMAPI bool32 set_uniform(uint16 index, const void* value);

	SHMAPI bool32 apply_globals(uint32 shader_id, LightingInfo lighting, uint64 renderer_frame_number, const Math::Mat4* projection, const Math::Mat4* view, const Math::Vec4f* ambient_color, const Math::Vec3f* camera_position, uint32 render_mode);

	SHMAPI bool32 bind_instance(uint32 instance_id);

	uint32 get_material_shader_id();
	uint32 get_terrain_shader_id();
	SHMAPI uint32 get_ui_shader_id();
	SHMAPI uint32 get_skybox_shader_id();
	MaterialShaderUniformLocations get_material_shader_uniform_locations();
	TerrainShaderUniformLocations get_terrain_shader_uniform_locations();
	UIShaderUniformLocations get_ui_shader_uniform_locations();
	SkyboxShaderUniformLocations get_skybox_shader_uniform_locations();

}