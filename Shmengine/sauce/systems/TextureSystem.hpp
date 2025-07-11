#pragma once

#include "Defines.hpp"
#include "containers/Buffer.hpp"

#include "core/Subsystems.hpp"

namespace TextureFlags
{
	enum Value
	{
		HAS_TRANSPARENCY = 1 << 0,
		IS_WRITABLE = 1 << 1,
		IS_READABLE = 1 << 2,
		IS_WRAPPED = 1 << 3,
		FLIP_Y = 1 << 4,
		IS_DEPTH = 1 << 5,
	};
}

enum class TextureType
{
	TYPE_2D,
	TYPE_CUBE
};

struct Texture
{

	Buffer internal_data = {};

	char name[Constants::max_texture_name_length];
	uint32 id;
	TextureType type;
	uint32 width;
	uint32 height;
	uint32 generation;
	uint32 channel_count;
	uint32 flags;

};

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

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI Texture* acquire(const char* name, bool32 auto_release);
	SHMAPI Texture* acquire_cube(const char* name, bool32 auto_release);
	SHMAPI Texture* acquire_writable(const char* name, uint32 width, uint32 height, uint32 channel_count, bool32 has_transparency);

	SHMAPI bool32 wrap_internal(const char* name, uint32 width, uint32 height, uint32 channel_count, bool32 has_transparency, bool32 is_writable, bool32 register_texture, void* internal_data, uint64 internal_data_size, Texture* out_texture);
	SHMAPI bool32 set_internal(Texture* t, void* internal_data, uint64 internal_data_size);
	SHMAPI bool32 resize(Texture* t, uint32 width, uint32 height, bool32 regenerate_internal_data);
	SHMAPI void write_to_texture(Texture* t, uint32 offset, uint32 size, const uint8* pixels);

	SHMAPI void release(const char* name);

	SHMAPI Texture* get_default_texture();
	SHMAPI Texture* get_default_diffuse_texture();
	SHMAPI Texture* get_default_specular_texture();
	SHMAPI Texture* get_default_normal_texture();

	SHMAPI void sleep_until_all_textures_loaded();
}



