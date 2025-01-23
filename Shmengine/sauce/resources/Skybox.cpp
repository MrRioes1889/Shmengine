#include "Skybox.hpp"
#include "core/Identifier.hpp"
#include "core/FrameData.hpp"
#include "memory/LinearAllocator.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/RendererGeometry.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ShaderSystem.hpp"

bool32 skybox_init(SkyboxConfig* config, Skybox* out_skybox)
{
	if (out_skybox->state >= ResourceState::INITIALIZED)
		return false;
	
	out_skybox->state = ResourceState::INITIALIZING;

	out_skybox->name = config->name;
	out_skybox->cubemap_name = config->cubemap_name;
	out_skybox->unique_id = 0;

	out_skybox->cubemap.filter_minify = TextureFilter::LINEAR;
	out_skybox->cubemap.filter_magnify = TextureFilter::LINEAR;
	out_skybox->cubemap.repeat_u = out_skybox->cubemap.repeat_v = out_skybox->cubemap.repeat_w = TextureRepeat::CLAMP_TO_EDGE;

	out_skybox->shader_instance_id = INVALID_ID;

	GeometrySystem::GeometryConfig skybox_cube_config = {};
	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, out_skybox->name.c_str(), skybox_cube_config);
	out_skybox->geometry = GeometrySystem::acquire_from_config(&skybox_cube_config, true);

	out_skybox->state = ResourceState::INITIALIZED;

	return true;
}

bool32 skybox_destroy(Skybox* skybox)
{
	if (skybox->state != ResourceState::UNLOADED && !skybox_unload(skybox))
		return false;

	GeometrySystem::release(skybox->geometry);
	skybox->name.free_data();
	skybox->cubemap_name.free_data();
	skybox->geometry = 0;
	skybox->shader_instance_id = INVALID_ID;
	skybox->state = ResourceState::DESTROYED;
	return true;
}

bool32 skybox_load(Skybox* skybox)
{

	if (skybox->state != ResourceState::INITIALIZED && skybox->state != ResourceState::UNLOADED)
		return false;

	skybox->state = ResourceState::LOADING;
	skybox->unique_id = identifier_acquire_new_id(skybox);

	skybox->cubemap.texture = TextureSystem::acquire_cube(skybox->cubemap_name.c_str(), true);
	if (!Renderer::texture_map_acquire_resources(&skybox->cubemap))
	{
		SHMFATAL("Failed to acquire renderer resources for skybox cube map!");
		return false;
	}

	Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);
	if (!Renderer::shader_acquire_instance_resources(skybox_shader, 1, &skybox->shader_instance_id))
	{
		SHMFATAL("Failed to acquire shader instance resources for skybox cube map!");
		return false;
	}

	skybox->state = ResourceState::LOADED;

	return true;

}

bool32 skybox_unload(Skybox* skybox)
{

	if (skybox->state <= ResourceState::INITIALIZED)
		return true;
	else if (skybox->state != ResourceState::LOADED)
		return false;

	skybox->state = ResourceState::UNLOADING;

	Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);

	Renderer::shader_release_instance_resources(skybox_shader, skybox->shader_instance_id);
	skybox->shader_instance_id = 0;
	TextureSystem::release(skybox->cubemap_name.c_str());
	Renderer::texture_map_release_resources(&skybox->cubemap);

	identifier_release_id(skybox->unique_id);
	skybox->unique_id = 0;
	skybox->state = ResourceState::UNLOADED;

	return true;
}


