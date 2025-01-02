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
	
	out_skybox->state = SkyboxState::INITIALIZING;

	out_skybox->name = config->name;
	out_skybox->cubemap_name = config->cubemap_name;

	out_skybox->cubemap.filter_minify = TextureFilter::LINEAR;
	out_skybox->cubemap.filter_magnify = TextureFilter::LINEAR;
	out_skybox->cubemap.repeat_u = out_skybox->cubemap.repeat_v = out_skybox->cubemap.repeat_w = TextureRepeat::CLAMP_TO_EDGE;

	out_skybox->shader_instance_id = INVALID_ID;

	GeometrySystem::GeometryConfig skybox_cube_config = {};
	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, out_skybox->name.c_str(), skybox_cube_config);
	out_skybox->geometry = GeometrySystem::acquire_from_config(&skybox_cube_config, true);

	out_skybox->state = SkyboxState::INITIALIZED;

	return true;
}

bool32 skybox_destroy(Skybox* skybox)
{
	if (skybox->state != SkyboxState::UNLOADED && !skybox_unload(skybox))
		return false;

	GeometrySystem::release(skybox->geometry);
	skybox->name.free_data();
	skybox->cubemap_name.free_data();
	skybox->geometry = 0;
	skybox->shader_instance_id = INVALID_ID;
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

	skybox->render_frame_number = INVALID_ID;

	Renderer::Shader* skybox_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_skybox);
	TextureMap* maps[1] = { &skybox->cubemap };
	if (!Renderer::shader_acquire_instance_resources(skybox_shader, 1, maps, &skybox->shader_instance_id))
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

	Renderer::shader_release_instance_resources(skybox_shader, skybox->shader_instance_id);
	skybox->shader_instance_id = 0;
	TextureSystem::release(skybox->cubemap_name.c_str());
	Renderer::texture_map_release_resources(&skybox->cubemap);

	skybox->state = SkyboxState::UNLOADED;

	return true;
}

bool32 skybox_on_render(uint32 shader_id, LightingInfo lighting, Math::Mat4* model, void* in_skybox, uint32 frame_number)
{
	Skybox* skybox = (Skybox*)in_skybox;

	ShaderSystem::bind_instance(skybox->shader_instance_id);

	if (shader_id == ShaderSystem::get_skybox_shader_id())
	{
		ShaderSystem::SkyboxShaderUniformLocations u_locations = ShaderSystem::get_skybox_shader_uniform_locations();

		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.cube_map, &skybox->cubemap));
	}
	else
	{
		SHMERRORV("Unknown shader id %u for rendering mesh. Skipping uniforms.", shader_id);
		return false;
	}

	bool32 needs_update = (skybox->render_frame_number != (uint32)frame_number);
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_apply_instance(ShaderSystem::get_shader(shader_id), needs_update));
	skybox->render_frame_number = frame_number;

	return true;
}
