#include "Skybox.hpp"
#include "core/Identifier.hpp"
#include "core/FrameData.hpp"
#include "memory/LinearAllocator.hpp"
#include "renderer/RendererFrontend.hpp"
#include "renderer/Geometry.hpp"
#include "systems/TextureSystem.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ShaderSystem.hpp"

bool8 skybox_init(SkyboxConfig* config, Skybox* out_skybox)
{
	if (out_skybox->state >= ResourceState::Initialized)
		return false;
	
	out_skybox->state = ResourceState::Initializing;

	out_skybox->name = config->name;
	out_skybox->cubemap_name = config->cubemap_name;
	out_skybox->unique_id = 0;

	out_skybox->cubemap.filter_minify = TextureFilter::LINEAR;
	out_skybox->cubemap.filter_magnify = TextureFilter::LINEAR;
	out_skybox->cubemap.repeat_u = out_skybox->cubemap.repeat_v = out_skybox->cubemap.repeat_w = TextureRepeat::CLAMP_TO_EDGE;

	out_skybox->shader_instance_id = Constants::max_u32;

	GeometryResourceData geo_resource = {};
	Renderer::generate_cube_geometry(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, out_skybox->name.c_str(), &geo_resource);
	GeometryConfig skybox_cube_config = Renderer::geometry_get_config_from_resource(&geo_resource);
	out_skybox->geometry_id = GeometrySystem::create_geometry(&skybox_cube_config, true);

	out_skybox->unique_id = identifier_acquire_new_id(out_skybox);

	Renderer::geometry_load(GeometrySystem::get_geometry_data(out_skybox->geometry_id));
	out_skybox->cubemap.texture = TextureSystem::acquire(out_skybox->cubemap_name.c_str(), TextureType::Cube, true);
	if (!Renderer::texture_map_acquire_resources(&out_skybox->cubemap))
	{
		SHMFATAL("Failed to acquire renderer resources for skybox cube map!");
		return false;
	}

	Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);
	if (!Renderer::shader_acquire_instance_resources(skybox_shader, 1, &out_skybox->shader_instance_id))
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

	Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);

	Renderer::shader_release_instance_resources(skybox_shader, skybox->shader_instance_id);
	skybox->shader_instance_id = 0;
	TextureSystem::release(skybox->cubemap_name.c_str());
	Renderer::texture_map_release_resources(&skybox->cubemap);
	Renderer::geometry_unload(GeometrySystem::get_geometry_data(skybox->geometry_id));

	identifier_release_id(skybox->unique_id);
	skybox->unique_id = 0;

	GeometrySystem::release(skybox->geometry_id);
	skybox->name.free_data();
	skybox->cubemap_name.free_data();
	skybox->geometry_id.invalidate();
	skybox->shader_instance_id = Constants::max_u32;
	skybox->state = ResourceState::Destroyed;
	return true;
}
