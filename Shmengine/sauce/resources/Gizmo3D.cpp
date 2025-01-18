#include "Gizmo3D.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererFrontend.hpp"

static void update_vertices(Gizmo3D* out_gizmo);

bool32 gizmo3D_init(Gizmo3D* out_gizmo)
{
	if (out_gizmo->state >= ResourceState::INITIALIZED)
		return false;

	out_gizmo->state = ResourceState::INITIALIZING;

	out_gizmo->xform = Math::transform_create();

	out_gizmo->unique_id = INVALID_ID;

	GeometryData* geometry = &out_gizmo->geometry;
	geometry->vertex_size = sizeof(Renderer::VertexColor3D);
	// NOTE: 12 * 2 line vertices per gizmo
	geometry->vertex_count = 2 * 12;

	geometry->index_count = 0;

	geometry->center = {};
	geometry->extents = {};

	out_gizmo->is_dirty = false;

	geometry->vertices.init(geometry->vertex_size * geometry->vertex_count, 0);

	update_vertices(out_gizmo);

	out_gizmo->geometry.id = INVALID_ID;

	out_gizmo->state = ResourceState::INITIALIZED;

	return true;
}

bool32 gizmo3D_destroy(Gizmo3D* gizmo)
{
	if (gizmo->state != ResourceState::UNLOADED && !gizmo3D_unload(gizmo))
		return false;

	gizmo->geometry.vertices.free_data();
	gizmo->geometry.indices.free_data();

	gizmo->state = ResourceState::DESTROYED;
	return true;
}

bool32 gizmo3D_load(Gizmo3D* gizmo)
{

	if (gizmo->state != ResourceState::INITIALIZED && gizmo->state != ResourceState::UNLOADED)
		return false;

	bool32 is_reload = gizmo->state == ResourceState::UNLOADED;

	gizmo->state = ResourceState::LOADING;
	gizmo->unique_id = identifier_acquire_new_id(gizmo);

	if (!Renderer::geometry_load(&gizmo->geometry))
	{
		SHMERROR("Failed to load gizmo geometry!");
		return false;
	}

	gizmo->state = ResourceState::LOADED;

	return true;

}

bool32 gizmo3D_unload(Gizmo3D* gizmo)
{

	if (gizmo->state <= ResourceState::INITIALIZED)
		return true;
	else if (gizmo->state != ResourceState::LOADED)
		return false;

	gizmo->state = ResourceState::UNLOADING;

	Renderer::geometry_unload(&gizmo->geometry);

	identifier_release_id(gizmo->unique_id);
	gizmo->unique_id = INVALID_ID;
	gizmo->state = ResourceState::UNLOADED;

	return true;

}

bool32 gizmo3D_update(Gizmo3D* gizmo)
{
	if (!gizmo->is_dirty)
		return true;

	update_vertices(gizmo);
	if (gizmo->state == ResourceState::LOADED)
		return Renderer::geometry_reload(&gizmo->geometry, gizmo->geometry.vertices.size(), 0);

	return true;
}

void gizmo3D_set_parent(Gizmo3D* gizmo, Math::Transform* parent)
{
	gizmo->xform.parent = parent;
}

static void update_vertices(Gizmo3D* gizmo)
{

	SarrayRef<byte, Renderer::VertexColor3D> vertices(&gizmo->geometry.vertices);
	Math::Extents3D extents = gizmo->geometry.extents;

	
}
