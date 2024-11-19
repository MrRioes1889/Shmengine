#include "Skybox.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/RendererGeometry.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ShaderSystem.hpp"

bool32 skybox_init(SkyboxConfig* config, Skybox* out_skybox)
{
	if (out_skybox->state >= SkyboxState::INITIALIZED)
		return false;
	
	out_skybox->name = config->name;
	out_skybox->cubemap_name = config->cubemap_name;

	out_skybox->cubemap.filter_minify = TextureFilter::LINEAR;
	out_skybox->cubemap.filter_magnify = TextureFilter::LINEAR;
	out_skybox->cubemap.repeat_u = out_skybox->cubemap.repeat_v = out_skybox->cubemap.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
	out_skybox->cubemap.use = TextureUse::MAP_CUBEMAP;

	out_skybox->instance_id = INVALID_ID;

	GeometrySystem::GeometryConfig skybox_cube_config = {};
	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, out_skybox->name.c_str(), 0, skybox_cube_config);
	skybox_cube_config.material_name[0] = 0;
	out_skybox->g = GeometrySystem::acquire_from_config(&skybox_cube_config, true);

	out_skybox->state = SkyboxState::INITIALIZED;

	return true;
}

bool32 skybox_destroy(Skybox* skybox)
{
	if (skybox->state != SkyboxState::UNLOADED && !skybox_unload(skybox))
		return false;

	GeometrySystem::release(skybox->g);
	skybox->name.free_data();
	skybox->cubemap_name.free_data();
	skybox->g = 0;
	skybox->instance_id = INVALID_ID;
	skybox->state = SkyboxState::DESTROYED;
	return true;
}

bool32 skybox_load(Skybox* skybox)
{

	if (skybox->state != SkyboxState::INITIALIZED && skybox->state != SkyboxState::UNLOADED)
		return false;

	skybox->state = SkyboxState::LOADING;

	skybox->cubemap.texture = TextureSystem::acquire_cube(skybox->cubemap_name.c_str(), true);
	if (!Renderer::texture_map_acquire_resources(&skybox->cubemap))
	{
		SHMFATAL("Failed to acquire renderer resources for skybox cube map!");
		return false;
	}
	skybox->renderer_frame_number = INVALID_ID64;

	Renderer::Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);
	TextureMap* maps[1] = { &skybox->cubemap };
	if (!Renderer::shader_acquire_instance_resources(skybox_shader, maps, &skybox->instance_id))
	{
		SHMFATAL("Failed to acquire shader instance resources for skybox cube map!");
		return false;
	}

	skybox->state = SkyboxState::LOADED;

	return true;

}

bool32 skybox_unload(Skybox* skybox)
{

	if (skybox->state <= SkyboxState::INITIALIZED)
		return true;
	else if (skybox->state != SkyboxState::LOADED)
		return false;

	skybox->state = SkyboxState::UNLOADING;

	Renderer::Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);

	Renderer::shader_release_instance_resources(skybox_shader, skybox->instance_id);
	skybox->instance_id = 0;
	TextureSystem::release(skybox->cubemap_name.c_str());
	Renderer::texture_map_release_resources(&skybox->cubemap);

	skybox->state = SkyboxState::UNLOADED;

	return true;
}
