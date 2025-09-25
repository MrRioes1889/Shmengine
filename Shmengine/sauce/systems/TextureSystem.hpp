#pragma once

#include "Defines.hpp"
#include "core/Subsystems.hpp"
#include "renderer/RendererTypes.hpp"

namespace TextureSystem
{
	struct SystemConfig
	{
		uint32 max_texture_count;

		inline static const char* default_name = "default";
		inline static const char* default_diffuse_name = "default_DIFF";
		inline static const char* default_specular_name = "default_SPEC";
		inline static const char* default_normal_name = "default_NORM";
	};

	bool8 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI Texture* acquire(const char* name, TextureType type, bool8 auto_destroy);
	SHMAPI Texture* acquire_writable(const char* name, uint32 width, uint32 height, uint8 channel_count, bool8 has_transparency);

	SHMAPI bool8 wrap_internal(const char* name, uint32 width, uint32 height, uint8 channel_count, bool8 has_transparency, bool8 is_writable, bool8 register_texture, void* internal_data, uint64 internal_data_size, Texture* out_texture);
	SHMAPI bool8 resize(Texture* t, uint32 width, uint32 height, bool8 regenerate_internal_data);
	SHMAPI void write_to_texture(Texture* t, uint32 offset, uint32 size, const uint8* pixels);

	SHMAPI void release(const char* name);

	SHMAPI Texture* get_default_texture();
	SHMAPI Texture* get_default_diffuse_texture();
	SHMAPI Texture* get_default_specular_texture();
	SHMAPI Texture* get_default_normal_texture();
}



