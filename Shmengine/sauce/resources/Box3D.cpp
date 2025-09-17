#include "Box3D.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererFrontend.hpp"

static void update_vertices(Box3D* out_box);

bool8 box3D_init(Math::Vec3f size, Math::Vec4f color, Box3D* out_box)
{
	if (out_box->state >= ResourceState::Initialized)
		return false;

	out_box->state = ResourceState::Initializing;

	out_box->xform = Math::transform_create();
	out_box->color = color;

	out_box->unique_id = Constants::max_u32;

	GeometryConfig geometry_config = {};
	geometry_config.vertex_size = sizeof(Renderer::VertexColor3D);
	// NOTE: 12 * 2 line vertices per box
	geometry_config.vertex_count = 2 * 12; 
	geometry_config.index_count = 0;

	geometry_config.center = {};
	geometry_config.extents.min = { -size.x * 0.5f, -size.y * 0.5f, -size.z * 0.5f };
	geometry_config.extents.max = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
	Renderer::geometry_init(&geometry_config, &out_box->geometry);
	
	update_vertices(out_box);
	out_box->is_dirty = false;

	out_box->unique_id = identifier_acquire_new_id(out_box);

	if (!Renderer::geometry_load(&out_box->geometry))
	{
		SHMERROR("Failed to load box geometry!");
		return false;
	}

	out_box->state = ResourceState::Initialized;

	return true;
}

bool8 box3D_destroy(Box3D* box)
{
	if (box->state != ResourceState::Initialized)
		return false;

	Renderer::geometry_unload(&box->geometry);

	identifier_release_id(box->unique_id);
	box->unique_id = Constants::max_u32;

	Renderer::geometry_destroy(&box->geometry);

	box->state = ResourceState::Destroyed;
	return true;
}

bool8 box3D_update(Box3D* box)
{
	if (!box->is_dirty || box->state != ResourceState::Initialized)
		return true;

	update_vertices(box);
	Renderer::geometry_load(&box->geometry);
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

	SarrayRef<Renderer::VertexColor3D> vertices(&box->geometry.vertices);
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
