#include "Terrain.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "renderer/RendererTypes.hpp"
#include "renderer/RendererGeometry.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/loaders/ImageLoader.hpp"
#include "resources/loaders/TerrainLoader.hpp"

#include "systems/ShaderSystem.hpp"

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
    out_terrain->xform = Math::transform_create();

	out_terrain->shader_instance_id = INVALID_ID;
	out_terrain->render_frame_number = INVALID_ID;

	out_terrain->material_properties = {};

    out_terrain->tile_count_x = config->tile_count_x;
    out_terrain->tile_count_z = config->tile_count_z;
    out_terrain->tile_scale_x = config->tile_scale_x;
    out_terrain->tile_scale_z = config->tile_scale_z;
    out_terrain->scale_y = config->scale_y;

    out_terrain->materials.init(config->materials_count, DarrayFlags::NON_RESIZABLE);
    out_terrain->materials.set_count(config->materials_count);
    for (uint32 i = 0; i < config->materials_count; i++)
        CString::copy(config->material_names[i], out_terrain->materials[i].name, max_material_name_length);

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
			if (height > geometry->extents.max.y)
				geometry->extents.max.y = height;
		}

		/*geometry->extents.max.y *= out_terrain->scale_y * 0.5f;
		geometry->extents.min.y = -geometry->extents.max.y;*/
		geometry->extents.max.y *= out_terrain->scale_y;
		geometry->extents.min.y = 0.0f;

		ResourceSystem::image_loader_unload(&image_config);
	}
	else
	{
		geometry->vertex_count = (out_terrain->tile_count_x + 1) * (out_terrain->tile_count_z + 1);
		out_terrain->vertex_infos.init(geometry->vertex_count, 0);
	}

	geometry->extents.max.x = out_terrain->tile_count_x * out_terrain->tile_scale_x * 0.5f;
	geometry->extents.min.x = -geometry->extents.max.x;
	geometry->extents.max.z = out_terrain->tile_count_z * out_terrain->tile_scale_z * 0.5f;
	geometry->extents.min.z = -geometry->extents.max.z;
	
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
			v->position.x = x * out_terrain->tile_scale_x + geometry->extents.min.x;		
			v->position.y = out_terrain->vertex_infos[i].height * out_terrain->scale_y + geometry->extents.min.y;
			v->position.z = z * out_terrain->tile_scale_z + geometry->extents.min.z;

			v->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
			v->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
			v->tex_coords.x = (float32)x;
			v->tex_coords.y = (float32)z;

			float32 step_size = 1.0f / (float32)out_terrain->materials.count;
			v->material_weights[0] = 1.0f - smoothstep(0.0f, step_size, out_terrain->vertex_infos[i].height);
			for (uint32 w = 1; w < out_terrain->materials.count; w++)
				v->material_weights[w] = smoothstep(step_size * (w - 1), step_size * w, out_terrain->vertex_infos[i].height) - smoothstep(step_size * w, step_size * (w + 1), out_terrain->vertex_infos[i].height);

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

    config.materials_count = resource.sub_materials_count;
    const char* submaterial_names[max_terrain_materials_count];
    for (uint32 i = 0; i < config.materials_count; i++)
        submaterial_names[i] = resource.sub_material_names[i].name;

    config.material_names = submaterial_names;

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

    terrain->materials.free_data();

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

	uint32 material_count = terrain->materials.count;

	Material* sub_materials[max_terrain_materials_count] = {};
	for (uint32 i = 0; i < material_count; i++)
		sub_materials[i] = MaterialSystem::acquire(terrain->materials[i].name);

	terrain->material_properties.materials_count = material_count;

	const char* map_names[3] = { "diffuse", "specular", "normal" };
	Texture* default_textures[3] =
	{
		TextureSystem::get_default_diffuse_texture(),
		TextureSystem::get_default_specular_texture(),
		TextureSystem::get_default_normal_texture()
	};

	Material* default_material = MaterialSystem::get_default_material();

	// Phong properties and maps for each material.
	for (uint32 mat_i = 0; mat_i < max_terrain_materials_count; mat_i++) {
		// Properties.
		MaterialPhongProperties* mat_props = &terrain->material_properties.materials[mat_i];
		// Use default material unless within the material count.
		Material* sub_mat = default_material;
		if (mat_i < material_count && sub_mat->maps.capacity >= 3)
			sub_mat = sub_materials[mat_i];

		MaterialPhongProperties* sub_mat_properties = (MaterialPhongProperties*)sub_mat->properties;
		mat_props->diffuse_color = sub_mat_properties->diffuse_color;
		mat_props->shininess = sub_mat_properties->shininess;

		// Maps, 3 for phong. Diffuse, spec, normal.
		for (uint32 map_i = 0; map_i < 3; ++map_i) {
			TextureMap* map = &terrain->texture_maps[(mat_i * 3) + map_i];
			map->repeat_u = sub_mat->maps[map_i].repeat_u;
			map->repeat_v = sub_mat->maps[map_i].repeat_v;
			map->repeat_w = sub_mat->maps[map_i].repeat_w;
			map->filter_minify = sub_mat->maps[map_i].filter_minify;
			map->filter_magnify = sub_mat->maps[map_i].filter_magnify;

			if (sub_mat == default_material || sub_mat->maps[map_i].texture == default_textures[map_i])
			{
				map->texture = default_textures[map_i];
			}
			else
			{
				map->texture = TextureSystem::acquire(sub_mat->maps[map_i].texture->name, true);
				if (!map->texture)
				{
					SHMWARN("Unable to acquire texture from terrain sub material.");
					map->texture = default_textures[map_i];
				}
			}

			if (!Renderer::texture_map_acquire_resources(map))
			{
				SHMERROR("Unable to acquire resources for texture map.");
				return 0;
			}
		}
	}

	for (uint32 i = 0; i < material_count; i++)
		MaterialSystem::release(terrain->materials[i].name);

	Renderer::Shader* terrain_shader = ShaderSystem::get_shader(ShaderSystem::get_terrain_shader_id());
	
	const uint32 max_map_count = max_terrain_materials_count * 3;
	TextureMap* maps[max_map_count] = {};
	for (uint32 i = 0; i < max_map_count; i++)
		maps[i] = &terrain->texture_maps[i];

	if (!Renderer::shader_acquire_instance_resources(terrain_shader, max_map_count, maps, &terrain->shader_instance_id))
		SHMERRORV("Failed to acquire renderer resources for terrain '%s'.", terrain->name.c_str());

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

	Renderer::Shader* terrain_shader = ShaderSystem::get_shader(ShaderSystem::get_terrain_shader_id());
	Renderer::shader_release_instance_resources(terrain_shader, terrain->shader_instance_id);
	terrain->shader_instance_id = INVALID_ID;

	for (uint32 map_i = 0; map_i < max_terrain_materials_count * 3; map_i++)
	{
		Renderer::texture_map_release_resources(&terrain->texture_maps[map_i]);
		if (terrain->texture_maps[map_i].texture->id != INVALID_ID)
			TextureSystem::release(terrain->texture_maps[map_i].texture->name);
	}

    Renderer::geometry_unload(&terrain->geometry);

    terrain->state = TerrainState::UNLOADED;

    return true;

}

bool32 terrain_update(Terrain* terrain)
{
    return true;
}

bool32 terrain_on_render(uint32 shader_id, LightingInfo lighting, Math::Mat4* model, void* terrain, uint32 frame_number)
{

	Terrain* t = (Terrain*)terrain;

	if (shader_id == ShaderSystem::get_terrain_shader_id())
	{
		ShaderSystem::bind_instance(t->shader_instance_id);

		ShaderSystem::TerrainShaderUniformLocations u_locations = ShaderSystem::get_terrain_shader_uniform_locations();
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.properties, &t->material_properties));

		for (uint32 mat_i = 0; mat_i < t->materials.count; mat_i++)
		{
			UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.samplers[mat_i * 3], &t->texture_maps[mat_i * 3]));
			UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.samplers[mat_i * 3 + 1], &t->texture_maps[mat_i * 3 + 1]));
			UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.samplers[mat_i * 3 + 2], &t->texture_maps[mat_i * 3 + 2]));
		}

		if (lighting.p_lights)
		{
			UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, &lighting.p_lights_count));
			UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights, lighting.p_lights));
		}
		else
		{
			UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, 0));
		}

		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.model, model));
	}
	else
	{
		SHMERRORV("Unknown shader id %u for rendering terrain. Skipping uniforms.", shader_id);
		return false;
	}

	bool32 needs_update = (t->render_frame_number != (uint32)frame_number);
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_apply_instance(ShaderSystem::get_shader(shader_id), needs_update));
	t->render_frame_number = frame_number;

	return true;
}
