#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"

namespace MaterialSystem
{
	struct SystemConfig
	{
		uint32 max_material_count;

		inline static const char* default_name = "default";
		inline static const char* default_ui_name = "default_ui";
		inline static const char* default_terrain_name = "default_terrain";
	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI bool8 load_from_resource(const char* name, const char* resource_name, bool8 auto_destroy);
	SHMAPI bool8 load_from_config(MaterialConfig* config, bool8 auto_destroy);

	SHMAPI MaterialId acquire_reference(const char* name);
	SHMAPI MaterialId acquire_reference(MaterialId id);
	SHMAPI void release_reference(const char* name);
	SHMAPI void release_reference(MaterialId id);

	SHMAPI Material* get_material(MaterialId id);

	SHMAPI Material* get_default_material();
	SHMAPI Material* get_default_ui_material();

}