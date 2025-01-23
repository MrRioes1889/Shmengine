#pragma once

#include "utility/MathTypes.hpp"
#include "core/Subsystems.hpp"
#include "containers/Sarray.hpp"

struct Material;

struct GeometryData
{

	uint32 id;
	bool8 loaded;

	char name[max_geometry_name_length];

	Math::Vec3f center;
	Math::Extents3D extents;
	
	uint32 vertex_size;
	uint32 vertex_count;
	uint32 index_count;

	Sarray<byte> vertices;
	Sarray<uint32> indices;

	uint64 vertex_buffer_offset;
	uint64 index_buffer_offset;

};

namespace GeometrySystem
{

	struct SystemConfig
	{
		uint32 max_geometry_count;

		inline static const char* default_name = "default";
	};

	struct GeometryConfig
	{
		uint32 vertex_size;
		uint32 vertex_count;
		uint32 index_count;

		Sarray<byte> vertices;	
		Sarray<uint32> indices;
		char name[max_geometry_name_length];

		Math::Vec3f center;
		Math::Extents3D extents;
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI GeometryData* acquire_by_id(uint32 id);
	SHMAPI GeometryData* acquire_from_config(GeometryConfig* config, bool32 auto_release);
	SHMAPI void release(GeometryData* geometry, GeometryConfig* out_config = 0);
	SHMAPI uint32 get_ref_count(GeometryData* geometry);

	GeometryData* get_default_geometry();
	GeometryData* get_default_geometry_2d();

}