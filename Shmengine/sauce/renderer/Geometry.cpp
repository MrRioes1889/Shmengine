#include "RendererFrontend.hpp"
#include "renderer/Utility.hpp"

#include "optick.h"

namespace Renderer
{
	static void _generate_cube_geometry(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, GeometryData* out_geometry);
	static void _generate_plane_geometry(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, GeometryData* out_geometry);

	extern SystemState* system_state;

	bool8 geometry_init(GeometryConfig* config, GeometryData* out_geometry)
	{
		switch (config->type)
		{
		case GeometryConfigType::Default:
		{
			out_geometry->center = config->default_config.center;
			out_geometry->extents = config->default_config.extents;

			out_geometry->vertex_size = config->default_config.vertex_size;
			out_geometry->vertex_count = config->default_config.vertex_count;
			out_geometry->index_count = config->default_config.index_count;
			out_geometry->vertices.init(out_geometry->vertex_count * out_geometry->vertex_size, 0);
			out_geometry->indices.init(out_geometry->index_count, 0);
			if (config->default_config.vertices)
				out_geometry->vertices.copy_memory(config->default_config.vertices, out_geometry->vertex_count * out_geometry->vertex_size, 0);
			if (config->default_config.indices)
				out_geometry->indices.copy_memory(config->default_config.indices, out_geometry->index_count, 0);
			break;
		}
		case GeometryConfigType::Cube:
		{
			_generate_cube_geometry(
				config->cube_config.dim.width,
				config->cube_config.dim.height,
				config->cube_config.dim.depth,
				config->cube_config.tiling.x,
				config->cube_config.tiling.y,
				out_geometry);
			break;
		}
		}

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


	static void _generate_plane_geometry(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, GeometryData* out_geometry)
	{
		if (width == 0) 
		{
			SHMWARN("Width must be nonzero. Defaulting to one.");
			width = 1.0f;
		}
		if (height == 0) 
		{
			SHMWARN("Height must be nonzero. Defaulting to one.");
			height = 1.0f;
		}
		if (x_segment_count == 0) 
		{
			SHMWARN("x_segment_count must be a positive number. Defaulting to one.");
			x_segment_count = 1;
		}
		if (y_segment_count == 0) 
		{
			SHMWARN("y_segment_count must be a positive number. Defaulting to one.");
			y_segment_count = 1;
		}
		if (tile_x == 0) 
		{
			SHMWARN("tile_x must be nonzero. Defaulting to one.");
			tile_x = 1.0f;
		}
		if (tile_y == 0) 
		{
			SHMWARN("tile_y must be nonzero. Defaulting to one.");
			tile_y = 1.0f;
		}

		out_geometry->vertex_size = sizeof(Renderer::Vertex3D);  // 4 verts per segment
		out_geometry->vertex_count = x_segment_count * y_segment_count * 4;  // 4 verts per segment
		out_geometry->vertices.init(out_geometry->vertex_size * out_geometry->vertex_count, 0);
		out_geometry->index_count = x_segment_count * y_segment_count * 6;  // 6 indices per segment
		out_geometry->indices.init(out_geometry->index_count, 0);

		// TODO: This generates extra vertices, but we can always deduplicate them later.
		float32 seg_width = width / x_segment_count;
		float32 seg_height = height / y_segment_count;
		float32 half_width = width * 0.5f;
		float32 half_height = height * 0.5f;
		for (uint32 y = 0; y < y_segment_count; ++y) {
			for (uint32 x = 0; x < x_segment_count; ++x) {
				// Generate vertices
				float32 min_x = (x * seg_width) - half_width;
				float32 min_y = (y * seg_height) - half_height;
				float32 max_x = min_x + seg_width;
				float32 max_y = min_y + seg_height;
				float32 min_uvx = (x / (float32)x_segment_count) * tile_x;
				float32 min_uvy = (y / (float32)y_segment_count) * tile_y;
				float32 max_uvx = ((x + 1) / (float32)x_segment_count) * tile_x;
				float32 max_uvy = ((y + 1) / (float32)y_segment_count) * tile_y;

				uint32 v_offset = ((y * x_segment_count) + x) * 4;
				Renderer::Vertex3D* v0 = &((Renderer::Vertex3D*)&out_geometry->vertices[0])[v_offset + 0];
				Renderer::Vertex3D* v1 = &((Renderer::Vertex3D*)&out_geometry->vertices[0])[v_offset + 1];
				Renderer::Vertex3D* v2 = &((Renderer::Vertex3D*)&out_geometry->vertices[0])[v_offset + 2];
				Renderer::Vertex3D* v3 = &((Renderer::Vertex3D*)&out_geometry->vertices[0])[v_offset + 3];

				v0->position.x = min_x;
				v0->position.y = min_y;
				v0->tex_coords.x = min_uvx;
				v0->tex_coords.y = min_uvy;

				v1->position.x = max_x;
				v1->position.y = max_y;
				v1->tex_coords.x = max_uvx;
				v1->tex_coords.y = max_uvy;

				v2->position.x = min_x;
				v2->position.y = max_y;
				v2->tex_coords.x = min_uvx;
				v2->tex_coords.y = max_uvy;

				v3->position.x = max_x;
				v3->position.y = min_y;
				v3->tex_coords.x = max_uvx;
				v3->tex_coords.y = min_uvy;

				// Generate indices
				uint32 i_offset = ((y * x_segment_count) + x) * 6;
				out_geometry->indices[i_offset + 0] = v_offset + 0;
				out_geometry->indices[i_offset + 1] = v_offset + 1;
				out_geometry->indices[i_offset + 2] = v_offset + 2;
				out_geometry->indices[i_offset + 3] = v_offset + 0;
				out_geometry->indices[i_offset + 4] = v_offset + 3;
				out_geometry->indices[i_offset + 5] = v_offset + 1;
			}
		}
	}

	static void _generate_cube_geometry(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, GeometryData* out_geometry)
	{
		if (width == 0) 
		{
			SHMWARN("Width must be nonzero. Defaulting to one.");
			width = 1.0f;
		}
		if (height == 0) 
		{
			SHMWARN("Height must be nonzero. Defaulting to one.");
			height = 1.0f;
		}
		if (depth == 0) 
		{
			SHMWARN("x_segment_count must be a positive number. Defaulting to one.");
			depth = 1.0f;
		}

		if (tile_x == 0) 
		{
			SHMWARN("tile_x must be nonzero. Defaulting to one.");
			tile_x = 1.0f;
		}
		if (tile_y == 0) 
		{
			SHMWARN("tile_y must be nonzero. Defaulting to one.");
			tile_y = 1.0f;
		}

		out_geometry->vertex_size = sizeof(Renderer::Vertex3D);  // 4 verts per segment
		out_geometry->vertex_count = 4 * 6;  // 4 verts per segment
		out_geometry->vertices.init(out_geometry->vertex_size * out_geometry->vertex_count, 0);
		out_geometry->index_count = 6 * 6;  // 6 indices per segment
		out_geometry->indices.init(out_geometry->index_count, 0);

		// TODO: This generates extra vertices, but we can always deduplicate them later.
		float32 half_width = width * 0.5f;
		float32 half_height = height * 0.5f;
		float32 half_depth = depth * 0.5f;

		float32 min_x = -half_width;
		float32 min_y = -half_height;
		float32 min_z = -half_depth;
		float32 max_x = half_width;
		float32 max_y = half_height;
		float32 max_z = half_depth;
		float32 min_uvx = 0.0f;
		float32 min_uvy = 0.0f;
		float32 max_uvx = tile_x;
		float32 max_uvy = tile_y;

		out_geometry->extents.min.x = min_x;
		out_geometry->extents.min.y = min_y;
		out_geometry->extents.min.z = min_z;
		out_geometry->extents.max.x = max_x;
		out_geometry->extents.max.y = max_y;
		out_geometry->extents.max.z = max_z;

		out_geometry->center = VEC3_ZERO;

		Renderer::Vertex3D* verts = (Renderer::Vertex3D*)out_geometry->vertices.data;

		// Front
		verts[(0 * 4) + 0].position = { min_x, min_y, max_z };
		verts[(0 * 4) + 1].position = { max_x, max_y, max_z };
		verts[(0 * 4) + 2].position = { min_x, max_y, max_z };
		verts[(0 * 4) + 3].position = { max_x, min_y, max_z };
		verts[(0 * 4) + 0].tex_coords = { min_uvx, min_uvy };
		verts[(0 * 4) + 1].tex_coords = { max_uvx, max_uvy };
		verts[(0 * 4) + 2].tex_coords = { min_uvx, max_uvy };
		verts[(0 * 4) + 3].tex_coords = { max_uvx, min_uvy };
		verts[(0 * 4) + 0].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 1].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 2].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 3].normal = { 0.0f, 0.0f, 1.0f };

		//Back
		verts[(1 * 4) + 0].position = { max_x, min_y, min_z };
		verts[(1 * 4) + 1].position = { min_x, max_y, min_z };
		verts[(1 * 4) + 2].position = { max_x, max_y, min_z };
		verts[(1 * 4) + 3].position = { min_x, min_y, min_z };
		verts[(1 * 4) + 0].tex_coords = { min_uvx, min_uvy };
		verts[(1 * 4) + 1].tex_coords = { max_uvx, max_uvy };
		verts[(1 * 4) + 2].tex_coords = { min_uvx, max_uvy };
		verts[(1 * 4) + 3].tex_coords = { max_uvx, min_uvy };
		verts[(1 * 4) + 0].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 1].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 2].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 3].normal = { 0.0f, 0.0f, -1.0f };

		//Left
		verts[(2 * 4) + 0].position = { min_x, min_y, min_z };
		verts[(2 * 4) + 1].position = { min_x, max_y, max_z };
		verts[(2 * 4) + 2].position = { min_x, max_y, min_z };
		verts[(2 * 4) + 3].position = { min_x, min_y, max_z };
		verts[(2 * 4) + 0].tex_coords = { min_uvx, min_uvy };
		verts[(2 * 4) + 1].tex_coords = { max_uvx, max_uvy };
		verts[(2 * 4) + 2].tex_coords = { min_uvx, max_uvy };
		verts[(2 * 4) + 3].tex_coords = { max_uvx, min_uvy };
		verts[(2 * 4) + 0].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 1].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 2].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 3].normal = { -1.0f, 0.0f, 0.0f };

		//Right
		verts[(3 * 4) + 0].position = { max_x, min_y, max_z };
		verts[(3 * 4) + 1].position = { max_x, max_y, min_z };
		verts[(3 * 4) + 2].position = { max_x, max_y, max_z };
		verts[(3 * 4) + 3].position = { max_x, min_y, min_z };
		verts[(3 * 4) + 0].tex_coords = { min_uvx, min_uvy };
		verts[(3 * 4) + 1].tex_coords = { max_uvx, max_uvy };
		verts[(3 * 4) + 2].tex_coords = { min_uvx, max_uvy };
		verts[(3 * 4) + 3].tex_coords = { max_uvx, min_uvy };
		verts[(3 * 4) + 0].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 1].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 2].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 3].normal = { 1.0f, 0.0f, 0.0f };

		//Bottom
		verts[(4 * 4) + 0].position = { max_x, min_y, max_z };
		verts[(4 * 4) + 1].position = { min_x, min_y, min_z };
		verts[(4 * 4) + 2].position = { max_x, min_y, min_z };
		verts[(4 * 4) + 3].position = { min_x, min_y, max_z };
		verts[(4 * 4) + 0].tex_coords = { min_uvx, min_uvy };
		verts[(4 * 4) + 1].tex_coords = { max_uvx, max_uvy };
		verts[(4 * 4) + 2].tex_coords = { min_uvx, max_uvy };
		verts[(4 * 4) + 3].tex_coords = { max_uvx, min_uvy };
		verts[(4 * 4) + 0].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 1].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 2].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 3].normal = { 0.0f, -1.0f, 0.0f };

		//Top
		verts[(5 * 4) + 0].position = { min_x, max_y, max_z };
		verts[(5 * 4) + 1].position = { max_x, max_y, min_z };
		verts[(5 * 4) + 2].position = { min_x, max_y, min_z };
		verts[(5 * 4) + 3].position = { max_x, max_y, max_z };
		verts[(5 * 4) + 0].tex_coords = { min_uvx, min_uvy };
		verts[(5 * 4) + 1].tex_coords = { max_uvx, max_uvy };
		verts[(5 * 4) + 2].tex_coords = { min_uvx, max_uvy };
		verts[(5 * 4) + 3].tex_coords = { max_uvx, min_uvy };
		verts[(5 * 4) + 0].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 1].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 2].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 3].normal = { 0.0f, 1.0f, 0.0f };

		for (uint32 i = 0; i < 6; ++i) 
		{
			uint32 v_offset = i * 4;
			uint32 i_offset = i * 6;
			out_geometry->indices[i_offset + 0] = v_offset + 0;
			out_geometry->indices[i_offset + 1] = v_offset + 1;
			out_geometry->indices[i_offset + 2] = v_offset + 2;
			out_geometry->indices[i_offset + 3] = v_offset + 0;
			out_geometry->indices[i_offset + 4] = v_offset + 3;
			out_geometry->indices[i_offset + 5] = v_offset + 1;
		}

		geometry_generate_tangents<Vertex3D>(out_geometry->vertex_count, (Vertex3D*)out_geometry->vertices.data, out_geometry->index_count, out_geometry->indices.data );
	}

}