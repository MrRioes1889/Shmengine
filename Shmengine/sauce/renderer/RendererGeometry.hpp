#pragma once

#include "Defines.hpp"
#include "RendererTypes.hpp"

namespace Renderer
{

	void geometry_generate_normals(uint32 vertex_count, Vertex3D* vertices, uint32 index_count, uint32* indices);

	void geometry_generate_tangents(uint32 vertex_count, Vertex3D* vertices, uint32 index_count, uint32* indices);

}