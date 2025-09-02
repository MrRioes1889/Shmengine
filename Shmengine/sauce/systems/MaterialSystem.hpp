#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"

namespace MaterialSystem
{
	struct SystemConfig
	{
		uint32 max_material_count;

		inline static const char* default_material_name = "default";
		inline static const char* default_ui_material_name = "default_ui";
		inline static const char* default_terrain_name = "default_terrain";
	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI MaterialId acquire_material_id(const char* name, Material** out_create_ptr);
	SHMAPI void release_material_id(const char* name, Material** out_destroy_ptr);

	SHMAPI Material* get_material(MaterialId id);

	SHMAPI Material* get_default_material();
	SHMAPI Material* get_default_ui_material();
	SHMAPI TextureMap* get_default_texture_map();

}