#include "RenderViewPick.hpp"

#include <core/Event.hpp>
#include <core/FrameData.hpp>
#include <core/Input.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <utility/Sort.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <resources/Mesh.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <renderer/Camera.hpp>
#include <resources/UIText.hpp>

struct RenderViewPickInternalData {
	Shader* material_phong_pick_shader;
	Shader* terrain_pick_shader;
	Shader* ui_pick_shader;

	Renderer::RenderPass* pass_3D;
	Renderer::RenderPass* pass_2D;

	ShaderUniformId id_color_location;
	ShaderUniformId model_location;
	ShaderUniformId projection_location;
	ShaderUniformId view_location;

	Math::Mat4 projection_3D;
	float32 near_clip_3D;
	float32 far_clip_3D;
	float32 fov_3D;

	Math::Mat4 projection_2D;
	Math::Mat4 view_2D;
	float32 near_clip_2D;
	float32 far_clip_2D;
	float32 fov_2D;

	Texture color_target_attachment_texture;
	Texture depth_target_attachment_texture;

	RenderView* world_view;
	RenderView* ui_view;

	UniqueId hovered_object_id;
};

bool32 render_view_pick_on_create(RenderView* self)
{

	self->internal_data.init(sizeof(RenderViewPickInternalData), 0, AllocationTag::RENDERER);
	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

	internal_data->world_view = RenderViewSystem::get("Builtin.World");
	internal_data->ui_view = RenderViewSystem::get("Builtin.UI");

	internal_data->hovered_object_id = 0;

	internal_data->pass_3D = &self->renderpasses[0];
	internal_data->pass_2D = &self->renderpasses[1];

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_material_phong_pick, internal_data->pass_3D))
	{
		SHMERROR("Failed to create material phong pick shader.");
		return false;
	}

	internal_data->material_phong_pick_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_material_phong_pick);

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_terrain_pick, internal_data->pass_3D))
	{
		SHMERROR("Failed to create terrain pick shader.");
		return false;
	}

	internal_data->terrain_pick_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_terrain_pick);

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_ui_pick, internal_data->pass_2D))
	{
		SHMERROR("Failed to create world pick shader.");
		return false;
	}

	internal_data->ui_pick_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui_pick);

	// NOTE: Only retrieving uniform locations once, since shaders all use the same layout
	internal_data->id_color_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader, "id_color");
	internal_data->model_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader, "model");
	internal_data->projection_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader, "projection");
	internal_data->view_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader, "view");

	internal_data->near_clip_3D = 0.1f;
	internal_data->far_clip_3D = 4000.0f;
	internal_data->fov_3D = Math::deg_to_rad(45.0f);
	internal_data->projection_3D = Math::mat_perspective(internal_data->fov_3D, 1280.0f / 720.0f, internal_data->near_clip_3D, internal_data->far_clip_3D);

	internal_data->near_clip_2D = -100.0f;
	internal_data->far_clip_2D = 100.0f;
	internal_data->fov_2D = 0.0f;
	internal_data->projection_2D = Math::mat_orthographic(0.0f, 1280.0f, 720.0f, 0.0f, internal_data->near_clip_2D, internal_data->far_clip_2D);
	internal_data->view_2D = MAT4_IDENTITY;

	return true;

}

void render_view_pick_on_destroy(RenderView* self)
{
	RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

	Renderer::texture_destroy(&data->color_target_attachment_texture);
	Renderer::texture_destroy(&data->depth_target_attachment_texture);
}

void render_view_pick_on_resize(RenderView* self, uint32 width, uint32 height)
{
	if (self->width == width && self->height == height)
		return;

	RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

	self->width = (uint16)width;
	self->height = (uint16)height;

	data->projection_2D = Math::mat_orthographic(0.0f, self->width, self->height, 0.0f, data->near_clip_2D, data->far_clip_2D);

	float32 aspect = (float32)width / (float32)height;
	data->projection_3D = Math::mat_perspective(data->fov_3D, aspect, data->near_clip_3D, data->far_clip_3D);

	for (uint32 i = 0; i < self->renderpasses.capacity; i++)
	{
		self->renderpasses[i].dim.width = width;
		self->renderpasses[i].dim.height = height;
	}
}

static bool32 set_globals_material_phong_pick(RenderViewPickInternalData* internal_data, Camera* camera)
{
	ShaderSystem::bind_shader(internal_data->material_phong_pick_shader->id);
	ShaderSystem::bind_globals();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->projection_location, &internal_data->projection_3D));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->view_location, &camera->get_view()));

	return Renderer::shader_apply_globals(internal_data->material_phong_pick_shader);
}

static bool32 set_globals_terrain_pick(RenderViewPickInternalData* internal_data, Camera* camera)
{
	ShaderSystem::bind_shader(internal_data->terrain_pick_shader->id);
	ShaderSystem::bind_globals();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->projection_location, &internal_data->projection_3D));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->view_location, &camera->get_view()));

	return Renderer::shader_apply_globals(internal_data->terrain_pick_shader);
}

static bool32 set_globals_ui_pick(RenderViewPickInternalData* internal_data)
{
	ShaderSystem::bind_shader(internal_data->ui_pick_shader->id);
	ShaderSystem::bind_globals();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->projection_location, &internal_data->projection_2D));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->view_location, &internal_data->view_2D));

	return Renderer::shader_apply_globals(internal_data->ui_pick_shader);
}

bool32 render_view_pick_on_build_packet(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data)
{
	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

	return true;
}

void render_view_pick_on_end_frame(RenderView* self)
{
}

bool32 render_view_pick_on_render(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
{

	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;
	Camera* world_camera = RenderViewSystem::get_bound_world_camera();

	uint32 depth_pass_id = 0;
	uint32 ui_pass_id = 1;
	Renderer::RenderPass* depth_pass = &self->renderpasses[depth_pass_id];
	Renderer::RenderPass* ui_pass = &self->renderpasses[ui_pass_id];

	if (render_target_index == 0)
	{

		if (!set_globals_material_phong_pick(internal_data, world_camera))
			SHMERROR("Failed to apply globals to material phong pick shader.");
		if (!set_globals_terrain_pick(internal_data, world_camera))
			SHMERROR("Failed to apply globals to terrain pick shader.");
		if (!set_globals_ui_pick(internal_data))
			SHMERROR("Failed to apply globals to ui pick shader.");

		if (!Renderer::renderpass_begin(depth_pass, &depth_pass->render_targets[render_target_index]))
		{
			SHMERROR("Failed to begin renderpass!");
			return false;
		}

		uint32 material_phong_shader_id = ShaderSystem::get_material_phong_shader_id();
		uint32 ui_shader_id = ShaderSystem::get_ui_shader_id();
		uint32 terrain_shader_id = ShaderSystem::get_terrain_shader_id();

		uint32 shader_id = Constants::max_u32;
		Shader* pick_shader = 0;

		for (uint32 geometry_i = 0; geometry_i < internal_data->world_view->geometries.count; geometry_i++)
		{
			RenderViewGeometryData* render_data = &internal_data->world_view->geometries[geometry_i];

			if (render_data->object_index == Constants::max_u32)
				continue;

			if (render_data->shader_id != shader_id)
			{
				shader_id = render_data->shader_id;

				pick_shader = 0;
				if (shader_id == material_phong_shader_id)
					pick_shader = internal_data->material_phong_pick_shader;
				else if (shader_id == terrain_shader_id)
					pick_shader = internal_data->terrain_pick_shader;
				else
					continue;

				ShaderSystem::use_shader(pick_shader->id);
				ShaderSystem::bind_globals();
			}

			if (pick_shader == 0)
				continue;

			RenderViewObjectData* object_data = &internal_data->world_view->objects[render_data->object_index];

			uint32 r, g, b = 0;
			Math::uint32_to_rgb(object_data->unique_id, &r, &g, &b);
			Math::Vec3f id_color = Math::rgb_uint32_to_vec3(r, g, b);
			ShaderSystem::set_uniform(internal_data->id_color_location, &id_color);
			ShaderSystem::set_uniform(internal_data->model_location, &object_data->model);

			Renderer::geometry_draw(render_data->geometry_data);
		}

		if (!renderpass_end(depth_pass))
		{
			SHMERROR("Failed to end renderpass!");
			return false;
		}


		if (!renderpass_begin(ui_pass, &ui_pass->render_targets[render_target_index]))
		{
			SHMERROR("Failed to begin renderpass!");
			return false;
		}

		shader_id = Constants::max_u32;
		pick_shader = 0;

		for (uint32 geometry_i = 0; geometry_i < internal_data->ui_view->geometries.count; geometry_i++)
		{
			RenderViewGeometryData* render_data = &internal_data->ui_view->geometries[geometry_i];

			if (render_data->object_index == Constants::max_u32)
				continue;

			if (render_data->shader_id != shader_id)
			{
				shader_id = render_data->shader_id;

				pick_shader = 0;
				if (shader_id == ui_shader_id)
					pick_shader = internal_data->ui_pick_shader;
				else
					continue;

				ShaderSystem::use_shader(pick_shader->id);
				ShaderSystem::bind_globals();
			}

			if (pick_shader == 0)
				continue;

			RenderViewObjectData* object_data = &internal_data->ui_view->objects[render_data->object_index];

			uint32 r, g, b = 0;
			Math::uint32_to_rgb(object_data->unique_id, &r, &g, &b);
			Math::Vec3f id_color = Math::rgb_uint32_to_vec3(r, g, b);
			ShaderSystem::set_uniform(internal_data->id_color_location, &id_color);
			ShaderSystem::set_uniform(internal_data->model_location, &object_data->model);

			Renderer::geometry_draw(render_data->geometry_data);
		}

		if (!renderpass_end(ui_pass))
		{
			SHMERROR("Failed to end renderpass!");
			return false;
		}

	}

	/*Texture* t = &internal_data->color_target_attachment_texture;

	uint8 pixel[4] = {};
	Math::Vec2i mouse_pos = Input::get_mouse_position();
	uint32 mouse_x = (uint32)clamp(mouse_pos.x, 0, self->width - 1);
	uint32 mouse_y = (uint32)clamp(mouse_pos.y, 0, self->height - 1);	
	Renderer::texture_read_pixel(t, mouse_x, mouse_y, (uint32*)&pixel);

	uint32 id = Constants::max_u32;
	id = Math::rgb_to_uint32(pixel[0], pixel[1], pixel[2]);
	if ((id & 0xFFFFFF) == 0xFFFFFF)
		id = 0;

	if (internal_data->hovered_object_id != id)
	{
		EventData e_data;
		e_data.ui32[0] = id;
		Event::event_fire(SystemEventCode::OBJECT_HOVER_ID_CHANGED, 0, e_data);
		internal_data->hovered_object_id = id;
	}*/

	return true;

}

bool32 render_view_pick_regenerate_attachment_target(const RenderView* self, uint32 pass_index, Renderer::RenderTargetAttachment* attachment)
{
	RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

	if (attachment->type == Renderer::RenderTargetAttachmentType::COLOR)
		attachment->texture = &data->color_target_attachment_texture;
	else if (attachment->type == Renderer::RenderTargetAttachmentType::DEPTH)
		attachment->texture = &data->depth_target_attachment_texture;
	else
	{
		SHMERROR("Unsupported attachment type!");
		return false;
	}

	if (pass_index == 1)
		return true;

	if (attachment->texture->internal_data.data)
		Renderer::texture_destroy(attachment->texture);

	static uint32 texture_i = 0;
	char texture_name[64] = "__pick_view_texture_";
	CString::append(texture_name, 64, CString::to_string(texture_i++));

	uint32 width = self->renderpasses[pass_index].dim.width;
	uint32 height = self->renderpasses[pass_index].dim.height;
	bool8 has_transparency = false;

	attachment->texture->id = Constants::max_u32;
	attachment->texture->type = TextureType::TYPE_2D;
	CString::copy(texture_name, attachment->texture->name, Constants::max_texture_name_length);
	attachment->texture->width = width;
	attachment->texture->height = height;
	attachment->texture->channel_count = 4;
	attachment->texture->generation = Constants::max_u32;
	attachment->texture->flags |= TextureFlags::IS_WRITABLE | TextureFlags::IS_READABLE;
	attachment->texture->flags |= has_transparency ? TextureFlags::HAS_TRANSPARENCY : 0;
	if (attachment->type == Renderer::RenderTargetAttachmentType::DEPTH)
		attachment->texture->flags |= TextureFlags::IS_DEPTH;

	Renderer::texture_create_writable(attachment->texture);

	return true;
}