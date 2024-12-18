#pragma once

#include "Defines.hpp"
#include "RendererTypes.hpp"
#include "systems/GeometrySystem.hpp"

namespace Renderer
{

	SHMAPI void generate_plane_config(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, const char* name, GeometrySystem::GeometryConfig& out_config);
	SHMAPI void generate_cube_config(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, const char* name, GeometrySystem::GeometryConfig& out_config);
	SHMAPI void generate_terrain_geometry(uint32 tile_count_x, uint32 tile_count_z, float32 tile_scale_x, float32 tile_scale_z, const char* name, GeometryData* out_geometry);

	void geometry_generate_normals(GeometrySystem::GeometryConfig& g_config);

	void geometry_generate_tangents(GeometrySystem::GeometryConfig& g_config);

	void geometry_deduplicate_vertices(GeometrySystem::GeometryConfig& g_config);

}