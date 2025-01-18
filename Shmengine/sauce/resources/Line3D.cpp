#include "Line3D.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererFrontend.hpp"

static void update_vertices(Line3D* out_line);

bool32 line3D_init(Math::Vec3f point0, Math::Vec3f point1, Math::Vec4f color, Line3D* out_line)
{
	if (out_line->state >= ResourceState::INITIALIZED)
		return false;

	out_line->state = ResourceState::INITIALIZING;

	out_line->xform = Math::transform_create();
	out_line->point0 = point0;
	out_line->point1 = point1;
	out_line->color = color;

	out_line->unique_id = INVALID_ID;

	GeometryData* geometry = &out_line->geometry;
	geometry->vertex_size = sizeof(Renderer::VertexColor3D);
	geometry->vertex_count = 2;

	geometry->index_count = 0;

	out_line->is_dirty = false;

	geometry->vertices.init(geometry->vertex_size * geometry->vertex_count, 0);
	
	update_vertices(out_line);

	out_line->geometry.id = INVALID_ID;

	out_line->state = ResourceState::INITIALIZED;

	return true;
}

bool32 line3D_destroy(Line3D* line)
{
	if (line->state != ResourceState::UNLOADED && !line3D_unload(line))
		return false;

	line->geometry.vertices.free_data();
	line->geometry.indices.free_data();

	line->state = ResourceState::DESTROYED;
	return true;
}

bool32 line3D_load(Line3D* line)
{

	if (line->state != ResourceState::INITIALIZED && line->state != ResourceState::UNLOADED)
		return false;

	bool32 is_reload = line->state == ResourceState::UNLOADED;

	line->state = ResourceState::LOADING;
	line->unique_id = identifier_acquire_new_id(line);

	if (!Renderer::geometry_load(&line->geometry))
	{
		SHMERROR("Failed to load line geometry!");
		return false;
	}

	line->state = ResourceState::LOADED;

	return true;

}

bool32 line3D_unload(Line3D* line)
{

	if (line->state <= ResourceState::INITIALIZED)
		return true;
	else if (line->state != ResourceState::LOADED)
		return false;

	line->state = ResourceState::UNLOADING;

	Renderer::geometry_unload(&line->geometry);

	identifier_release_id(line->unique_id);
	line->unique_id = INVALID_ID;
	line->state = ResourceState::UNLOADED;

	return true;

}

bool32 line3D_update(Line3D* line)
{
	if (!line->is_dirty)
		return true;

	update_vertices(line);
	if (line->state == ResourceState::LOADED)
		return Renderer::geometry_reload(&line->geometry, line->geometry.vertices.size(), 0);

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
	Renderer::VertexColor3D& v0 = line->geometry.vertices.get_as<Renderer::VertexColor3D>(0);
	v0.position = line->point0;
	v0.color = line->color;
	Renderer::VertexColor3D& v1 = line->geometry.vertices.get_as<Renderer::VertexColor3D>(1);
	v1.position = line->point1;
	v1.color = line->color;
}
