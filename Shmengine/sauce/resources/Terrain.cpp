#include "Terrain.hpp"

#include "core/Memory.hpp"
#include "core/Logging.hpp"
#include "core/Identifier.hpp"
#include "core/FrameData.hpp"
#include "memory/LinearAllocator.hpp"
#include "renderer/Utility.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/loaders/TextureLoader.hpp"
#include "resources/loaders/TerrainLoader.hpp"

#include "systems/ShaderSystem.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/MaterialSystem.hpp"

bool8 terrain_init(TerrainConfig* config, Terrain* out_terrain)
{
    if (out_terrain->state >= ResourceState::Initialized)
        return false;

    if (!config->tile_count_x ||
        !config->tile_count_z ||
        !config->tile_scale_x ||
        !config->tile_scale_z)
    {
        SHMERROR("Failed to init terrain. Tile counts and scales have to be greater than 0.");
        out_terrain->state = ResourceState::Uninitialized;
        return false;
    }

    out_terrain->state = ResourceState::Initializing;
  
    out_terrain->name = config->name;
    out_terrain->xform = Math::transform_create();

	out_terrain->shader_instance_id.invalidate();
	out_terrain->unique_id = Constants::max_u32;

	out_terrain->material_properties = {};

    out_terrain->tile_count_x = config->tile_count_x;
    out_terrain->tile_count_z = config->tile_count_z;
    out_terrain->tile_scale_x = config->tile_scale_x;
    out_terrain->tile_scale_z = config->tile_scale_z;
    out_terrain->scale_y = config->scale_y;

    out_terrain->material_ids.init(config->materials_count, 0);
	for (uint32 i = 0; i < out_terrain->material_ids.capacity; i++)
		out_terrain->material_ids[i].invalidate();

	GeometryConfig geometry_config = {};
	geometry_config.extents = {};
	geometry_config.center = {};

	geometry_config.vertex_size = sizeof(TerrainVertex);

	if (config->heightmap_name)
	{
		TextureResourceData resource = {};
		if (!ResourceSystem::texture_loader_load(config->heightmap_name, false, &resource))
		{
			SHMERROR("Failed to load heightmap for terrain!");
			return false;
		}
		TextureConfig texture_config = ResourceSystem::texture_loader_get_config_from_resource(&resource);

		out_terrain->tile_count_x = texture_config.width - 1;
		out_terrain->tile_count_z = texture_config.height - 1;
		geometry_config.vertex_count = (out_terrain->tile_count_x + 1) * (out_terrain->tile_count_z + 1);
		out_terrain->vertex_infos.init(geometry_config.vertex_count, 0);

		for (uint32 i = 0; i < geometry_config.vertex_count; i++)
		{
			uint8 r = texture_config.pixels[(i * 4) + 0];
			float32 height = r / 255.0f;
			out_terrain->vertex_infos[i].height = height;
			if (height > geometry_config.extents.max.y)
				geometry_config.extents.max.y = height;
		}

		/*geometry_config.extents.max.y *= out_terrain->scale_y * 0.5f;
		geometry_config.extents.min.y = -geometry_config.extents.max.y;*/
		geometry_config.extents.max.y *= out_terrain->scale_y;
		geometry_config.extents.min.y = 0.0f;

		ResourceSystem::texture_loader_unload(&resource);
	}
	else
	{
		geometry_config.vertex_count = (out_terrain->tile_count_x + 1) * (out_terrain->tile_count_z + 1);
		out_terrain->vertex_infos.init(geometry_config.vertex_count, 0);
	}
	geometry_config.index_count = out_terrain->tile_count_x * out_terrain->tile_count_z * 6;

	geometry_config.extents.max.x = out_terrain->tile_count_x * out_terrain->tile_scale_x * 0.5f;
	geometry_config.extents.min.x = -geometry_config.extents.max.x;
	geometry_config.extents.max.z = out_terrain->tile_count_z * out_terrain->tile_scale_z * 0.5f;
	geometry_config.extents.min.z = -geometry_config.extents.max.z;
	
	Renderer::geometry_init(&geometry_config, &out_terrain->geometry);

	SarrayRef<TerrainVertex> vertices(&out_terrain->geometry.vertices);
	for (uint32 z = 0, i = 0; z < out_terrain->tile_count_z + 1; z++)
	{
		for (uint32 x = 0; x < out_terrain->tile_count_x + 1; x++, i++)
		{
			TerrainVertex* v = &vertices[i];
			v->position.x = x * out_terrain->tile_scale_x + geometry_config.extents.min.x;		
			v->position.y = out_terrain->vertex_infos[i].height * out_terrain->scale_y + geometry_config.extents.min.y;
			v->position.z = z * out_terrain->tile_scale_z + geometry_config.extents.min.z;

			v->color = { 1.0f, 1.0f, 1.0f, 1.0f };       // white;
			v->normal = { 0.0f, 1.0f, 0.0f };  // TODO: calculate based on geometry.
			v->tex_coords.x = (float32)x;
			v->tex_coords.y = (float32)z;

			float32 step_size = 1.0f / (float32)out_terrain->material_ids.capacity;
			v->material_weights[0] = 1.0f - smoothstep(0.0f, step_size, out_terrain->vertex_infos[i].height);
			for (uint32 w = 1; w < out_terrain->material_ids.capacity; w++)
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

			out_terrain->geometry.indices[i + 0] = v2;
			out_terrain->geometry.indices[i + 1] = v1;
			out_terrain->geometry.indices[i + 2] = v0;
			out_terrain->geometry.indices[i + 3] = v3;
			out_terrain->geometry.indices[i + 4] = v1;
			out_terrain->geometry.indices[i + 5] = v2;
		}
	}

    Renderer::geometry_generate_normals<TerrainVertex>(geometry_config.vertex_count, (TerrainVertex*)out_terrain->geometry.vertices.data,
        geometry_config.index_count, out_terrain->geometry.indices.data);
    Renderer::geometry_generate_tangents<TerrainVertex>(geometry_config.vertex_count, (TerrainVertex*)out_terrain->geometry.vertices.data,
        geometry_config.index_count, out_terrain->geometry.indices.data);

	out_terrain->unique_id = identifier_acquire_new_id(out_terrain);

    if (!Renderer::geometry_load(&out_terrain->geometry))
    {
        SHMERROR("Failed to load terrain geometry!");
        return false;
    }

	for (uint32 i = 0; i < out_terrain->material_ids.capacity; i++)
	{
		Material* material;
		out_terrain->material_ids[i] = MaterialSystem::acquire_material_id(config->material_names[i], &material);
		if (material)
			Renderer::material_init_from_resource_async(config->material_names[i], material);
	}

	out_terrain->material_properties.materials_count = out_terrain->material_ids.capacity;

	Material* default_material = MaterialSystem::get_default_material();

	// Phong properties and maps for each material.
	for (uint32 mat_i = 0; mat_i < Constants::max_terrain_materials_count; mat_i++) {
		// Properties.
		MaterialPhongProperties* mat_props = &out_terrain->material_properties.materials[mat_i];
		// Use default material unless within the material count.
		Material* sub_mat = default_material;
		if (mat_i < out_terrain->material_ids.capacity && sub_mat->maps.capacity >= 3)
			sub_mat = MaterialSystem::get_material(out_terrain->material_ids[mat_i]);
		if (sub_mat->state != ResourceState::Initialized)
			sub_mat = default_material;

		MaterialPhongProperties* sub_mat_properties = (MaterialPhongProperties*)sub_mat->properties;
		mat_props->diffuse_color = sub_mat_properties->diffuse_color;
		mat_props->shininess = sub_mat_properties->shininess;
	}

	Shader* terrain_shader = ShaderSystem::get_shader(ShaderSystem::get_terrain_shader_id());
	out_terrain->shader_instance_id = Renderer::shader_acquire_instance(terrain_shader);
	
	if (!out_terrain->shader_instance_id.is_valid())
		SHMERRORV("Failed to acquire renderer resources for terrain '%s'.", out_terrain->name.c_str());

    out_terrain->state = ResourceState::Initialized;

    return true;
}

bool8 terrain_init_from_resource(const char* resource_name, Terrain* out_terrain)
{
    TerrainResourceData resource = {};
    if (!ResourceSystem::terrain_loader_load(resource_name, &resource))
    {
        SHMERRORV("Failed to load terrain from resource '%s'", resource_name);
        out_terrain->state = ResourceState::Uninitialized;
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
    const char* submaterial_names[Constants::max_terrain_materials_count];
    for (uint32 i = 0; i < config.materials_count; i++)
        submaterial_names[i] = resource.sub_material_names[i].name;

    config.material_names = submaterial_names;

    terrain_init(&config, out_terrain);
    ResourceSystem::terrain_loader_unload(&resource);

    return true;
}

bool8 terrain_destroy(Terrain* terrain)
{
    if (terrain->state != ResourceState::Initialized)
        return false;

    terrain->state = ResourceState::Destroying;

	Shader* terrain_shader = ShaderSystem::get_shader(ShaderSystem::get_terrain_shader_id());
	Renderer::shader_release_instance(terrain_shader, terrain->shader_instance_id);
	terrain->shader_instance_id.invalidate();

	for (uint32 mat_i = 0; mat_i < terrain->material_ids.capacity; mat_i++)
	{
		Material* material = MaterialSystem::get_material(terrain->material_ids[mat_i]);
		if (material)
			MaterialSystem::release_material_id(material->name, &material);
		if (material)
			Renderer::material_destroy(material);

		terrain->material_ids[mat_i].invalidate();
	}

    Renderer::geometry_unload(&terrain->geometry);

	identifier_release_id(terrain->unique_id);
	terrain->unique_id = Constants::max_u32;

	Renderer::geometry_destroy(&terrain->geometry);
    terrain->vertex_infos.free_data();

    terrain->material_ids.free_data();

    terrain->name.free_data();

    terrain->state = ResourceState::Destroyed;
    return true;
}

bool8 terrain_update(Terrain* terrain)
{
    return true;
}
