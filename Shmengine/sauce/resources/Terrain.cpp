#include "Terrain.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererTypes.hpp"
#include "renderer/RendererGeometry.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/loaders/ImageLoader.hpp"
#include "resources/loaders/TerrainLoader.hpp"

#include "systems/MaterialSystem.hpp"

bool32 terrain_init(TerrainConfig* config, Terrain* out_terrain)
{
    if (out_terrain->state >= TerrainState::INITIALIZED)
        return false;

    if (!config->tile_count_x ||
        !config->tile_count_z ||
        !config->tile_scale_x ||
        !config->tile_scale_z)
    {
        SHMERROR("Failed to init terrain. Tile counts and scales have to be greater than 0.");
        out_terrain->state = TerrainState::UNINITIALIZED;
        return false;
    }

    out_terrain->state = TerrainState::INITIALIZING;
  
    out_terrain->name = config->name;
    out_terrain->xform = config->xform;

    out_terrain->tile_count_x = config->tile_count_x;
    out_terrain->tile_count_z = config->tile_count_z;
    out_terrain->tile_scale_x = config->tile_scale_x;
    out_terrain->tile_scale_z = config->tile_scale_z;
    out_terrain->scale_y = config->scale_y;

    out_terrain->sub_material_names.init(config->sub_materials_count, DarrayFlags::NON_RESIZABLE);
    out_terrain->sub_material_names.set_count(config->sub_materials_count);
    for (uint32 i = 0; i < config->sub_materials_count; i++)
        CString::copy(config->sub_material_names[i], out_terrain->sub_material_names[i].name, max_material_name_length);

    out_terrain->material = 0;

	if (!out_terrain->tile_count_x)
	{
		SHMWARN("tile_count_x must be a positive number. Defaulting to one.");
		out_terrain->tile_count_x = 1;
	}
	if (!out_terrain->tile_count_z)
	{
		SHMWARN("tile_count_z must be a positive number. Defaulting to one.");
		out_terrain->tile_count_z = 1;
	}
	if (!out_terrain->tile_scale_x)
	{
		SHMWARN("tile_scale_x must be nonzero. Defaulting to one.");
		out_terrain->tile_scale_x = 1.0f;
	}
	if (!out_terrain->tile_scale_z)
	{
		SHMWARN("tile_scale_z must be nonzero. Defaulting to one.");
		out_terrain->tile_scale_z = 1.0f;
	}

	GeometryData* geometry = &out_terrain->geometry;
	geometry->extents = {};
	geometry->center = {};

	geometry->vertex_size = sizeof(TerrainVertex);

	if (config->heightmap_name)
	{
		ImageResourceParams image_load_params = {};
		image_load_params.flip_y = false;

		ImageConfig image_config = {};
		if (!ResourceSystem::image_loader_load(config->heightmap_name, &image_load_params, &image_config))
		{
			SHMERROR("Failed to load heightmap for terrain!");
			return false;
		}

		out_terrain->tile_count_x = image_config.width - 1;
		out_terrain->tile_count_z = image_config.height - 1;
		geometry->vertex_count = (out_terrain->tile_count_x + 1) * (out_terrain->tile_count_z + 1);
		out_terrain->vertex_infos.init(geometry->vertex_count, 0);

		for (uint32 i = 0; i < geometry->vertex_count; i++)
		{
			uint8 r = image_config.pixels[(i * 4) + 0];
			float32 height = r / 255.0f;
			out_terrain->vertex_infos[i].height = height;
		}

		ResourceSystem::image_loader_unload(&image_config);
	}
	else
	{
		geometry->vertex_count = (out_terrain->tile_count_x + 1) * (out_terrain->tile_count_z + 1);
		out_terrain->vertex_infos.init(geometry->vertex_count, 0);
	}
	
	//geometry->vertex_count = 4 * tile_count_x * tile_count_z;
	geometry->vertices.init(geometry->vertex_size * geometry->vertex_count, 0);

	uint32 indices_count = geometry->vertex_count * 6;
	geometry->indices.init(indices_count, 0);

	// TODO: read from heightmap 

	for (uint32 z = 0, i = 0; z < out_terrain->tile_count_z + 1; z++)
	{
		for (uint32 x = 0; x < out_terrain->tile_count_x + 1; x++, i++)
		{
			TerrainVertex* v = &geometry->vertices.get_as<TerrainVertex>(i);
			v->position.x = x * out_terrain->tile_scale_x;
			v->position.z = z * out_terrain->tile_scale_z;
			v->position.y = out_terrain->vertex_infos[i].height * out_terrain->scale_y;

			v->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
			v->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
			v->tex_coords.x = (float32)x;
			v->tex_coords.y = (float32)z;

			v->material_weights[0] = smoothstep(0.0f, 0.25f, out_terrain->vertex_infos[i].height);
			v->material_weights[1] = smoothstep(0.25f, 0.5f, out_terrain->vertex_infos[i].height);
			v->material_weights[2] = smoothstep(0.5f, 0.75f, out_terrain->vertex_infos[i].height);
			v->material_weights[3] = smoothstep(0.75f, 1.0f, out_terrain->vertex_infos[i].height);
		}
	}

	for (uint32 z = 0, i = 0; z < out_terrain->tile_count_z; z++)
	{
		for (uint32 x = 0; x < out_terrain->tile_count_x; x++, i += 6)
		{
			uint32 v0 = (z * (out_terrain->tile_count_x + 1)) + x;
			uint32 v1 = (z * (out_terrain->tile_count_x + 1)) + x + 1;
			uint32 v2 = ((z + 1) * (out_terrain->tile_count_x + 1)) + x;
			uint32 v3 = ((z + 1) * (out_terrain->tile_count_x + 1)) + x + 1;

			geometry->indices[i + 0] = v2;
			geometry->indices[i + 1] = v1;
			geometry->indices[i + 2] = v0;
			geometry->indices[i + 3] = v3;
			geometry->indices[i + 4] = v1;
			geometry->indices[i + 5] = v2;
		}
	}

	//for (uint32 z = 0, i = 0; z < tile_count_z; z++) 
	//{
	//	for (uint32 x = 0; x < tile_count_x; x++) 
	//	{
	//		// Bottom Left
	//		TerrainVertex* v = &out_geometry->vertices.get_as<TerrainVertex>(i++);
	//		v->position.x = x * tile_scale_x;
	//		v->position.y = 0.0f;  // <-- this will be modified by a heightmap.
	//		v->position.z = z * tile_scale_z;	

	//		v->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
	//		v->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
	//		v->tex_coords.x = (float32)x;
	//		v->tex_coords.y = (float32)z;

	//		v->tangent = {};  // TODO: obviously wrong.

	//		// Bottom Right
	//		TerrainVertex* v1 = &out_geometry->vertices.get_as<TerrainVertex>(i++);
	//		v1->position.x = (x + 1) * tile_scale_x;
	//		v1->position.y = 0.0f;  // <-- this will be modified by a heightmap.
	//		v1->position.z = z * tile_scale_z;

	//		v1->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
	//		v1->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
	//		v1->tex_coords.x = (float32)x;
	//		v1->tex_coords.y = (float32)z;

	//		v1->tangent = {};  // TODO: obviously wrong.

	//		// Top Left
	//		TerrainVertex* v2 = &out_geometry->vertices.get_as<TerrainVertex>(i++);
	//		v2->position.x = x * tile_scale_x;
	//		v2->position.y = 0.0f;  // <-- this will be modified by a heightmap.
	//		v2->position.z = (z + 1) * tile_scale_z;

	//		v2->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
	//		v2->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
	//		v2->tex_coords.x = (float32)x;
	//		v2->tex_coords.y = (float32)z;

	//		v2->tangent = {};  // TODO: obviously wrong.

	//		// Top Right
	//		TerrainVertex* v3 = &out_geometry->vertices.get_as<TerrainVertex>(i++);
	//		v3->position.x = (x + 1) * tile_scale_x;
	//		v3->position.y = 0.0f;  // <-- this will be modified by a heightmap.
	//		v3->position.z = (z + 1) * tile_scale_z;

	//		v3->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
	//		v3->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
	//		v3->tex_coords.x = (float32)x;
	//		v3->tex_coords.y = (float32)z;

	//		v3->tangent = {};  // TODO: obviously wrong.
	//	}
	//}

	//for (uint32 z = 0, i = 0, v = 0; z < tile_count_z; z++) 
	//{
	//	for (uint32 x = 0; x < tile_count_x; x++, i += 6) 
	//	{
	//		uint32 v0 = v++;
	//		uint32 v1 = v++;
	//		uint32 v2 = v++;
	//		uint32 v3 = v++;

	//		out_geometry->indices[i + 0] = v2;
	//		out_geometry->indices[i + 1] = v1;
	//		out_geometry->indices[i + 2] = v0;
	//		out_geometry->indices[i + 3] = v3;
	//		out_geometry->indices[i + 4] = v1;
	//		out_geometry->indices[i + 5] = v2;
	//	}
	//}

    Renderer::geometry_generate_terrain_normals(out_terrain->geometry.vertex_count, (TerrainVertex*)out_terrain->geometry.vertices.data,
        out_terrain->geometry.indices.capacity, out_terrain->geometry.indices.data);
    Renderer::geometry_generate_terrain_tangents(out_terrain->geometry.vertex_count, (TerrainVertex*)out_terrain->geometry.vertices.data,
        out_terrain->geometry.indices.capacity, out_terrain->geometry.indices.data);

    out_terrain->geometry.id = INVALID_ID;

    out_terrain->state = TerrainState::INITIALIZED;

    return true;
}

bool32 terrain_init_from_resource(const char* resource_name, Terrain* out_terrain)
{
    
    out_terrain->state = TerrainState::INITIALIZING;

    TerrainResourceData resource = {};
    if (!ResourceSystem::terrain_loader_load(resource_name, 0, &resource))
    {
        SHMERRORV("Failed to load terrain from resource '%s'", resource_name);
        out_terrain->state = TerrainState::UNINITIALIZED;
        return false;
    }

    TerrainConfig config = {};
    config.name = resource.name;

    if (resource.heightmap_name[0])
        config.heightmap_name = resource.heightmap_name;

    config.tile_count_x = resource.tile_count_x;
    config.tile_count_z = resource.tile_count_z;
    config.tile_scale_x = resource.tile_scale_x;
    config.tile_scale_z = resource.tile_scale_z;
    config.scale_y = resource.scale_y;

    config.sub_materials_count = resource.sub_materials_count;
    const char* submaterial_names[max_terrain_materials_count];
    for (uint32 i = 0; i < config.sub_materials_count; i++)
        submaterial_names[i] = resource.sub_material_names[i].name;

    config.sub_material_names = submaterial_names;
    config.xform = resource.xform;

    terrain_init(&config, out_terrain);
    ResourceSystem::terrain_loader_unload(&resource);

    return true;

}

bool32 terrain_destroy(Terrain* terrain)
{
    if (terrain->state != TerrainState::UNLOADED && !terrain_unload(terrain))
        return false;

    terrain->geometry.vertices.free_data();
    terrain->geometry.indices.free_data();
    terrain->vertex_infos.free_data();

    terrain->sub_material_names.free_data();

    terrain->name.free_data();

    terrain->state = TerrainState::DESTROYED;
    return true;
}

bool32 terrain_load(Terrain* terrain)
{

    if (terrain->state != TerrainState::INITIALIZED && terrain->state != TerrainState::UNLOADED)
        return false;

    bool32 is_reload = terrain->state == TerrainState::UNLOADED;

    terrain->state = TerrainState::LOADING;

    if (!Renderer::geometry_load(&terrain->geometry))
    {
        SHMERROR("Failed to load terrain geometry!");
        return false;
    }

    char terrain_material_name[max_material_name_length] = {};
    CString::print_s(terrain_material_name, max_material_name_length, "terrain_mat_%s", terrain->name.c_str());
    const char* submaterial_names[max_terrain_materials_count];
    for (uint32 i = 0; i < terrain->sub_material_names.count; i++)
        submaterial_names[i] = terrain->sub_material_names[i].name;

    terrain->material = MaterialSystem::acquire_terrain_material(terrain_material_name, terrain->sub_material_names.count, submaterial_names, true);
    if (!terrain->material)
    {
        SHMWARN("Failed to load terrain material. Using default.");
        terrain->material = MaterialSystem::get_default_terrain_material();
    }

    terrain->state = TerrainState::LOADED;

    return true;

}

bool32 terrain_unload(Terrain* terrain)
{

    if (terrain->state <= TerrainState::INITIALIZED)
        return true;
    else if (terrain->state != TerrainState::LOADED)
        return false;

    terrain->state = TerrainState::UNLOADING;

    MaterialSystem::release(terrain->material->name);
    terrain->material = 0;
    Renderer::geometry_unload(&terrain->geometry);

    terrain->state = TerrainState::UNLOADED;

    return true;

}

bool32 terrain_update(Terrain* terrain)
{
    return true;
}
