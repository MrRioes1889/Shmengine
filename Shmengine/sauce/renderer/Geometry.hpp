#pragma once

#include "Defines.hpp"
#include "RendererTypes.hpp"

struct TerrainVertex;
struct Terrain;

namespace Renderer
{

	SHMAPI bool8 create_geometry(GeometryConfig* config, GeometryData* g);
	SHMAPI void destroy_geometry(GeometryData* g);

	SHMAPI void geometry_draw(GeometryData* geometry);

	SHMAPI void generate_plane_config(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, const char* name, GeometryConfig* out_config);
	SHMAPI void generate_cube_config(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, const char* name, GeometryConfig* out_config);

	void generate_mesh_normals(uint32 vertices_count, Vertex3D* vertices, uint32 indices_count, uint32* indices);
	void generate_mesh_tangents(uint32 vertices_count, Vertex3D* vertices, uint32 indices_count, uint32* indices);
	
	void generate_terrain_normals(uint32 vertices_count, TerrainVertex* vertices, uint32 indices_count, uint32* indices);
	void generate_terrain_tangents(uint32 vertices_count, TerrainVertex* vertices, uint32 indices_count, uint32* indices);

	void geometry_deduplicate_vertices(GeometryConfig& g_config);

}