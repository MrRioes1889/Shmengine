#pragma once

#include "Defines.hpp"
#include "resources/ResourceTypes.hpp"

namespace MaterialSystem
{
	struct Config
	{
		uint32 max_material_count;

		inline static const char* default_name = "default";
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	Material* acquire(const char* name);
	Material* acquire_from_config(const ResourceDataMaterial& config);
	void release(const char* name);

	Material* get_default_material();

}