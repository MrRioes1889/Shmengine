#include "RenderViewSkybox.hpp"

#include <core/Event.hpp>
#include <core/Identifier.hpp>
#include <core/FrameData.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <resources/Skybox.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <renderer/Camera.hpp>
#include <utility/Sort.hpp>

#include <optick.h>

struct SkyboxShaderUniformLocations
{
	ShaderUniformId projection;
	ShaderUniformId view;
	ShaderUniformId cube_map;
};

struct RenderViewSkyboxInternalData 
{
	ShaderId skybox_shader_id;
	SkyboxShaderUniformLocations skybox_shader_u_locations;

	float32 near_clip;
	float32 far_clip;
	float32 fov;

	Math::Mat4 projection_matrix;
};

bool8 render_view_skybox_on_create(RenderView* self)
{

	self->internal_data.init(sizeof(RenderViewSkyboxInternalData), 0, AllocationTag::Renderer);
	RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;

	Shader* skybox_shader = 0;
	internal_data->skybox_shader_id = ShaderSystem::acquire_shader_id(Renderer::RendererConfig::builtin_shader_name_skybox, &skybox_shader);
	if (skybox_shader)
		Renderer::shader_init_from_resource(Renderer::RendererConfig::builtin_shader_name_skybox, &self->renderpasses[0], skybox_shader);

	internal_data->skybox_shader_u_locations.projection = Renderer::shader_get_uniform_index(skybox_shader, "projection");
	internal_data->skybox_shader_u_locations.view = Renderer::shader_get_uniform_index(skybox_shader, "view");
	internal_data->skybox_shader_u_locations.cube_map = Renderer::shader_get_uniform_index(skybox_shader, "cube_texture");

	internal_data->near_clip = 0.1f;
	internal_data->far_clip = 1000.0f;
	internal_data->fov = Math::deg_to_rad(45.0f);

	internal_data->projection_matrix = Math::mat_perspective(internal_data->fov, 1280.0f / 720.0f, internal_data->near_clip, internal_data->far_clip);

	return true;

}

void render_view_skybox_on_destroy(RenderView* self)
{
}

void render_view_skybox_on_resize(RenderView* self, uint32 width, uint32 height)
{
	if (self->width == width && self->height == height)
		return;

	RenderViewSkyboxInternalData* data = (RenderViewSkyboxInternalData*)self->internal_data.data;

	self->width = (uint16)width;
	self->height = (uint16)height;
	float32 aspect = (float32)width / (float32)height;
	data->projection_matrix = Math::mat_perspective(data->fov, aspect, data->near_clip, data->far_clip);

	for (uint32 i = 0; i < self->renderpasses.capacity; i++)
	{
		self->renderpasses[i].dim.width = width;
		self->renderpasses[i].dim.height = height;
	}
}

bool8 render_view_skybox_on_build_packet(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data)
{
	RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;
	
	return true;
}

void render_view_skybox_on_end_frame(RenderView* self)
{
}

static bool8 set_globals_skybox(RenderViewSkyboxInternalData* internal_data, Math::Mat4* view_matrix)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->skybox_shader_id);
	Renderer::shader_bind_globals(shader);

	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->skybox_shader_u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->skybox_shader_u_locations.view, view_matrix));

	return Renderer::shader_apply_globals(ShaderSystem::get_shader(internal_data->skybox_shader_id));
}

static bool8 set_instance_skybox(RenderViewSkyboxInternalData* internal_data, RenderViewInstanceData instance)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->skybox_shader_id);
	Renderer::shader_bind_instance(shader, instance.shader_instance_id);

	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->skybox_shader_u_locations.cube_map, instance.texture_maps[0]));

	return Renderer::shader_apply_instance(ShaderSystem::get_shader(internal_data->skybox_shader_id));
}

bool8 render_view_skybox_on_render(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
{
	OPTICK_EVENT();

	RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;
	Camera* camera = RenderViewSystem::get_bound_world_camera();
	if (!camera)
	{
		SHMERROR("Cannot render skybox without bound world camera!");
		return false;
	}

	Math::Mat4 view_matrix = {};
	if (self->geometries.count)
	{
		view_matrix = camera->get_view();
		view_matrix.data[12] = 0.0f;
		view_matrix.data[13] = 0.0f;
		view_matrix.data[14] = 0.0f;
	}
	
	if (!set_globals_skybox(internal_data, &view_matrix))
		SHMERROR("Failed to apply globals to skybox shader.");

	for (uint32 instance_i = 0; instance_i < self->instances.count; instance_i++)
	{	
		RenderViewInstanceData* instance_data = &self->instances[instance_i];

		if (instance_data->shader_instance_id == Constants::max_u32)
			continue;

		bool8 instance_set = true;
		if (instance_data->shader_id == internal_data->skybox_shader_id)
			instance_set = set_instance_skybox(internal_data, *instance_data);
		else
			SHMERROR("Unknown shader for applying instance.");

		if (!instance_set)
			SHMERROR("Failed to apply instance.");
	}

	Renderer::RenderPass* renderpass = &self->renderpasses[0];

	if (!Renderer::renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
	{
		SHMERROR("Failed to begin renderpass!");
		return false;
	}

	ShaderId shader_id = ShaderId::invalid_value;
	Shader* shader = 0;

	for (uint32 geometry_i = 0; geometry_i < self->geometries.count; geometry_i++)
	{
		RenderViewGeometryData* render_data = &self->geometries[geometry_i];
				
		if (render_data->shader_id != shader_id)
		{
			shader_id = render_data->shader_id;
			shader = ShaderSystem::get_shader(shader_id);
			Renderer::shader_use(shader);
			Renderer::shader_bind_globals(shader);
		}
			
		if (render_data->shader_instance_id != Constants::max_u32)
			Renderer::shader_bind_instance(shader, render_data->shader_instance_id);

		Renderer::geometry_draw(render_data->geometry_data);
	}

	if (!Renderer::renderpass_end(renderpass))
	{
		SHMERROR("render_view_skybox_on_render - draw_frame - failed to end renderpass!");
		return false;
	}

	return true;
}