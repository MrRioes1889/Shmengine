#pragma once

#include "renderer/RendererTypes.hpp"
#include "core/Subsystems.hpp"

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
		Sarray<byte> vertices;	
		Sarray<uint32> indices;
		char name[Geometry::max_name_length];
		char material_name[Material::max_name_length];

		Math::Vec3f center;
		Math::Vec3f min_extents;
		Math::Vec3f max_extents;
	};

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
	void system_shutdown(void* state);

	SHMAPI Geometry* acquire_by_id(uint32 id);
	SHMAPI Geometry* acquire_from_config(GeometryConfig* config, bool32 auto_release);
	SHMAPI void release(Geometry* geometry);

	Geometry* get_default_geometry();
	Geometry* get_default_geometry_2d();

}