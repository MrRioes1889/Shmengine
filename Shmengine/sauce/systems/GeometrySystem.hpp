#pragma once

#include "renderer/RendererTypes.hpp"

namespace GeometrySystem
{

	struct Config
	{
		uint32 max_geometry_count;

		inline static const char* default_name = "default";
	};

	struct GeometryConfig
	{
		uint32 vertex_size;
		uint32 vertex_count;
		void* vertices;
		uint32 index_count;
		uint32* indices;
		char name[Geometry::max_name_length];
		char material_name[Material::max_name_length];
	};

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config);
	void system_shutdown();

	Geometry* acquire_by_id(uint32 id);
	Geometry* acquire_from_config(const GeometryConfig& config, bool32 auto_release);
	void release(Geometry* geometry);

	Geometry* get_default_geometry();
	Geometry* get_default_geometry_2d();

	GeometryConfig generate_plane_config(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, const char* name, const char* material_name);
	GeometryConfig generate_cube_config(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, const char* name, const char* material_name);

}