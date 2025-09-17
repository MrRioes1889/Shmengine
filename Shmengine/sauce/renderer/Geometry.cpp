#include "RendererFrontend.hpp"
#include "renderer/Utility.hpp"

#include "optick.h"

namespace Renderer
{
	extern SystemState* system_state;

	bool8 geometry_init(GeometryConfig* config, GeometryData* out_geometry)
	{
		out_geometry->center = config->center;
		out_geometry->extents = config->extents;

		out_geometry->vertex_size = config->vertex_size;
		out_geometry->vertex_count = config->vertex_count;
		out_geometry->index_count = config->index_count;
		out_geometry->vertices.init(out_geometry->vertex_count * out_geometry->vertex_size, 0);
		out_geometry->indices.init(out_geometry->index_count, 0);
		if (config->vertices)
			out_geometry->vertices.copy_memory(config->vertices, out_geometry->vertex_count * out_geometry->vertex_size, 0);
		if (config->indices)
			out_geometry->indices.copy_memory(config->indices, out_geometry->index_count, 0);

		out_geometry->vertex_buffer_alloc_ref = {};
		out_geometry->index_buffer_alloc_ref = {};
		out_geometry->loaded = false;

		return true;
	}

	void geometry_destroy(GeometryData* g)
	{
		if (g->loaded)
			Renderer::geometry_unload(g);

		g->vertices.free_data();
		g->indices.free_data();
		g->vertex_size = 0;
		g->vertex_count = 0;
		g->index_count = 0;
		g->vertex_buffer_alloc_ref = {};
		g->index_buffer_alloc_ref = {};
		g->loaded = false;
	}
}