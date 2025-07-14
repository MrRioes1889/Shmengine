#include "Gizmo3D.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/Geometry.hpp"

static void update_vertices(Gizmo3D* out_gizmo);

static const uint32 rotate_circle_segments = 32;

bool32 gizmo3D_init(Gizmo3D* out_gizmo)
{
	if (out_gizmo->state >= ResourceState::Initialized)
		return false;

	out_gizmo->state = ResourceState::Initializing;

	out_gizmo->xform = Math::transform_create();

	out_gizmo->unique_id = Constants::max_u32;

	GeometryConfig geometry_config = {};
	geometry_config.vertex_size = sizeof(Renderer::VertexColor3D);
	// NOTE: 12 * 2 line vertices per gizmo
	geometry_config.vertex_count = 0;
	geometry_config.index_count = 0;

	geometry_config.center = {};
	geometry_config.extents = {};

	const uint32 max_vertex_count = (12 + (rotate_circle_segments * 2 * 3));
	geometry_config.vertices.init(geometry_config.vertex_size * max_vertex_count, 0);
	Renderer::create_geometry(&geometry_config, &out_gizmo->geometry);

	update_vertices(out_gizmo);
	out_gizmo->is_dirty = false;

	out_gizmo->state = ResourceState::Initialized;

	return true;
}

bool32 gizmo3D_destroy(Gizmo3D* gizmo)
{
	if (gizmo->state != ResourceState::Unloaded && !gizmo3D_unload(gizmo))
		return false;

	Renderer::destroy_geometry(&gizmo->geometry);

	gizmo->state = ResourceState::Destroyed;
	return true;
}

bool32 gizmo3D_load(Gizmo3D* gizmo)
{

	if (gizmo->state != ResourceState::Initialized && gizmo->state != ResourceState::Unloaded)
		return false;

	bool32 is_reload = gizmo->state == ResourceState::Unloaded;

	gizmo->state = ResourceState::Loading;
	gizmo->unique_id = identifier_acquire_new_id(gizmo);

	if (!Renderer::geometry_load(&gizmo->geometry))
	{
		SHMERROR("Failed to load gizmo geometry!");
		return false;
	}

	gizmo->state = ResourceState::Loaded;

	return true;

}

bool32 gizmo3D_unload(Gizmo3D* gizmo)
{

	if (gizmo->state <= ResourceState::Initialized)
		return true;
	else if (gizmo->state != ResourceState::Loaded)
		return false;

	gizmo->state = ResourceState::Unloading;

	Renderer::geometry_unload(&gizmo->geometry);

	identifier_release_id(gizmo->unique_id);
	gizmo->unique_id = Constants::max_u32;
	gizmo->state = ResourceState::Unloaded;

	return true;

}

bool32 gizmo3D_update(Gizmo3D* gizmo)
{
	if (!gizmo->is_dirty)
		return true;

	update_vertices(gizmo);
	if (gizmo->state == ResourceState::Loaded)
		return Renderer::geometry_reload(&gizmo->geometry, gizmo->geometry.vertices.size(), 0);

	gizmo->is_dirty = false;

	return true;
}

void gizmo3D_set_parent(Gizmo3D* gizmo, Math::Transform* parent)
{
	gizmo->xform.parent = parent;
}

void gizmo3D_set_mode(Gizmo3D* gizmo, GizmoMode mode)
{
	gizmo->mode = mode;
	gizmo->is_dirty = true;
}

static void update_vertices(Gizmo3D* gizmo)
{

	SarrayRef<byte, Renderer::VertexColor3D> vertices(&gizmo->geometry.vertices);
	Math::Extents3D extents = gizmo->geometry.extents;

	uint32 vertex_count = 0;
	switch (gizmo->mode)
	{
	case GizmoMode::NONE:
		vertex_count = 6; break;
	case GizmoMode::MOVE:
		vertex_count = 18; break;
	case GizmoMode::SCALE:
		vertex_count = 12; break;
	case GizmoMode::ROTATE:
		vertex_count = 12 + (rotate_circle_segments * 2 * 3); break;
	}

	if (gizmo->geometry.vertices.capacity < vertex_count)
		gizmo->geometry.vertices.resize(vertex_count);
	gizmo->geometry.vertex_count = vertex_count;

	gizmo->geometry.vertices.zero_memory();

	const Math::Vec4f grey = { 0.5f, 0.5f, 0.5f, 1.0f };
	const Math::Vec4f red = { 1.0f, 0.0f, 0.0f, 1.0f };
	const Math::Vec4f green = { 0.0f, 1.0f, 0.0f, 1.0f };
	const Math::Vec4f blue = { 0.0f, 0.0f, 1.0f, 1.0f };

	switch (gizmo->mode)
	{
	case GizmoMode::NONE:
	{	
		// x
		vertices[0].color = grey;
		vertices[1].color = grey;
		vertices[1].position.x = 1.0f;
		// y
		vertices[2].color = grey;
		vertices[3].color = grey;
		vertices[3].position.y = 1.0f;
		// z
		vertices[4].color = grey;
		vertices[5].color = grey;
		vertices[5].position.z = 1.0f;

		break;
	}
	case GizmoMode::MOVE:
	{
		// x
		vertices[0].color = red;
		vertices[0].position.x = 0.2f;
		vertices[1].color = red;
		vertices[1].position.x = 1.0f;
		// y
		vertices[2].color = green;
		vertices[2].position.y = 0.2f;
		vertices[3].color = green;
		vertices[3].position.y = 1.0f;
		// z
		vertices[4].color = blue;
		vertices[4].position.z = 0.2f;
		vertices[5].color = blue;
		vertices[5].position.z = 1.0f;
		// x "box" lines
		vertices[6].color = red;
		vertices[6].position.x = 0.4f;
		vertices[7].color = red;
		vertices[7].position.x = 0.4f;
		vertices[7].position.y = 0.4f;
		vertices[8].color = red;
		vertices[8].position.x = 0.4f;
		vertices[9].color = red;
		vertices[9].position.x = 0.4f;
		vertices[9].position.z = 0.4f;
		// y "box" lines
		vertices[10].color = green;
		vertices[10].position.y = 0.4f;
		vertices[11].color = green;
		vertices[11].position.y = 0.4f;
		vertices[11].position.z = 0.4f;
		vertices[12].color = green;
		vertices[12].position.y = 0.4f;
		vertices[13].color = green;
		vertices[13].position.y = 0.4f;
		vertices[13].position.x = 0.4f;
		// z "box" lines
		vertices[14].color = blue;
		vertices[14].position.z = 0.4f;
		vertices[15].color = blue;
		vertices[15].position.z = 0.4f;
		vertices[15].position.y = 0.4f;
		vertices[16].color = blue;
		vertices[16].position.z = 0.4f;
		vertices[17].color = blue;
		vertices[17].position.z = 0.4f;
		vertices[17].position.x = 0.4f;

		break;
	}
	case GizmoMode::SCALE:
	{
		// x
		vertices[0].color = red;  // First vert is at origin, no pos needed.
		vertices[1].color = red;
		vertices[1].position.x = 1.0f;
		// y
		vertices[2].color = green;  // First vert is at origin, no pos needed.
		vertices[3].color = green;
		vertices[3].position.y = 1.0f;
		// z
		vertices[4].color = blue;  // First vert is at origin, no pos needed.
		vertices[5].color = blue;
		vertices[5].position.z = 1.0f;
		// x/y outer line
		vertices[6].position.x = 0.8f;
		vertices[6].color = red;
		vertices[7].position.y = 0.8f;
		vertices[7].color = green;
		// z/y outer line
		vertices[8].position.z = 0.8f;
		vertices[8].color = blue;
		vertices[9].position.y = 0.8f;
		vertices[9].color = green;
		// x/z outer line
		vertices[10].position.x = 0.8f;
		vertices[10].color = red;
		vertices[11].position.z = 0.8f;
		vertices[11].color = blue;

		break;
	}
	case GizmoMode::ROTATE:
	{
		float32 radius = 1.0f;

		// Start with the center, draw small axes.
		// x
		vertices[0].color = red;  // First vert is at origin, no pos needed.
		vertices[1].color = red;
		vertices[1].position.x = 0.2f;
		// y
		vertices[2].color = green;  // First vert is at origin, no pos needed.
		vertices[3].color = green;
		vertices[3].position.y = 0.2f;
		// z
		vertices[4].color = blue;  // First vert is at origin, no pos needed.
		vertices[5].color = blue;
		vertices[5].position.z = 0.2f;
		// For each axis, generate points in a circle.
		uint32 j = 6;
		// z
		for (uint32 i = 0; i < rotate_circle_segments; ++i, j += 2) {
			// 2 at a time to form a line.
			float32 theta = (float32)i / rotate_circle_segments * Constants::DOUBLE_PI;
			vertices[j].position.x = radius * Math::cos(theta);
			vertices[j].position.y = radius * Math::sin(theta);
			vertices[j].color = blue;
			theta = (float32)((i + 1) % rotate_circle_segments) / rotate_circle_segments * Constants::DOUBLE_PI;
			vertices[j + 1].position.x = radius * Math::cos(theta);
			vertices[j + 1].position.y = radius * Math::sin(theta);
			vertices[j + 1].color = blue;
		}
		// y
		for (uint32 i = 0; i < rotate_circle_segments; ++i, j += 2) {
			// 2 at a time to form a line.
			float32 theta = (float32)i / rotate_circle_segments * Constants::DOUBLE_PI;
			vertices[j].position.x = radius * Math::cos(theta);
			vertices[j].position.z = radius * Math::sin(theta);
			vertices[j].color = green;
			theta = (float32)((i + 1) % rotate_circle_segments) / rotate_circle_segments * Constants::DOUBLE_PI;
			vertices[j + 1].position.x = radius * Math::cos(theta);
			vertices[j + 1].position.z = radius * Math::sin(theta);
			vertices[j + 1].color = green;
		}
		// x
		for (uint32 i = 0; i < rotate_circle_segments; ++i, j += 2) {
			// 2 at a time to form a line.
			float32 theta = (float32)i / rotate_circle_segments * Constants::DOUBLE_PI;
			vertices[j].position.y = radius * Math::cos(theta);
			vertices[j].position.z = radius * Math::sin(theta);
			vertices[j].color = red;
			theta = (float32)((i + 1) % rotate_circle_segments) / rotate_circle_segments * Constants::DOUBLE_PI;
			vertices[j + 1].position.y = radius * Math::cos(theta);
			vertices[j + 1].position.z = radius * Math::sin(theta);
			vertices[j + 1].color = red;
		}

		break;
	}
	}

	
}
