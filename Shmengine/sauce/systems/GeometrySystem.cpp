#include "GeometrySystem.hpp"

#include "core/Logging.hpp"
#include "utility/String.hpp"
#include "core/Memory.hpp"
#include "systems/MaterialSystem.hpp"
#include "renderer/RendererFrontend.hpp"

namespace GeometrySystem
{

	struct GeometryReference
	{
		uint32 reference_count;
		Geometry geometry;
		bool32 auto_release;
	};

	struct SystemState
	{
		GeometrySystem::Config config;

		Geometry default_geometry;

		GeometryReference* registered_geometries;
	};

	static SystemState* system_state = 0;

	bool32 create_geometry(GeometryConfig config, Geometry* g);
	void destroy_geometry(Geometry* g);
	bool32 create_default_geometry();
	

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state, Config config)
    {
        out_state = allocator_callback(sizeof(SystemState));
        system_state = (SystemState*)out_state;

        system_state->config = config;

        uint64 geometry_array_size = sizeof(GeometryReference) * config.max_geometry_count;
        system_state->registered_geometries = (GeometryReference*)allocator_callback(geometry_array_size);

        // Invalidate all geometries in the array.
        uint32 count = system_state->config.max_geometry_count;
        for (uint32 i = 0; i < count; ++i) {
            system_state->registered_geometries[i].geometry.id = INVALID_OBJECT_ID;
            system_state->registered_geometries[i].geometry.generation = INVALID_OBJECT_ID;
            system_state->registered_geometries[i].geometry.internal_id = INVALID_OBJECT_ID;
        }

        if (!create_default_geometry()) {
            SHMFATAL("Failed to create default geometry. Application cannot continue.");
            return false;
        }

        return true;

	}

	void system_shutdown()
	{
		system_state = 0;
	}

	Geometry* acquire_by_id(uint32 id)
	{
		if (id != INVALID_OBJECT_ID && system_state->registered_geometries[id].geometry.id != INVALID_OBJECT_ID)
		{
			system_state->registered_geometries[id].reference_count++;
			return &system_state->registered_geometries[id].geometry;
		}

		SHMERROR("acquire_by_id - Cannot load invalid geometry id!");
		return 0;
	}

	Geometry* acquire_from_config(const GeometryConfig& config, bool32 auto_release)
	{

		Geometry* g = 0;
		for (uint32 i = 0; i < system_state->config.max_geometry_count; i++)
		{
			if (system_state->registered_geometries[i].geometry.id == INVALID_OBJECT_ID)
			{
				system_state->registered_geometries[i].reference_count = 1;
				system_state->registered_geometries[i].auto_release = auto_release;
				g = &system_state->registered_geometries[i].geometry;
			}
		}

		if (!g)
		{
			SHMERROR("Could not obtain a free slot for geometry.");
			return 0;
		}

		if (!create_geometry(config, g))
		{
			SHMERROR("Failed to create geometry from config.");
			return 0;
		}

		return g;

	}

	void release(Geometry* g)
	{
		if (g->id != INVALID_OBJECT_ID)
		{
			GeometryReference& ref = system_state->registered_geometries[g->id];

			uint32 id = g->id;
			if (ref.geometry.id == id)
			{
				if (ref.reference_count > 0)
					ref.reference_count--;

				if (ref.reference_count < 1 && ref.auto_release)
				{
					destroy_geometry(g);
					ref.reference_count = 0;
					ref.auto_release = false;
				}
			}
			else
			{
				SHMFATAL("Geometry id mismatch. Something went horribly wrong...");			
			}

		}

	}

	Geometry* get_default_geometry()
	{
		return &system_state->default_geometry;
	}

	bool32 create_geometry(GeometryConfig config, Geometry* g)
	{

		if (!Renderer::create_geometry(g, config.vertex_count, config.vertices, config.index_count, config.indices))
		{
			system_state->registered_geometries[g->id].reference_count = 0;
			system_state->registered_geometries[g->id].auto_release = false;
			g->id = INVALID_OBJECT_ID;
			g->generation = INVALID_OBJECT_ID;
			g->internal_id = INVALID_OBJECT_ID;

			return false;
		}

		if (*config.material_name)
		{
			g->material = MaterialSystem::acquire(config.material_name);
			if (!g->material)
				g->material = MaterialSystem::get_default_material();
		}

		return true;

	}

	void destroy_geometry(Geometry* g)
	{
		Renderer::destroy_geometry(g);
		g->internal_id = INVALID_OBJECT_ID;
		g->generation = INVALID_OBJECT_ID;
		g->id = INVALID_OBJECT_ID;

		String::empty(g->name);

		if (g->material)
		{
			MaterialSystem::release(g->material->name);
			g->material = 0;
		}
	}

	bool32 create_default_geometry()
	{

		Renderer::Vertex3D verts[4] = {};

		const float32 f = 10.0f;

		verts[0].position.x = -0.5 * f;  // 0    3
		verts[0].position.y = -0.5 * f;  //
		verts[0].tex_coordinates.x = 0.0f;      //
		verts[0].tex_coordinates.y = 0.0f;      // 2    1

		verts[1].position.y = 0.5 * f;
		verts[1].position.x = 0.5 * f;
		verts[1].tex_coordinates.x = 1.0f;
		verts[1].tex_coordinates.y = 1.0f;

		verts[2].position.x = -0.5 * f;
		verts[2].position.y = 0.5 * f;
		verts[2].tex_coordinates.x = 0.0f;
		verts[2].tex_coordinates.y = 1.0f;

		verts[3].position.x = 0.5 * f;
		verts[3].position.y = -0.5 * f;
		verts[3].tex_coordinates.x = 1.0f;
		verts[3].tex_coordinates.y = 0.0f;

		uint32 indices[6] = { 0, 1, 2, 0, 3, 1 };

		// Send the geometry off to the renderer to be uploaded to the GPU.
		if (!Renderer::create_geometry(&system_state->default_geometry, 4, verts, 6, indices)) {
			SHMFATAL("Failed to create default geometry. Application cannot continue.");
			return false;
		}

		// Acquire the default material.
		system_state->default_geometry.material = MaterialSystem::get_default_material();

		return true;

	}

	GeometryConfig generate_plane_config(float32 width, float32 height, uint32 x_segment_count, uint32 y_segment_count, float32 tile_x, float32 tile_y, const char* name, const char* material_name)
	{

		if (width == 0) {
			SHMWARN("Width must be nonzero. Defaulting to one.");
			width = 1.0f;
		}
		if (height == 0) {
			SHMWARN("Height must be nonzero. Defaulting to one.");
			height = 1.0f;
		}
		if (x_segment_count < 1) {
			SHMWARN("x_segment_count must be a positive number. Defaulting to one.");
			x_segment_count = 1;
		}
		if (y_segment_count < 1) {
			SHMWARN("y_segment_count must be a positive number. Defaulting to one.");
			y_segment_count = 1;
		}

		if (tile_x == 0) {
			SHMWARN("tile_x must be nonzero. Defaulting to one.");
			tile_x = 1.0f;
		}
		if (tile_y == 0) {
			SHMWARN("tile_y must be nonzero. Defaulting to one.");
			tile_y = 1.0f;
		}

		GeometryConfig config;
		config.vertex_count = x_segment_count * y_segment_count * 4;  // 4 verts per segment
		config.vertices = (Renderer::Vertex3D*)Memory::allocate(sizeof(Renderer::Vertex3D) * config.vertex_count, true, AllocationTag::MAIN);
		config.index_count = x_segment_count * y_segment_count * 6;  // 6 indices per segment
		config.indices = (uint32*)Memory::allocate(sizeof(uint32) * config.index_count, true, AllocationTag::MAIN);

		// TODO: This generates extra vertices, but we can always deduplicate them later.
		float32 seg_width = width / x_segment_count;
		float32 seg_height = height / y_segment_count;
		float32 half_width = width * 0.5f;
		float32 half_height = height * 0.5f;
		for (uint32 y = 0; y < y_segment_count; ++y) {
			for (uint32 x = 0; x < x_segment_count; ++x) {
				// Generate vertices
				float32 min_x = (x * seg_width) - half_width;
				float32 min_y = (y * seg_height) - half_height;
				float32 max_x = min_x + seg_width;
				float32 max_y = min_y + seg_height;
				float32 min_uvx = (x / (float32)x_segment_count) * tile_x;
				float32 min_uvy = (y / (float32)y_segment_count) * tile_y;
				float32 max_uvx = ((x + 1) / (float32)x_segment_count) * tile_x;
				float32 max_uvy = ((y + 1) / (float32)y_segment_count) * tile_y;

				uint32 v_offset = ((y * x_segment_count) + x) * 4;
				Renderer::Vertex3D* v0 = &config.vertices[v_offset + 0];
				Renderer::Vertex3D* v1 = &config.vertices[v_offset + 1];
				Renderer::Vertex3D* v2 = &config.vertices[v_offset + 2];
				Renderer::Vertex3D* v3 = &config.vertices[v_offset + 3];

				v0->position.x = min_x;
				v0->position.y = min_y;
				v0->tex_coordinates.x = min_uvx;
				v0->tex_coordinates.y = min_uvy;

				v1->position.x = max_x;
				v1->position.y = max_y;
				v1->tex_coordinates.x = max_uvx;
				v1->tex_coordinates.y = max_uvy;

				v2->position.x = min_x;
				v2->position.y = max_y;
				v2->tex_coordinates.x = min_uvx;
				v2->tex_coordinates.y = max_uvy;

				v3->position.x = max_x;
				v3->position.y = min_y;
				v3->tex_coordinates.x = max_uvx;
				v3->tex_coordinates.y = min_uvy;

				// Generate indices
				uint32 i_offset = ((y * x_segment_count) + x) * 6;
				config.indices[i_offset + 0] = v_offset + 0;
				config.indices[i_offset + 1] = v_offset + 1;
				config.indices[i_offset + 2] = v_offset + 2;
				config.indices[i_offset + 3] = v_offset + 0;
				config.indices[i_offset + 4] = v_offset + 3;
				config.indices[i_offset + 5] = v_offset + 1;
			}
		}

		if (name && String::length(name) > 0) {
			String::copy(Geometry::max_name_length, config.name, name);
		}
		else {
			String::copy(Geometry::max_name_length, config.name, Config::default_name);
		}

		if (material_name && String::length(material_name) > 0) {
			String::copy(Material::max_name_length, config.material_name, material_name);
		}
		else {
			String::copy(Material::max_name_length, config.material_name, MaterialSystem::Config::default_name);
		}

		return config;

	}
}