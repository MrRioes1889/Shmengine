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
	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI ShaderId acquire_shader_id(const char* name, Shader** out_init_ptr);
	SHMAPI void release_shader_id(const char* name, Shader** out_destroy_ptr);

	SHMAPI ShaderId get_shader_id(const char* name);
	SHMAPI Shader* get_shader(ShaderId id);

	SHMAPI ShaderId get_material_phong_shader_id();
	SHMAPI ShaderId get_terrain_shader_id();
	SHMAPI ShaderId get_ui_shader_id();
	SHMAPI ShaderId get_skybox_shader_id();
	SHMAPI ShaderId get_color3D_shader_id();
}