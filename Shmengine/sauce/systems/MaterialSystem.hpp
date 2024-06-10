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

	struct MaterialConfig
	{
		char name[Material::max_name_length];
		char diffuse_map_name[Texture::max_name_length];
		Math::Vec4f diffuse_color;
		bool32 auto_release;
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	Material* acquire(const char* name);
	Material* acquire_from_config(const MaterialConfig& config);
	void release(const char* name);

	Material* get_default_material();

}