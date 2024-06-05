#pragma once

#include "Defines.hpp"
#include "renderer/RendererTypes.hpp"

namespace TextureSystem
{
	struct Config
	{
		uint32 max_texture_count;

		inline static const char* default_texture_name = "default";
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	Texture* acquire(const char* name, bool32 auto_release);
	void release(const char* name);

	Texture* get_default_texture();
}



