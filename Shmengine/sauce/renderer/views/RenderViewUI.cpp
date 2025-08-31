#include "RenderViewUI.hpp"

#include <core/Event.hpp>
#include <core/Identifier.hpp>
#include <core/FrameData.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <resources/UIText.hpp>

#include <optick.h>

struct RenderViewUIInternalData {
	ShaderId ui_shader_id;
	UIShaderUniformLocations ui_shader_u_locations;

	float32 near_clip;
	float32 far_clip;
	Math::Mat4 projection_matrix;
	Math::Mat4 view_matrix;
};

bool8 render_view_ui_on_create(RenderView* self)
{
		
	self->internal_data.init(sizeof(RenderViewUIInternalData), 0, AllocationTag::Renderer);
	RenderViewUIInternalData* internal_data = (RenderViewUIInternalData*)self->internal_data.data;

	Shader* ui_shader = 0;
	internal_data->ui_shader_id = ShaderSystem::acquire_shader_id(Renderer::RendererConfig::builtin_shader_name_ui, &ui_shader);
	if (ui_shader)
		Renderer::shader_init_from_resource(Renderer::RendererConfig::builtin_shader_name_ui, &self->renderpasses[0], ui_shader);

	internal_data->ui_shader_u_locations.projection = Renderer::shader_get_uniform_index(ui_shader, "projection");
	internal_data->ui_shader_u_locations.view = Renderer::shader_get_uniform_index(ui_shader, "view");
	internal_data->ui_shader_u_locations.diffuse_texture = Renderer::shader_get_uniform_index(ui_shader, "diffuse_texture");
	internal_data->ui_shader_u_locations.model = Renderer::shader_get_uniform_index(ui_shader, "model");
	internal_data->ui_shader_u_locations.properties = Renderer::shader_get_uniform_index(ui_shader, "properties");

	internal_data->near_clip = -100.0f;
	internal_data->far_clip = 100.0f;

	internal_data->projection_matrix = Math::mat_orthographic(0.0f, 1280.0f, 720.0f, 0.0f, internal_data->near_clip, internal_data->far_clip);
	internal_data->view_matrix = MAT4_IDENTITY;

	return true;

}

void render_view_ui_on_destroy(RenderView* self)
{
}

void render_view_ui_on_resize(RenderView* self, uint32 width, uint32 height)
{
	if (self->width == width && self->height == height)
		return;

	RenderViewUIInternalData* data = (RenderViewUIInternalData*)self->internal_data.data;

	self->width = (uint16)width;
	self->height = (uint16)height;
	data->projection_matrix = Math::mat_orthographic(0.0f, (float32)self->width, (float32)self->height, 0.0f, data->near_clip, data->far_clip);

	for (uint32 i = 0; i < self->renderpasses.capacity; i++)
	{
		self->renderpasses[i].dim.width = width;
		self->renderpasses[i].dim.height = height;
	}
}

bool8 render_view_ui_on_build_packet(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data)
{		
	RenderViewUIInternalData* internal_data = (RenderViewUIInternalData*)self->internal_data.data;

	return true;
}

void render_view_ui_on_end_frame(RenderView* self)
{
}

static bool8 set_globals_ui(RenderViewUIInternalData* internal_data)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->ui_shader_id);
	Renderer::shader_bind_globals(shader);

	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->ui_shader_u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->ui_shader_u_locations.view, &internal_data->view_matrix));

	return Renderer::shader_apply_globals(ShaderSystem::get_shader(internal_data->ui_shader_id));
}

static bool8 set_instance_ui(RenderViewUIInternalData* internal_data, RenderViewInstanceData instance)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->ui_shader_id);
	Renderer::shader_bind_instance(shader, instance.shader_instance_id);

	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->ui_shader_u_locations.properties, instance.instance_properties));
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->ui_shader_u_locations.diffuse_texture, instance.texture_maps[0]));

	return Renderer::shader_apply_instance(ShaderSystem::get_shader(internal_data->ui_shader_id));
}

static bool8 set_locals_ui(RenderViewUIInternalData* internal_data, Math::Mat4* model)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->ui_shader_id);
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->ui_shader_u_locations.model, model));

	return true;
}

bool8 render_view_ui_on_render(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
{

	OPTICK_EVENT();

	RenderViewUIInternalData* internal_data = (RenderViewUIInternalData*)self->internal_data.data;

	if (!set_globals_ui(internal_data))
		SHMERROR("Failed to apply globals to ui shader.");

	for (uint32 instance_i = 0; instance_i < self->instances.count; instance_i++)
	{
		RenderViewInstanceData* instance_data = &self->instances[instance_i];

		if (instance_data->shader_instance_id == Constants::max_u32)
			continue;

		bool8 instance_set = true;
		if (instance_data->shader_id == internal_data->ui_shader_id)
			instance_set = set_instance_ui(internal_data, *instance_data);
		else
			SHMERROR("Unknown shader for applying instance.");

		if (!instance_set)
			SHMERROR("Failed to apply instance.");
	}

	Renderer::RenderPass* renderpass = &self->renderpasses[0];

	if (!Renderer::renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
	{
		SHMERROR("render_view_ui_on_render - failed to begin renderpass!");
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

		if (render_data->object_index != Constants::max_u32)
		{
			Math::Mat4* model = &self->objects[render_data->object_index].model;
			set_locals_ui(internal_data, model);
		}

		Renderer::geometry_draw(render_data->geometry_data);
	}

	if (!renderpass_end(renderpass))
	{
		SHMERROR("render_view_ui_on_render - draw_frame - failed to end renderpass!");
		return false;
	}

	return true;
}
