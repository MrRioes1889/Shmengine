#pragma once

#include "Defines.hpp"
#include "RendererTypes.hpp"

struct TerrainVertex;
struct Terrain;

namespace Renderer
{

	SHMAPI bool8 geometry_init(GeometryConfig* config, GeometryData* g);
	SHMAPI void geometry_destroy(GeometryData* g);

	SHMAPI void geometry_draw(GeometryData* geometry);

	SHMAPI GeometryConfig geometry_get_config_from_resource(GeometryResourceData* resource);

	void generate_mesh_normals(uint32 vertices_count, Vertex3D* vertices, uint32 indices_count, uint32* indices);
	void generate_mesh_tangents(uint32 vertices_count, Vertex3D* vertices, uint32 indices_count, uint32* indices);
	
	void generate_terrain_normals(uint32 vertices_count, TerrainVertex* vertices, uint32 indices_count, uint32* indices);
	void generate_terrain_tangents(uint32 vertices_count, TerrainVertex* vertices, uint32 indices_count, uint32* indices);
}