#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"
#include "core/Subsystems.hpp"
#include "core/Identifier.hpp"

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

	SHMAPI bool8 create_shader(ShaderConfig* config);
	SHMAPI bool8 create_shader_from_resource(const char* resource_name, Renderer::RenderPass* renderpass);

	SHMAPI void destroy_shader(ShaderId shader_id);
	SHMAPI void destroy_shader(const char* shader_name);

	SHMAPI ShaderId get_shader_id(const char* shader_name);

	SHMAPI Shader* get_shader(ShaderId shader_id);
	SHMAPI Shader* get_shader(const char* shader_name);

	SHMAPI void bind_shader(ShaderId shader_id);

	SHMAPI bool8 use_shader(ShaderId shader_id);
	SHMAPI bool8 use_shader(const char* shader_name);

	SHMAPI ShaderUniformId get_uniform_index(ShaderId shader_id, const char* uniform_name);

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