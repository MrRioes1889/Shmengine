#include "Skybox.hpp"
#include "core/Identifier.hpp"
#include "core/FrameData.hpp"
#include "memory/LinearAllocator.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/ShaderSystem.hpp"

struct SkyBoxVertex
{
	Math::Vec3f position;
};

static void _skybox_geometry_init(GeometryData* out_geometry);

bool8 skybox_init(SkyboxConfig* config, Skybox* out_skybox)
{
	if (out_skybox->state >= ResourceState::Initialized)
		return false;
	
	out_skybox->state = ResourceState::Initializing;

	out_skybox->name = config->name;
	out_skybox->cubemap_name = config->cubemap_name;
	out_skybox->unique_id = 0;

	out_skybox->shader_instance_id.invalidate();
	out_skybox->unique_id = identifier_acquire_new_id(out_skybox);

	_skybox_geometry_init(&out_skybox->geometry);
	Renderer::geometry_load(&out_skybox->geometry);

	Texture* cube_texture = TextureSystem::acquire(out_skybox->cubemap_name.c_str(), TextureType::Cube, true);
	TextureMapConfig map_config = {};
	map_config.filter_minify = TextureFilter::LINEAR;
	map_config.filter_magnify = TextureFilter::LINEAR;
	map_config.repeat_u = map_config.repeat_v = map_config.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
	if (!Renderer::texture_map_init(&map_config, cube_texture, &out_skybox->cubemap))
	{
		SHMFATAL("Failed to acquire renderer resources for skybox cube map!");
		return false;
	}

	Shader* skybox_shader = ShaderSystem::get_shader(ShaderSystem::get_shader_id(Renderer::RendererConfig::builtin_shader_name_skybox));
	out_skybox->shader_instance_id = Renderer::shader_acquire_instance(skybox_shader);
	if (!out_skybox->shader_instance_id.is_valid())
	{
		SHMFATAL("Failed to acquire shader instance resources for skybox cube map!");
		return false;
	}

	out_skybox->state = ResourceState::Initialized;

	return true;
}

bool8 skybox_destroy(Skybox* skybox)
{
	if (skybox->state != ResourceState::Initialized)
		return false;

	skybox->state = ResourceState::Destroying;

	Shader* skybox_shader = ShaderSystem::get_shader(ShaderSystem::get_shader_id(Renderer::RendererConfig::builtin_shader_name_skybox));

	Renderer::shader_release_instance(skybox_shader, skybox->shader_instance_id);
	skybox->shader_instance_id = 0;
	TextureSystem::release(skybox->cubemap_name.c_str());
	Renderer::texture_map_destroy(&skybox->cubemap);
	Renderer::geometry_unload(&skybox->geometry);

	identifier_release_id(skybox->unique_id);
	skybox->unique_id = 0;

	skybox->name.free_data();
	skybox->cubemap_name.free_data();
	skybox->shader_instance_id.invalidate();
	skybox->state = ResourceState::Destroyed;
	return true;
}

static void _skybox_geometry_init(GeometryData* out_geometry)
{
	const Math::Vec3f cube_dim = { 10.0f, 10.0f, 10.0f };
	const Math::Vec3f cube_tiling = { 1.0f, 1.0f };
	const uint32 vertex_size = sizeof(SkyBoxVertex);
	const uint32 vertex_count = 4 * 6;
	const uint32 index_count = 6 * 6;

	SkyBoxVertex config_vertices[vertex_count];
	uint32 config_indices[index_count];

	GeometryConfig config = {};
	config.vertex_size = vertex_size;
	config.vertex_count = vertex_count;  // 4 verts per segment
	config.vertices = (byte*)config_vertices;
	config.index_count = index_count;  // 6 indices per segment
	config.indices = config_indices;

	float32 half_width = cube_dim.width * 0.5f;
	float32 half_height = cube_dim.height * 0.5f;
	float32 half_depth = cube_dim.depth * 0.5f;

	float32 min_x = -half_width;
	float32 min_y = -half_height;
	float32 min_z = -half_depth;
	float32 max_x = half_width;
	float32 max_y = half_height;
	float32 max_z = half_depth;

	config.extents.min.x = min_x;
	config.extents.min.y = min_y;
	config.extents.min.z = min_z;
	config.extents.max.x = max_x;
	config.extents.max.y = max_y;
	config.extents.max.z = max_z;

	config.center = VEC3_ZERO;

	SarrayRef<SkyBoxVertex> verts_ref(config_vertices, sizeof(config_vertices));

	// Front
	verts_ref[(0 * 4) + 0].position = { min_x, min_y, max_z };
	verts_ref[(0 * 4) + 1].position = { max_x, max_y, max_z };
	verts_ref[(0 * 4) + 2].position = { min_x, max_y, max_z };
	verts_ref[(0 * 4) + 3].position = { max_x, min_y, max_z };

	//Back
	verts_ref[(1 * 4) + 0].position = { max_x, min_y, min_z };
	verts_ref[(1 * 4) + 1].position = { min_x, max_y, min_z };
	verts_ref[(1 * 4) + 2].position = { max_x, max_y, min_z };
	verts_ref[(1 * 4) + 3].position = { min_x, min_y, min_z };

	//Left
	verts_ref[(2 * 4) + 0].position = { min_x, min_y, min_z };
	verts_ref[(2 * 4) + 1].position = { min_x, max_y, max_z };
	verts_ref[(2 * 4) + 2].position = { min_x, max_y, min_z };
	verts_ref[(2 * 4) + 3].position = { min_x, min_y, max_z };

	//Right
	verts_ref[(3 * 4) + 0].position = { max_x, min_y, max_z };
	verts_ref[(3 * 4) + 1].position = { max_x, max_y, min_z };
	verts_ref[(3 * 4) + 2].position = { max_x, max_y, max_z };
	verts_ref[(3 * 4) + 3].position = { max_x, min_y, min_z };

	//Bottom
	verts_ref[(4 * 4) + 0].position = { max_x, min_y, max_z };
	verts_ref[(4 * 4) + 1].position = { min_x, min_y, min_z };
	verts_ref[(4 * 4) + 2].position = { max_x, min_y, min_z };
	verts_ref[(4 * 4) + 3].position = { min_x, min_y, max_z };

	//Top
	verts_ref[(5 * 4) + 0].position = { min_x, max_y, max_z };
	verts_ref[(5 * 4) + 1].position = { max_x, max_y, min_z };
	verts_ref[(5 * 4) + 2].position = { min_x, max_y, min_z };
	verts_ref[(5 * 4) + 3].position = { max_x, max_y, max_z };

	SarrayRef<uint32> indices_ref(config_indices, sizeof(config_indices));
	for (uint32 i = 0; i < 6; ++i) 
	{
		uint32 v_offset = i * 4;
		uint32 i_offset = i * 6;
		indices_ref[i_offset + 0] = v_offset + 0;
		indices_ref[i_offset + 1] = v_offset + 1;
		indices_ref[i_offset + 2] = v_offset + 2;
		indices_ref[i_offset + 3] = v_offset + 0;
		indices_ref[i_offset + 4] = v_offset + 3;
		indices_ref[i_offset + 5] = v_offset + 1;
	}

	Renderer::geometry_init(&config, out_geometry);
}
