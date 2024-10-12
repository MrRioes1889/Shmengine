#include "GeometrySystem.hpp"

#include "core/Logging.hpp"
#include "utility/CString.hpp"
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
		Geometry default_geometry_2d;

		GeometryReference* registered_geometries;
	};

	static SystemState* system_state = 0;

	bool32 create_geometry(const GeometryConfig& config, Geometry* g);
	void destroy_geometry(Geometry* g);
	bool32 create_default_geometries();
	

	bool32 system_init(FP_allocator_allocate_callback allocator_callback, void*& out_state, Config config)
    {
        out_state = allocator_callback(sizeof(SystemState));
        system_state = (SystemState*)out_state;

        system_state->config = config;

        uint64 geometry_array_size = sizeof(GeometryReference) * config.max_geometry_count;
        system_state->registered_geometries = (GeometryReference*)allocator_callback(geometry_array_size);

        // Invalidate all geometries in the array.
        uint32 count = system_state->config.max_geometry_count;
        for (uint32 i = 0; i < count; ++i) {
            system_state->registered_geometries[i].geometry.id = INVALID_ID;
            system_state->registered_geometries[i].geometry.generation = INVALID_ID;
            system_state->registered_geometries[i].geometry.internal_id = INVALID_ID;
        }

        if (!create_default_geometries()) {
            SHMFATAL("Failed to create default geometries. Application cannot continue.");
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
		if (id != INVALID_ID && system_state->registered_geometries[id].geometry.id != INVALID_ID)
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
			if (system_state->registered_geometries[i].geometry.id == INVALID_ID)
			{
				system_state->registered_geometries[i].reference_count = 1;
				system_state->registered_geometries[i].auto_release = auto_release;
				g = &system_state->registered_geometries[i].geometry;
				g->id = i;
				break;
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
		if (g->id != INVALID_ID)
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

	Geometry* get_default_geometry_2d()
	{
		return &system_state->default_geometry_2d;
	}

	bool32 create_geometry(const GeometryConfig& config, Geometry* g)
	{

		if (!Renderer::geometry_create(g, config.vertex_size, config.vertex_count, config.vertices.data, config.indices.capacity, config.indices.data))
		{
			system_state->registered_geometries[g->id].reference_count = 0;
			system_state->registered_geometries[g->id].auto_release = false;
			g->id = INVALID_ID;
			g->generation = INVALID_ID;
			g->internal_id = INVALID_ID;

			return false;
		}

		g->center = config.center;
		g->extents.min = config.min_extents;
		g->extents.max = config.max_extents;

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
		Renderer::geometry_destroy(g);
		g->internal_id = INVALID_ID;
		g->generation = INVALID_ID;
		g->id = INVALID_ID;

		CString::empty(g->name);

		if (g->material)
		{
			MaterialSystem::release(g->material->name);
			g->material = 0;
		}
	}

	bool32 create_default_geometries()
	{

		Renderer::Vertex3D verts[4] = {};

		float32 f = 10.0f;

		verts[0].position.x = -0.5f * f;  // 0    3
		verts[0].position.y = -0.5f * f;  //
		verts[0].texcoord.x = 0.0f;      //
		verts[0].texcoord.y = 0.0f;      // 2    1

		verts[1].position.y = 0.5f * f;
		verts[1].position.x = 0.5f * f;
		verts[1].texcoord.x = 1.0f;
		verts[1].texcoord.y = 1.0f;

		verts[2].position.x = -0.5f * f;
		verts[2].position.y = 0.5f * f;
		verts[2].texcoord.x = 0.0f;
		verts[2].texcoord.y = 1.0f;

		verts[3].position.x = 0.5f * f;
		verts[3].position.y = -0.5f * f;
		verts[3].texcoord.x = 1.0f;
		verts[3].texcoord.y = 0.0f;

		uint32 indices[6] = { 0, 1, 2, 0, 3, 1 };

		// Send the geometry off to the renderer to be uploaded to the GPU.
		system_state->default_geometry.id = INVALID_ID;
		system_state->default_geometry.internal_id = INVALID_ID;
		system_state->default_geometry.generation = INVALID_ID;
		if (!Renderer::geometry_create(&system_state->default_geometry, sizeof(Renderer::Vertex3D), 4, verts, 6, indices)) {
			SHMFATAL("Failed to create default geometry. Application cannot continue.");
			return false;
		}

		Renderer::Vertex2D verts_2d[4] = {};

		f = 10.0f;

		verts_2d[0].position.x = -0.5f * f;
		verts_2d[0].position.y = -0.5f * f;
		verts_2d[0].tex_coordinates.x = 0.0f;
		verts_2d[0].tex_coordinates.y = 0.0f;

		verts_2d[1].position.y = 0.5f * f;
		verts_2d[1].position.x = 0.5f * f;
		verts_2d[1].tex_coordinates.x = 1.0f;
		verts_2d[1].tex_coordinates.y = 1.0f;

		verts_2d[2].position.x = -0.5f * f;
		verts_2d[2].position.y = 0.5f * f;
		verts_2d[2].tex_coordinates.x = 0.0f;
		verts_2d[2].tex_coordinates.y = 1.0f;

		verts_2d[3].position.x = 0.5f * f;
		verts_2d[3].position.y = -0.5f * f;
		verts_2d[3].tex_coordinates.x = 1.0f;
		verts_2d[3].tex_coordinates.y = 0.0f;

		uint32 indices_2d[6] = { 2, 1, 0, 3, 0, 1 };

		// Send the geometry off to the renderer to be uploaded to the GPU.
		system_state->default_geometry_2d.id = INVALID_ID;
		system_state->default_geometry_2d.internal_id = INVALID_ID;
		system_state->default_geometry_2d.generation = INVALID_ID;
		if (!Renderer::geometry_create(&system_state->default_geometry_2d, sizeof(Renderer::Vertex2D), 4, verts_2d, 6, indices_2d)) {
			SHMFATAL("Failed to create default geometry. Application cannot continue.");
			return false;
		}

		// Acquire the default material.
		system_state->default_geometry.material = MaterialSystem::get_default_material();
		system_state->default_geometry_2d.material = MaterialSystem::get_default_material();

		return true;

	}

	
}