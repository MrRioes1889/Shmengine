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
		GeometryData geometry;
		bool32 auto_release;
	};

	struct SystemState
	{
		GeometrySystem::SystemConfig config;

		GeometryData default_geometry;
		GeometryData default_geometry_2d;

		GeometryReference* registered_geometries;
	};

	static SystemState* system_state = 0;

	static bool32 create_geometry(GeometryConfig* config, GeometryData* g);
	static void destroy_geometry(GeometryData* g, GeometryConfig* out_revert_config);
	static bool32 create_default_geometries();

	bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config)
    {

		SystemConfig* sys_config = (SystemConfig*)config;
		system_state = (SystemState*)allocator_callback(allocator, sizeof(SystemState));

        system_state->config = *sys_config;

        uint64 geometry_array_size = sizeof(GeometryReference) * sys_config->max_geometry_count;
        system_state->registered_geometries = (GeometryReference*)allocator_callback(allocator, geometry_array_size);

        // Invalidate all geometries in the array.
        uint32 count = system_state->config.max_geometry_count;
        for (uint32 i = 0; i < count; ++i)
            system_state->registered_geometries[i].geometry.id = Constants::max_u32;

        if (!create_default_geometries()) {
            SHMFATAL("Failed to create default geometries. Application cannot continue.");
            return false;
        }

        return true;

	}

	void system_shutdown(void* state)
	{
		Renderer::geometry_unload(&system_state->default_geometry);
		Renderer::geometry_unload(&system_state->default_geometry_2d);
		destroy_geometry(&system_state->default_geometry, 0);
		destroy_geometry(&system_state->default_geometry_2d, 0);
		system_state = 0;
	}

	GeometryData* acquire_by_id(uint32 id)
	{
		if (id != Constants::max_u32 && system_state->registered_geometries[id].geometry.id != Constants::max_u32)
		{
			system_state->registered_geometries[id].reference_count++;
			return &system_state->registered_geometries[id].geometry;
		}

		SHMERROR("acquire_by_id - Cannot load invalid geometry id!");
		return 0;
	}

	GeometryData* acquire_from_config(GeometryConfig* config, bool32 auto_release)
	{

		GeometryData* g = 0;
		for (uint32 i = 0; i < system_state->config.max_geometry_count; i++)
		{
			if (system_state->registered_geometries[i].geometry.id == Constants::max_u32)
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

	void release(GeometryData* g, GeometryConfig* out_revert_config)
	{
		if (g->id != Constants::max_u32)
		{
			GeometryReference& ref = system_state->registered_geometries[g->id];

			uint32 id = g->id;
			if (ref.geometry.id == id)
			{
				if (ref.reference_count > 0)
					ref.reference_count--;

				if (ref.reference_count < 1 && ref.auto_release)
				{
					destroy_geometry(g, out_revert_config);
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

	uint32 get_ref_count(GeometryData* geometry)
	{
		if (geometry->id == Constants::max_u32)
			return 0;

		GeometryReference ref = system_state->registered_geometries[geometry->id];
		return ref.reference_count;
	}

	GeometryData* get_default_geometry()
	{
		return &system_state->default_geometry;
	}

	GeometryData* get_default_geometry_2d()
	{
		return &system_state->default_geometry_2d;
	}

	static bool32 create_geometry(GeometryConfig* config, GeometryData* g)
	{

		g->center = config->center;
		g->extents.min = config->extents.min;
		g->extents.max = config->extents.max;

		g->vertex_size = config->vertex_size;
		g->vertex_count = config->vertex_count;
		g->index_count = config->index_count;
		g->vertices.steal(config->vertices);
		g->indices.steal(config->indices);

		return true;

	}

	static void destroy_geometry(GeometryData* g, GeometryConfig* out_config)
	{

		if (out_config)
		{
			CString::copy(g->name, out_config->name, Constants::max_geometry_name_length);

			out_config->center = g->center;
			out_config->extents.min = g->extents.min;
			out_config->extents.max = g->extents.max;
			out_config->vertex_count = g->vertex_count;
			out_config->index_count = g->index_count;
			out_config->vertex_size = g->vertex_size;

			out_config->vertices.steal(g->vertices);
			out_config->indices.steal(g->indices);
		}
		
		g->id = Constants::max_u32;

		g->vertices.free_data();
		g->indices.free_data();
		g->vertex_size = 0;
		g->vertex_count = 0;
		g->index_count = 0;

		CString::empty(g->name);

	}

	static bool32 create_default_geometries()
	{

		system_state->default_geometry.vertex_count = 4;
		system_state->default_geometry.index_count = 6;
		system_state->default_geometry.vertex_size = sizeof(Renderer::Vertex3D);

		Renderer::Vertex3D verts[4] = {};

		float32 f = 10.0f;

		verts[0].position.x = -0.5f * f;  // 0    3
		verts[0].position.y = -0.5f * f;  //
		verts[0].tex_coords.x = 0.0f;      //
		verts[0].tex_coords.y = 0.0f;      // 2    1

		verts[1].position.y = 0.5f * f;
		verts[1].position.x = 0.5f * f;
		verts[1].tex_coords.x = 1.0f;
		verts[1].tex_coords.y = 1.0f;

		verts[2].position.x = -0.5f * f;
		verts[2].position.y = 0.5f * f;
		verts[2].tex_coords.x = 0.0f;
		verts[2].tex_coords.y = 1.0f;

		verts[3].position.x = 0.5f * f;
		verts[3].position.y = -0.5f * f;
		verts[3].tex_coords.x = 1.0f;
		verts[3].tex_coords.y = 0.0f;

		uint32 indices[6] = { 0, 1, 2, 0, 3, 1 };

		system_state->default_geometry.vertices.init(sizeof(Renderer::Vertex3D) * system_state->default_geometry.vertex_count, 0, AllocationTag::ARRAY, verts);
		system_state->default_geometry.indices.init(system_state->default_geometry.index_count, 0, AllocationTag::ARRAY, indices);

		// Send the geometry off to the renderer to be uploaded to the GPU.
		system_state->default_geometry.id = Constants::max_u32;
		
		if (!Renderer::geometry_load(&system_state->default_geometry)) {
			SHMFATAL("Failed to create default geometry. Application cannot continue.");
			return false;
		}

		system_state->default_geometry_2d.vertex_count = 4;
		system_state->default_geometry_2d.index_count = 6;
		system_state->default_geometry_2d.vertex_size = sizeof(Renderer::Vertex2D);

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

		system_state->default_geometry_2d.vertices.init(sizeof(Renderer::Vertex2D) * system_state->default_geometry_2d.vertex_count, 0, AllocationTag::ARRAY, verts_2d);
		system_state->default_geometry_2d.indices.init(system_state->default_geometry_2d.index_count, 0, AllocationTag::ARRAY, indices_2d);

		// Send the geometry off to the renderer to be uploaded to the GPU.
		system_state->default_geometry_2d.id = Constants::max_u32;
		if (!Renderer::geometry_load(&system_state->default_geometry_2d)) {
			SHMFATAL("Failed to create default geometry. Application cannot continue.");
			return false;
		}

		return true;

	}

	
}