#include "RendererGeometry.hpp"

#include "utility/Math.hpp"
#include "resources/Terrain.hpp"
#include "systems/MaterialSystem.hpp"

namespace Renderer
{

	void generate_plane_config(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, const char* name, GeometrySystem::GeometryConfig& out_config)
	{

		if (width == 0) {
			SHMWARN("Width must be nonzero. Defaulting to one.");
			width = 1.0f;
		}
		if (height == 0) {
			SHMWARN("Height must be nonzero. Defaulting to one.");
			height = 1.0f;
		}
		if (x_segment_count == 0) {
			SHMWARN("x_segment_count must be a positive number. Defaulting to one.");
			x_segment_count = 1;
		}
		if (y_segment_count == 0) {
			SHMWARN("y_segment_count must be a positive number. Defaulting to one.");
			y_segment_count = 1;
		}
		if (tile_x == 0) {
			SHMWARN("tile_x must be nonzero. Defaulting to one.");
			tile_x = 1.0f;
		}
		if (tile_y == 0) {
			SHMWARN("tile_y must be nonzero. Defaulting to one.");
			tile_y = 1.0f;
		}

		out_config.vertex_size = sizeof(Renderer::Vertex3D);  // 4 verts per segment
		out_config.vertex_count = x_segment_count * y_segment_count * 4;  // 4 verts per segment
		out_config.vertices.init(out_config.vertex_size * out_config.vertex_count, 0);
		uint32 index_count = x_segment_count * y_segment_count * 6;  // 6 indices per segment
		out_config.indices.init(index_count, 0);

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
				Renderer::Vertex3D* v0 = &((Renderer::Vertex3D*)&out_config.vertices[0])[v_offset + 0];
				Renderer::Vertex3D* v1 = &((Renderer::Vertex3D*)&out_config.vertices[0])[v_offset + 1];
				Renderer::Vertex3D* v2 = &((Renderer::Vertex3D*)&out_config.vertices[0])[v_offset + 2];
				Renderer::Vertex3D* v3 = &((Renderer::Vertex3D*)&out_config.vertices[0])[v_offset + 3];

				v0->position.x = min_x;
				v0->position.y = min_y;
				v0->texcoord.x = min_uvx;
				v0->texcoord.y = min_uvy;

				v1->position.x = max_x;
				v1->position.y = max_y;
				v1->texcoord.x = max_uvx;
				v1->texcoord.y = max_uvy;

				v2->position.x = min_x;
				v2->position.y = max_y;
				v2->texcoord.x = min_uvx;
				v2->texcoord.y = max_uvy;

				v3->position.x = max_x;
				v3->position.y = min_y;
				v3->texcoord.x = max_uvx;
				v3->texcoord.y = min_uvy;

				// Generate indices
				uint32 i_offset = ((y * x_segment_count) + x) * 6;
				out_config.indices[i_offset + 0] = v_offset + 0;
				out_config.indices[i_offset + 1] = v_offset + 1;
				out_config.indices[i_offset + 2] = v_offset + 2;
				out_config.indices[i_offset + 3] = v_offset + 0;
				out_config.indices[i_offset + 4] = v_offset + 3;
				out_config.indices[i_offset + 5] = v_offset + 1;
			}
		}

		if (name && CString::length(name) > 0) {
			CString::copy(name, out_config.name, max_geometry_name_length);
		}
		else {
			CString::copy(GeometrySystem::SystemConfig::default_name, out_config.name, max_geometry_name_length);
		}

	}

	void generate_cube_config(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, const char* name, GeometrySystem::GeometryConfig& out_config)
	{

		if (width == 0) {
			SHMWARN("Width must be nonzero. Defaulting to one.");
			width = 1.0f;
		}
		if (height == 0) {
			SHMWARN("Height must be nonzero. Defaulting to one.");
			height = 1.0f;
		}
		if (depth == 0) {
			SHMWARN("x_segment_count must be a positive number. Defaulting to one.");
			depth = 1.0f;
		}

		if (tile_x == 0) {
			SHMWARN("tile_x must be nonzero. Defaulting to one.");
			tile_x = 1.0f;
		}
		if (tile_y == 0) {
			SHMWARN("tile_y must be nonzero. Defaulting to one.");
			tile_y = 1.0f;
		}

		out_config.vertex_size = sizeof(Renderer::Vertex3D);  // 4 verts per segment
		out_config.vertex_count = 4 * 6;  // 4 verts per segment
		out_config.vertices.init(out_config.vertex_size * out_config.vertex_count, 0);
		uint32 index_count = 6 * 6;  // 6 indices per segment
		out_config.indices.init(index_count, 0);

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

		out_config.extents.min.x = min_x;
		out_config.extents.min.y = min_y;
		out_config.extents.min.z = min_z;
		out_config.extents.max.x = max_x;
		out_config.extents.max.y = max_y;
		out_config.extents.max.z = max_z;

		out_config.center = VEC3_ZERO;

		Renderer::Vertex3D* verts = (Renderer::Vertex3D*)out_config.vertices.data;

		// Front
		verts[(0 * 4) + 0].position = { min_x, min_y, max_z };
		verts[(0 * 4) + 1].position = { max_x, max_y, max_z };
		verts[(0 * 4) + 2].position = { min_x, max_y, max_z };
		verts[(0 * 4) + 3].position = { max_x, min_y, max_z };
		verts[(0 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(0 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(0 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(0 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(0 * 4) + 0].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 1].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 2].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 3].normal = { 0.0f, 0.0f, 1.0f };

		//Back
		verts[(1 * 4) + 0].position = { max_x, min_y, min_z };
		verts[(1 * 4) + 1].position = { min_x, max_y, min_z };
		verts[(1 * 4) + 2].position = { max_x, max_y, min_z };
		verts[(1 * 4) + 3].position = { min_x, min_y, min_z };
		verts[(1 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(1 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(1 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(1 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(1 * 4) + 0].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 1].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 2].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 3].normal = { 0.0f, 0.0f, -1.0f };

		//Left
		verts[(2 * 4) + 0].position = { min_x, min_y, min_z };
		verts[(2 * 4) + 1].position = { min_x, max_y, max_z };
		verts[(2 * 4) + 2].position = { min_x, max_y, min_z };
		verts[(2 * 4) + 3].position = { min_x, min_y, max_z };
		verts[(2 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(2 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(2 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(2 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(2 * 4) + 0].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 1].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 2].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 3].normal = { -1.0f, 0.0f, 0.0f };

		//Right
		verts[(3 * 4) + 0].position = { max_x, min_y, max_z };
		verts[(3 * 4) + 1].position = { max_x, max_y, min_z };
		verts[(3 * 4) + 2].position = { max_x, max_y, max_z };
		verts[(3 * 4) + 3].position = { max_x, min_y, min_z };
		verts[(3 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(3 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(3 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(3 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(3 * 4) + 0].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 1].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 2].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 3].normal = { 1.0f, 0.0f, 0.0f };

		//Bottom
		verts[(4 * 4) + 0].position = { max_x, min_y, max_z };
		verts[(4 * 4) + 1].position = { min_x, min_y, min_z };
		verts[(4 * 4) + 2].position = { max_x, min_y, min_z };
		verts[(4 * 4) + 3].position = { min_x, min_y, max_z };
		verts[(4 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(4 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(4 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(4 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(4 * 4) + 0].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 1].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 2].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 3].normal = { 0.0f, -1.0f, 0.0f };

		//Top
		verts[(5 * 4) + 0].position = { min_x, max_y, max_z };
		verts[(5 * 4) + 1].position = { max_x, max_y, min_z };
		verts[(5 * 4) + 2].position = { min_x, max_y, min_z };
		verts[(5 * 4) + 3].position = { max_x, max_y, max_z };
		verts[(5 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(5 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(5 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(5 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(5 * 4) + 0].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 1].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 2].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 3].normal = { 0.0f, 1.0f, 0.0f };

		for (uint32 i = 0; i < 6; ++i) {
			uint32 v_offset = i * 4;
			uint32 i_offset = i * 6;
			out_config.indices[i_offset + 0] = v_offset + 0;
			out_config.indices[i_offset + 1] = v_offset + 1;
			out_config.indices[i_offset + 2] = v_offset + 2;
			out_config.indices[i_offset + 3] = v_offset + 0;
			out_config.indices[i_offset + 4] = v_offset + 3;
			out_config.indices[i_offset + 5] = v_offset + 1;
		}

		geometry_generate_tangents(out_config);

		if (name && CString::length(name) > 0) {
			CString::copy(name, out_config.name, max_geometry_name_length);
		}
		else {
			CString::copy(GeometrySystem::SystemConfig::default_name, out_config.name, max_geometry_name_length);
		}

	}

	void generate_terrain_geometry(uint32 tile_count_x, uint32 tile_count_z, float32 tile_scale_x, float32 tile_scale_z, const char* name, GeometryData* out_geometry)
	{

		if (!tile_count_x)
		{
			SHMWARN("tile_count_x must be a positive number. Defaulting to one.");
			tile_count_x = 1;
		}
		if (!tile_count_z)
		{
			SHMWARN("tile_count_z must be a positive number. Defaulting to one.");
			tile_count_z = 1;
		}
		if (!tile_scale_x)
		{
			SHMWARN("tile_scale_x must be nonzero. Defaulting to one.");
			tile_scale_x = 1.0f;
		}
		if (!tile_scale_z)
		{
			SHMWARN("tile_scale_z must be nonzero. Defaulting to one.");
			tile_scale_z = 1.0f;
		}

		out_geometry->extents = {};
		out_geometry->center = {};

		out_geometry->vertex_size = sizeof(TerrainVertex);
		//out_geometry->vertex_count = (4 + 2 * (tile_count_x - 1)) + ((4 + 2 * (tile_count_x - 1)) * (tile_count_z - 1) / 2);
		out_geometry->vertex_count = (tile_count_x + 1) * (tile_count_z + 1);
		//out_geometry->vertex_count = 4 * tile_count_x * tile_count_z;
		//out_geometry->vertex_count = tile_count_x * tile_count_z;
		out_geometry->vertices.init(out_geometry->vertex_size * out_geometry->vertex_count, 0);

		uint32 indices_count = out_geometry->vertex_count * 6;
		out_geometry->indices.init(indices_count, 0);

		for (uint32 z = 0, i = 0; z < tile_count_z + 1; z++) 
		{
			for (uint32 x = 0; x < tile_count_x + 1; x++, i++) 
			{
				TerrainVertex* v = &out_geometry->vertices.get_as<TerrainVertex>(i);
				v->position.x = x * tile_scale_x;
				v->position.z = z * tile_scale_z;
				v->position.y = 0.0f;  // <-- this will be modified by a heightmap.

				v->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
				v->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
				v->tex_coords.x = (float32)x;
				v->tex_coords.y = (float32)z;

				v->tangent = {};  // TODO: obviously wrong.
			}
		}

		for (uint32 z = 0, i = 0; z < tile_count_z; z++) 
		{
			for (uint32 x = 0; x < tile_count_x; x++, i += 6) 
			{
				uint32 v0 = (z * (tile_count_x + 1)) + x;
				uint32 v1 = (z * (tile_count_x + 1)) + x + 1;
				uint32 v2 = ((z + 1) * (tile_count_x + 1)) + x;
				uint32 v3 = ((z + 1) * (tile_count_x + 1)) + x + 1;

				out_geometry->indices[i + 0] = v2;
				out_geometry->indices[i + 1] = v1;
				out_geometry->indices[i + 2] = v0;
				out_geometry->indices[i + 3] = v3;
				out_geometry->indices[i + 4] = v1;
				out_geometry->indices[i + 5] = v2;
			}
		}
		
	}

	void geometry_generate_normals(GeometrySystem::GeometryConfig& g_config)
	{
        Vertex3D* vertices = (Renderer::Vertex3D*)g_config.vertices.data;
        uint32* indices = g_config.indices.data;

        for (uint32 i = 0; i < g_config.indices.capacity; i += 3) {
            uint32 i0 = indices[i + 0];
            uint32 i1 = indices[i + 1];
            uint32 i2 = indices[i + 2];

            Math::Vec3f edge1 = vertices[i1].position - vertices[i0].position;
            Math::Vec3f edge2 = vertices[i2].position - vertices[i0].position;

            Math::Vec3f normal = Math::normalized(Math::cross_product(edge1, edge2));

            // NOTE: This just generates a face normal. Smoothing out should be done in a separate pass if desired.
            vertices[i0].normal = normal;
            vertices[i1].normal = normal;
            vertices[i2].normal = normal;
        }
	}

	void geometry_generate_tangents(GeometrySystem::GeometryConfig& g_config)
	{
        Vertex3D* vertices = (Renderer::Vertex3D*)g_config.vertices.data;
        uint32* indices = g_config.indices.data;

        for (uint32 i = 0; i < g_config.indices.capacity; i += 3) {
            uint32 i0 = indices[i + 0];
            uint32 i1 = indices[i + 1];
            uint32 i2 = indices[i + 2];

            Math::Vec3f edge1 = vertices[i1].position - vertices[i0].position;
            Math::Vec3f edge2 = vertices[i2].position - vertices[i0].position;

            float32 deltaU1 = vertices[i1].texcoord.x - vertices[i0].texcoord.x;
            float32 deltaV1 = vertices[i1].texcoord.y - vertices[i0].texcoord.y;

            float32 deltaU2 = vertices[i2].texcoord.x - vertices[i0].texcoord.x;
            float32 deltaV2 = vertices[i2].texcoord.y - vertices[i0].texcoord.y;

            float32 dividend = (deltaU1 * deltaV2 - deltaU2 * deltaV1);
            float32 fc = 1.0f / dividend;

            Math::Vec3f tangent = {
                (fc * (deltaV2 * edge1.x - deltaV1 * edge2.x)),
                (fc * (deltaV2 * edge1.y - deltaV1 * edge2.y)),
                (fc * (deltaV2 * edge1.z - deltaV1 * edge2.z)) };

            tangent = Math::normalized(tangent);

            float32 sx = deltaU1, sy = deltaU2;
            float32 tx = deltaV1, ty = deltaV2;
            float32 handedness = ((tx * sy - ty * sx) < 0.0f) ? -1.0f : 1.0f;
            Math::Vec3f t4 = tangent * handedness;
            vertices[i0].tangent = t4;
            vertices[i1].tangent = t4;
            vertices[i2].tangent = t4;
        }

	}

    static bool8 vertex3d_equal(Vertex3D vert_0, Vertex3D vert_1) {
        return 
            (Math::vec_compare(vert_0.position, vert_1.position, FLOAT_EPSILON) &&
            Math::vec_compare(vert_0.normal, vert_1.normal, FLOAT_EPSILON) &&
            Math::vec_compare(vert_0.texcoord, vert_1.texcoord, FLOAT_EPSILON) &&
            Math::vec_compare(vert_0.color, vert_1.color, FLOAT_EPSILON));
    }

    void reassign_index(uint32 index_count, uint32* indices, uint32 from, uint32 to) {
        for (uint32 i = 0; i < index_count; ++i) {
            if (indices[i] == from) {
                indices[i] = to;
            }
            else if (indices[i] > from) {
                // Pull in all indicies higher than 'from' by 1.
                indices[i]--;
            }
        }
    }

    void geometry_deduplicate_vertices(GeometrySystem::GeometryConfig& g_config)
    {

        Darray<Vertex3D> new_vertices(g_config.vertex_count / 4, 0, (AllocationTag)g_config.vertices.allocation_tag);
        Vertex3D* old_vertices = (Renderer::Vertex3D*)g_config.vertices.transfer_data();        
        uint32 old_vertex_count = g_config.vertex_count;

        uint32* indices = g_config.indices.data;

        uint32 found_count = 0;
        for (uint32 o = 0; o < old_vertex_count; o++)
        {
            bool32 found = false;
            for (uint32 n = 0; n < new_vertices.count; n++)
            {
                if (vertex3d_equal(new_vertices[n], old_vertices[o]))
                {
                    reassign_index(g_config.indices.capacity, indices, o - found_count, n);
                    found = true;
                    found_count++;
                    break;
                }
            }

            if (!found)
            {
                new_vertices.emplace(old_vertices[o]);
            }
        }

        Memory::free_memory(old_vertices);

        g_config.vertex_count = new_vertices.count;
        Vertex3D* ptr = new_vertices.transfer_data();
        g_config.vertices.init(g_config.vertex_count * sizeof(Vertex3D), 0, (AllocationTag)new_vertices.allocation_tag, ptr);
        
        uint32 removed_count = old_vertex_count - g_config.vertex_count;
        SHMDEBUGV("geometry_deduplicate_vertices: removed %u vertices, orig/now %u/%u.", removed_count, old_vertex_count, g_config.vertex_count);

    }

}