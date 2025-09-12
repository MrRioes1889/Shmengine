#include "Skybox.hpp"
#include "core/Identifier.hpp"
#include "core/FrameData.hpp"
#include "memory/LinearAllocator.hpp"
#include "renderer/RendererFrontend.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/ShaderSystem.hpp"

bool8 skybox_init(SkyboxConfig* config, Skybox* out_skybox)
{
	if (out_skybox->state >= ResourceState::Initialized)
		return false;
	
	out_skybox->state = ResourceState::Initializing;

	out_skybox->name = config->name;
	out_skybox->cubemap_name = config->cubemap_name;
	out_skybox->unique_id = 0;

	out_skybox->shader_instance_id.invalidate();

	GeometryConfig skybox_cube_config = {};
	skybox_cube_config.type = GeometryConfigType::Cube;
	skybox_cube_config.cube_config.dim = { 10.0f, 10.0f, 10.0f };
	skybox_cube_config.cube_config.tiling = { 1.0f, 1.0f };
	Renderer::geometry_init(&skybox_cube_config, &out_skybox->geometry);

	out_skybox->unique_id = identifier_acquire_new_id(out_skybox);

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
