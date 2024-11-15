#include "Skybox.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/RendererGeometry.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ShaderSystem.hpp"

bool32 skybox_create(SkyboxConfig* config, Skybox* out_skybox)
{
	
	out_skybox->cubemap_name = config->cubemap_name;

	return true;
}

void skybox_destroy(Skybox* skybox)
{
	skybox_unload(skybox);

	skybox->g = 0;
	skybox->instance_id = INVALID_ID;
	skybox->cubemap_name = 0;
}

bool32 skybox_init(Skybox* skybox)
{

	skybox->cubemap.filter_minify = TextureFilter::LINEAR;
	skybox->cubemap.filter_magnify = TextureFilter::LINEAR;
	skybox->cubemap.repeat_u = skybox->cubemap.repeat_v = skybox->cubemap.repeat_w = TextureRepeat::CLAMP_TO_EDGE;
	skybox->cubemap.use = TextureUse::MAP_CUBEMAP;

	skybox->instance_id = INVALID_ID;

	GeometrySystem::GeometryConfig skybox_cube_config = {};
	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, skybox->cubemap_name, 0, skybox_cube_config);
	skybox_cube_config.material_name[0] = 0;
	skybox->g = GeometrySystem::acquire_from_config(&skybox_cube_config, true);

	return true;
}

bool32 skybox_load(Skybox* skybox)
{

	if (!Renderer::texture_map_acquire_resources(&skybox->cubemap))
	{
		SHMFATAL("Failed to acquire renderer resources for skybox cube map!");
		return false;
	}
	skybox->cubemap.texture = TextureSystem::acquire_cube(skybox->cubemap_name, true);
	skybox->renderer_frame_number = INVALID_ID64;

	Renderer::Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);
	TextureMap* maps[1] = { &skybox->cubemap };
	if (!Renderer::shader_acquire_instance_resources(skybox_shader, maps, &skybox->instance_id))
	{
		SHMFATAL("Failed to acquire shader instance resources for skybox cube map!");
		return false;
	}

	return true;

}

bool32 skybox_unload(Skybox* skybox)
{

	Renderer::Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);

	Renderer::shader_release_instance_resources(skybox_shader, skybox->instance_id);
	skybox->instance_id = 0;
	TextureSystem::release(skybox->cubemap_name);
	Renderer::texture_map_release_resources(&skybox->cubemap);
	GeometrySystem::release(skybox->g);


	return true;
}
