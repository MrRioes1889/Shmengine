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

	bool32 create_geometry(GeometryConfig config, Geometry* g);
	void destroy_geometry(Geometry* g);
	bool32 create_default_geometries();
	

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

	bool32 create_geometry(GeometryConfig config, Geometry* g)
	{

		if (!Renderer::create_geometry(g, config.vertex_size, config.vertex_count, config.vertices, config.index_count, config.indices))
		{
			system_state->registered_geometries[g->id].reference_count = 0;
			system_state->registered_geometries[g->id].auto_release = false;
			g->id = INVALID_ID;
			g->generation = INVALID_ID;
			g->internal_id = INVALID_ID;

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
		if (!Renderer::create_geometry(&system_state->default_geometry, sizeof(Renderer::Vertex3D), 4, verts, 6, indices)) {
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
		if (!Renderer::create_geometry(&system_state->default_geometry_2d, sizeof(Renderer::Vertex2D), 4, verts_2d, 6, indices_2d)) {
			SHMFATAL("Failed to create default geometry. Application cannot continue.");
			return false;
		}

		// Acquire the default material.
		system_state->default_geometry.material = MaterialSystem::get_default_material();
		system_state->default_geometry_2d.material = MaterialSystem::get_default_material();

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
		config.vertex_size = sizeof(Renderer::Vertex3D);  // 4 verts per segment
		config.vertex_count = x_segment_count * y_segment_count * 4;  // 4 verts per segment
		config.vertices = (Renderer::Vertex3D*)Memory::allocate(config.vertex_size * config.vertex_count, true, AllocationTag::MAIN);
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
				Renderer::Vertex3D* v0 = &((Renderer::Vertex3D*)config.vertices)[v_offset + 0];
				Renderer::Vertex3D* v1 = &((Renderer::Vertex3D*)config.vertices)[v_offset + 1];
				Renderer::Vertex3D* v2 = &((Renderer::Vertex3D*)config.vertices)[v_offset + 2];
				Renderer::Vertex3D* v3 = &((Renderer::Vertex3D*)config.vertices)[v_offset + 3];

				v0->position.x = min_x;
				v0->position.y = min_y;
				v0->texcoord.x = min_uvx;
				v0->texcoord.y = min_uvy;

				v1->position.x = max_x;
				v1->position.y = max_y;
				v1->texcoord.x = max_uvx;
				v1->texcoord.y = max_uvy;

				v2->position.x = min_x;
				v2->position.y = max_y;
				v2->texcoord.x = min_uvx;
				v2->texcoord.y = max_uvy;

				v3->position.x = max_x;
				v3->position.y = min_y;
				v3->texcoord.x = max_uvx;
				v3->texcoord.y = min_uvy;

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

		if (name && CString::length(name) > 0) {
			CString::copy(Geometry::max_name_length, config.name, name);
		}
		else {
			CString::copy(Geometry::max_name_length, config.name, Config::default_name);
		}

		if (material_name && CString::length(material_name) > 0) {
			CString::copy(Material::max_name_length, config.material_name, material_name);
		}
		else {
			CString::copy(Material::max_name_length, config.material_name, MaterialSystem::Config::default_name);
		}

		return config;

	}

	GeometryConfig generate_cube_config(float32 width, float32 height, float32 depth, float32 tile_x, float32 tile_y, const char* name, const char* material_name)
	{

		if (width == 0) {
			SHMWARN("Width must be nonzero. Defaulting to one.");
			width = 1.0f;
		}
		if (height == 0) {
			SHMWARN("Height must be nonzero. Defaulting to one.");
			height = 1.0f;
		}
		if (depth == 0) {
			SHMWARN("x_segment_count must be a positive number. Defaulting to one.");
			depth = 1.0f;
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
		config.vertex_size = sizeof(Renderer::Vertex3D);  // 4 verts per segment
		config.vertex_count = 4 * 6;  // 4 verts per segment
		config.vertices = (Renderer::Vertex3D*)Memory::allocate(config.vertex_size * config.vertex_count, true, AllocationTag::MAIN);
		config.index_count = 6 * 6;  // 6 indices per segment
		config.indices = (uint32*)Memory::allocate(sizeof(uint32) * config.index_count, true, AllocationTag::MAIN);

		// TODO: This generates extra vertices, but we can always deduplicate them later.
		float32 half_width = width * 0.5f;
		float32 half_height = height * 0.5f;
		float32 half_depth = depth * 0.5f;

		float32 min_x = -half_width;
		float32 min_y = -half_height;
		float32 min_z = -half_depth;
		float32 max_x = half_width;
		float32 max_y = half_height;
		float32 max_z = half_depth;
		float32 min_uvx = 0.0f;
		float32 min_uvy = 0.0f;
		float32 max_uvx = tile_x;
		float32 max_uvy = tile_y;

		Renderer::Vertex3D* verts = (Renderer::Vertex3D*)config.vertices;

		// Front
		verts[(0 * 4) + 0].position = { min_x, min_y, max_z };
		verts[(0 * 4) + 1].position = { max_x, max_y, max_z };
		verts[(0 * 4) + 2].position = { min_x, max_y, max_z };
		verts[(0 * 4) + 3].position = { max_x, min_y, max_z };
		verts[(0 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(0 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(0 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(0 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(0 * 4) + 0].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 1].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 2].normal = { 0.0f, 0.0f, 1.0f };
		verts[(0 * 4) + 3].normal = { 0.0f, 0.0f, 1.0f };

		//Back
		verts[(1 * 4) + 0].position = { max_x, min_y, min_z };
		verts[(1 * 4) + 1].position = { min_x, max_y, min_z };
		verts[(1 * 4) + 2].position = { max_x, max_y, min_z };
		verts[(1 * 4) + 3].position = { min_x, min_y, min_z };
		verts[(1 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(1 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(1 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(1 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(1 * 4) + 0].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 1].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 2].normal = { 0.0f, 0.0f, -1.0f };
		verts[(1 * 4) + 3].normal = { 0.0f, 0.0f, -1.0f };

		//Left
		verts[(2 * 4) + 0].position = { min_x, min_y, min_z };
		verts[(2 * 4) + 1].position = { min_x, max_y, max_z };
		verts[(2 * 4) + 2].position = { min_x, max_y, min_z };
		verts[(2 * 4) + 3].position = { min_x, min_y, max_z };
		verts[(2 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(2 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(2 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(2 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(2 * 4) + 0].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 1].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 2].normal = { -1.0f, 0.0f, 0.0f };
		verts[(2 * 4) + 3].normal = { -1.0f, 0.0f, 0.0f };

		//Right
		verts[(3 * 4) + 0].position = { max_x, min_y, max_z };
		verts[(3 * 4) + 1].position = { max_x, max_y, min_z };
		verts[(3 * 4) + 2].position = { max_x, max_y, max_z };
		verts[(3 * 4) + 3].position = { max_x, min_y, min_z };
		verts[(3 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(3 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(3 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(3 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(3 * 4) + 0].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 1].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 2].normal = { 1.0f, 0.0f, 0.0f };
		verts[(3 * 4) + 3].normal = { 1.0f, 0.0f, 0.0f };

		//Bottom
		verts[(4 * 4) + 0].position = { max_x, min_y, max_z };
		verts[(4 * 4) + 1].position = { min_x, min_y, min_z };
		verts[(4 * 4) + 2].position = { max_x, min_y, min_z };
		verts[(4 * 4) + 3].position = { min_x, min_y, max_z };
		verts[(4 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(4 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(4 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(4 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(4 * 4) + 0].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 1].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 2].normal = { 0.0f, -1.0f, 0.0f };
		verts[(4 * 4) + 3].normal = { 0.0f, -1.0f, 0.0f };

		//Top
		verts[(5 * 4) + 0].position = { min_x, max_y, max_z };
		verts[(5 * 4) + 1].position = { max_x, max_y, min_z };
		verts[(5 * 4) + 2].position = { min_x, max_y, min_z };
		verts[(5 * 4) + 3].position = { max_x, max_y, max_z };
		verts[(5 * 4) + 0].texcoord = { min_uvx, min_uvy };
		verts[(5 * 4) + 1].texcoord = { max_uvx, max_uvy };
		verts[(5 * 4) + 2].texcoord = { min_uvx, max_uvy };
		verts[(5 * 4) + 3].texcoord = { max_uvx, min_uvy };
		verts[(5 * 4) + 0].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 1].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 2].normal = { 0.0f, 1.0f, 0.0f };
		verts[(5 * 4) + 3].normal = { 0.0f, 1.0f, 0.0f };

		for (uint32 i = 0; i < 6; ++i) {
			uint32 v_offset = i * 4;
			uint32 i_offset = i * 6;
			((uint32*)config.indices)[i_offset + 0] = v_offset + 0;
			((uint32*)config.indices)[i_offset + 1] = v_offset + 1;
			((uint32*)config.indices)[i_offset + 2] = v_offset + 2;
			((uint32*)config.indices)[i_offset + 3] = v_offset + 0;
			((uint32*)config.indices)[i_offset + 4] = v_offset + 3;
			((uint32*)config.indices)[i_offset + 5] = v_offset + 1;
		}

		if (name && CString::length(name) > 0) {
			CString::copy(Geometry::max_name_length, config.name, name);
		}
		else {
			CString::copy(Geometry::max_name_length, config.name, Config::default_name);
		}

		if (material_name && CString::length(material_name) > 0) {
			CString::copy(Material::max_name_length, config.material_name, material_name);
		}
		else {
			CString::copy(Material::max_name_length, config.material_name, MaterialSystem::Config::default_name);
		}

		return config;

	}
}