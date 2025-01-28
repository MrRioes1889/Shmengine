#include "Box3D.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererFrontend.hpp"

static void update_vertices(Box3D* out_box);

bool32 box3D_init(Math::Vec3f size, Math::Vec4f color, Box3D* out_box)
{
	if (out_box->state >= ResourceState::INITIALIZED)
		return false;

	out_box->state = ResourceState::INITIALIZING;

	out_box->xform = Math::transform_create();
	out_box->color = color;

	out_box->unique_id = INVALID_ID;

	GeometryData* geometry = &out_box->geometry;
	geometry->vertex_size = sizeof(Renderer::VertexColor3D);
	// NOTE: 12 * 2 line vertices per box
	geometry->vertex_count = 2 * 12; 

	geometry->index_count = 0;

	geometry->center = {};
	geometry->extents.min = { -size.x * 0.5f, -size.y * 0.5f, -size.z * 0.5f };
	geometry->extents.max = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };

	out_box->is_dirty = false;

	geometry->vertices.init(geometry->vertex_size * geometry->vertex_count, 0);

	update_vertices(out_box);

	out_box->geometry.id = INVALID_ID;

	out_box->state = ResourceState::INITIALIZED;

	return true;
}

bool32 box3D_destroy(Box3D* box)
{
	if (box->state != ResourceState::UNLOADED && !box3D_unload(box))
		return false;

	box->geometry.vertices.free_data();
	box->geometry.indices.free_data();

	box->state = ResourceState::DESTROYED;
	return true;
}

bool32 box3D_load(Box3D* box)
{

	if (box->state != ResourceState::INITIALIZED && box->state != ResourceState::UNLOADED)
		return false;

	bool32 is_reload = box->state == ResourceState::UNLOADED;

	box->state = ResourceState::LOADING;
	box->unique_id = identifier_acquire_new_id(box);

	if (!Renderer::geometry_load(&box->geometry))
	{
		SHMERROR("Failed to load box geometry!");
		return false;
	}

	box->state = ResourceState::LOADED;

	return true;

}

bool32 box3D_unload(Box3D* box)
{

	if (box->state <= ResourceState::INITIALIZED)
		return true;
	else if (box->state != ResourceState::LOADED)
		return false;

	box->state = ResourceState::UNLOADING;

	Renderer::geometry_unload(&box->geometry);

	identifier_release_id(box->unique_id);
	box->unique_id = INVALID_ID;
	box->state = ResourceState::UNLOADED;

	return true;

}

bool32 box3D_update(Box3D* box)
{
	if (!box->is_dirty)
		return true;

	update_vertices(box);
	if (box->state == ResourceState::LOADED)
		return Renderer::geometry_reload(&box->geometry, box->geometry.vertices.size(), 0);

	box->is_dirty = false;

	return true;
}

void box3D_set_parent(Box3D* box, Math::Transform* parent)
{
	box->xform.parent = parent;
}

void box3D_set_extents(Box3D* box, Math::Extents3D extents)
{
	box->geometry.extents = extents;
	box->is_dirty = true;
}

void box3D_set_color(Box3D* box, Math::Vec4f color)
{
	box->color = color;
	box->is_dirty = true;
}

static void update_vertices(Box3D* box)
{

	SarrayRef<byte, Renderer::VertexColor3D> vertices(&box->geometry.vertices);
	Math::Extents3D extents = box->geometry.extents;

	{
		// top
		vertices[0].position = { extents.min.x, extents.min.y, extents.min.z};
		vertices[1].position = { extents.max.x, extents.min.y, extents.min.z};
		// right
		vertices[2].position = { extents.max.x, extents.min.y, extents.min.z};
		vertices[3].position = { extents.max.x, extents.max.y, extents.min.z};
		// bottom
		vertices[4].position = { extents.max.x, extents.max.y, extents.min.z};
		vertices[5].position = { extents.min.x, extents.max.y, extents.min.z};
		// left
		vertices[6].position = { extents.min.x, extents.min.y, extents.min.z};
		vertices[7].position = { extents.min.x, extents.max.y, extents.min.z};
	}
	// back lines
	{
		// top
		vertices[8].position = { extents.min.x, extents.min.y, extents.max.z};
		vertices[9].position = { extents.max.x, extents.min.y, extents.max.z};
		// right
		vertices[10].position = { extents.max.x, extents.min.y, extents.max.z};
		vertices[11].position = { extents.max.x, extents.max.y, extents.max.z};
		// bottom
		vertices[12].position = { extents.max.x, extents.max.y, extents.max.z};
		vertices[13].position = { extents.min.x, extents.max.y, extents.max.z};
		// left
		vertices[14].position = { extents.min.x, extents.min.y, extents.max.z};
		vertices[15].position = { extents.min.x, extents.max.y, extents.max.z};
	}

	// top connecting lines
	{
		// left
		vertices[16].position = { extents.min.x, extents.min.y, extents.min.z};
		vertices[17].position = { extents.min.x, extents.min.y, extents.max.z};
		// right
		vertices[18].position = { extents.max.x, extents.min.y, extents.min.z};
		vertices[19].position = { extents.max.x, extents.min.y, extents.max.z};
	}
	// bottom connecting lines
	{
		// left
		vertices[20].position = { extents.min.x, extents.max.y, extents.min.z};
		vertices[21].position = { extents.min.x, extents.max.y, extents.max.z};
		// right
		vertices[22].position = { extents.max.x, extents.max.y, extents.min.z};
		vertices[23].position = { extents.max.x, extents.max.y, extents.max.z};
	}

	for (uint32 i = 0; i < vertices.capacity; i++)
		vertices[i].color = box->color;
}
