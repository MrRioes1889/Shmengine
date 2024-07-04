#pragma once

#include "Defines.hpp"
#include "RendererTypes.hpp"
#include "systems/GeometrySystem.hpp"

namespace Renderer
{

	void generate_plane_config(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, const char* name, const char* material_name, GeometrySystem::GeometryConfig& out_config);
	void generate_cube_config(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, const char* name, const char* material_name, GeometrySystem::GeometryConfig& out_config);

	void geometry_generate_normals(GeometrySystem::GeometryConfig& g_config);

	void geometry_generate_tangents(GeometrySystem::GeometryConfig& g_config);

	void geometry_deduplicate_vertices(GeometrySystem::GeometryConfig& g_config);

}