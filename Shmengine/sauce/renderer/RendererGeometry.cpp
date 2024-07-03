#include "RendererGeometry.hpp"

#include "utility/Math.hpp"

namespace Renderer
{

	void geometry_generate_normals(GeometrySystem::GeometryConfig& g_config)
	{
        Vertex3D* vertices = (Renderer::Vertex3D*)g_config.vertices.data;
        uint32* indices = g_config.indices.data;

        for (uint32 i = 0; i < g_config.indices.count; i += 3) {
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

        for (uint32 i = 0; i < g_config.indices.count; i += 3) {
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
            Math::Vec4f t4 = Math::to_vec4(tangent, handedness);
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
            Math::vec_compare(vert_0.color, vert_1.color, FLOAT_EPSILON) &&
            Math::vec_compare(vert_0.tangent, vert_1.tangent, FLOAT_EPSILON));
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

        Darray<Vertex3D> new_vertices(g_config.vertex_count / 3, 0, (AllocationTag)g_config.vertices.allocation_tag);
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
                    reassign_index(g_config.indices.count, indices, n - found_count, o);
                    found = true;
                    found_count++;
                    break;
                }
            }

            if (!found)
            {
                new_vertices.push(old_vertices[o]);
            }
        }

        Memory::free_memory(old_vertices, true, (AllocationTag)g_config.vertices.allocation_tag);

        g_config.vertex_count = new_vertices.count;
        Vertex3D* ptr = new_vertices.transfer_data();
        g_config.vertices.init(new_vertices.size * sizeof(Vertex3D), 0, (AllocationTag)new_vertices.allocation_tag, ptr);
        
        uint32 removed_count = g_config.vertex_count - old_vertex_count;
        SHMDEBUGV("geometry_deduplicate_vertices: removed %u vertices, orig/now %u/%u.", removed_count, g_config.vertex_count, old_vertex_count);

    }

}