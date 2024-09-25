#include "Skybox.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/RendererGeometry.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ShaderSystem.hpp"

bool32 skybox_create(const char* cubemap_name, Skybox* out_skybox)
{
	TextureMap* cube_map = &out_skybox->cubemap;
	cube_map->filter_minify = TextureFilter::LINEAR;
	cube_map->filter_magnify = TextureFilter::LINEAR;
	cube_map->repeat_u = cube_map->repeat_v = cube_map->repeat_w = TextureRepeat::CLAMP_TO_EDGE;
	cube_map->use = TextureUse::MAP_CUBEMAP;
	if (!Renderer::texture_map_acquire_resources(cube_map))
	{
		SHMFATAL("Failed to acquire renderer resources for skybox cube map!");
		return false;
	}
	cube_map->texture = TextureSystem::acquire_cube("skybox", true);

	GeometrySystem::GeometryConfig skybox_cube_config = {};
	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, cubemap_name, 0, skybox_cube_config);
	skybox_cube_config.material_name[0] = 0;
	out_skybox->g = GeometrySystem::acquire_from_config(skybox_cube_config, true);
	out_skybox->renderer_frame_number = INVALID_ID64;
	Renderer::Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);
	TextureMap* maps[1] = { &out_skybox->cubemap };
	if (!Renderer::shader_acquire_instance_resources(skybox_shader, maps, &out_skybox->instance_id))
	{
		SHMFATAL("Failed to acquire shader instance resources for skybox cube map!");
		return false;
	}

	return true;
}

void skybox_destroy(Skybox* skybox)
{
	Renderer::texture_map_release_resources(&skybox->cubemap);
}
