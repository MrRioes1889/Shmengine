#include "RendererGeometry.hpp"

#include "utility/Math.hpp"

namespace Renderer
{

	void geometry_generate_normals(uint32 vertex_count, Vertex3D* vertices, uint32 index_count, uint32* indices)
	{
        for (uint32 i = 0; i < index_count; i += 3) {
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

	void geometry_generate_tangents(uint32 vertex_count, Vertex3D* vertices, uint32 index_count, uint32* indices)
	{
        for (uint32 i = 0; i < index_count; i += 3) {
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

}