#pragma once

#include "Defines.hpp"
#include "RendererTypes.hpp"
#include "systems/GeometrySystem.hpp"

namespace Renderer
{

	void geometry_generate_normals(GeometrySystem::GeometryConfig& g_config);

	void geometry_generate_tangents(GeometrySystem::GeometryConfig& g_config);

	void geometry_deduplicate_vertices(GeometrySystem::GeometryConfig& g_config);

}