#include "Line3D.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererFrontend.hpp"

static void update_vertices(Line3D* out_line);

bool8 line3D_init(Math::Vec3f point0, Math::Vec3f point1, Math::Vec4f color, Line3D* out_line)
{
	if (out_line->state >= ResourceState::Initialized)
		return false;

	out_line->state = ResourceState::Initializing;

	out_line->xform = Math::transform_create();
	out_line->point0 = point0;
	out_line->point1 = point1;
	out_line->color = color;

	out_line->unique_id = Constants::max_u32;

	GeometryConfig geometry_config = {};
	geometry_config.type = GeometryConfigType::Default;
	geometry_config.default_config.vertex_size = sizeof(Renderer::VertexColor3D);
	geometry_config.default_config.vertex_count = 2;

	geometry_config.default_config.index_count = 0;

	Renderer::geometry_init(&geometry_config, &out_line->geometry);
	
	update_vertices(out_line);
	out_line->is_dirty = false;

	out_line->unique_id = identifier_acquire_new_id(out_line);

	if (!Renderer::geometry_load(&out_line->geometry))
	{
		SHMERROR("Failed to load line geometry!");
		return false;
	}

	out_line->state = ResourceState::Initialized;

	return true;
}

bool8 line3D_destroy(Line3D* line)
{
	if (line->state != ResourceState::Initialized)
		return false;

	line->state = ResourceState::Destroying;

	Renderer::geometry_unload(&line->geometry);

	identifier_release_id(line->unique_id);
	line->unique_id = Constants::max_u32;

	Renderer::geometry_destroy(&line->geometry);

	line->state = ResourceState::Destroyed;
	return true;
}

bool8 line3D_update(Line3D* line)
{
	if (!line->is_dirty || line->state != ResourceState::Initialized)
		return true;

	update_vertices(line);
	Renderer::geometry_load(&line->geometry);

	line->is_dirty = false;

	return true;
}

void line3D_set_parent(Line3D* line, Math::Transform* parent)
{
	line->xform.parent = parent;
}

void line3D_set_points(Line3D* line, Math::Vec3f point0, Math::Vec3f point1)
{
	line->point0 = point0;
	line->point1 = point1;
	line->is_dirty = true;
}

void line3D_set_color(Line3D* line, Math::Vec4f color)
{
	line->color = color;
	line->is_dirty = true;
}

static void update_vertices(Line3D* line)
{
	SarrayRef<Renderer::VertexColor3D> vertices(&line->geometry.vertices);
	Renderer::VertexColor3D& v0 = vertices[0];
	v0.position = line->point0;
	v0.color = line->color;
	Renderer::VertexColor3D& v1 = vertices[1];
	v1.position = line->point1;
	v1.color = line->color;
}
