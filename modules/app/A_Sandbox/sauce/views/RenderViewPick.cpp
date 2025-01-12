#include "RenderViewPick.hpp"

#include <core/Event.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <utility/Sort.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <resources/Mesh.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/CameraSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <resources/UIText.hpp>
#include <core/Input.hpp>

#include <optick.h>

struct PickShaderInstance
{
	bool8 is_dirty;
};

struct PickShaderInfo {
	Shader* shader;
	Darray<PickShaderInstance> instances;
};

struct RenderViewPickInternalData {
	PickShaderInfo material_phong_pick_shader;
	PickShaderInfo terrain_pick_shader;
	PickShaderInfo ui_pick_shader;

	Renderer::RenderPass* pass_3D;
	Renderer::RenderPass* pass_2D;

	uint16 id_color_location;
	uint16 model_location;
	uint16 projection_location;
	uint16 view_location;

	Math::Mat4 projection_3D;
	Math::Mat4 view_3D;
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

	uint32 geometries_3D_count;
	Renderer::ObjectRenderData* geometries_3D;

	uint32 geometries_2D_count;
	Renderer::ObjectRenderData* geometries_2D;
	
	UniqueId hovered_object_id;
};

static bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
{

	RenderView* self = (RenderView*)listener_inst;
	if (!self)
		return false;

	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;
	if (!internal_data)
		return false;

	switch (code)
	{
	case SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED:
	{
		RenderViewSystem::regenerate_render_targets(self);
		return false;
	}
	}

	return false;
}

static void acquire_shader_instances(const RenderView* self, PickShaderInfo* shader_info)
{
	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

	uint32 instance_id;
	if (!Renderer::shader_acquire_instance_resources(shader_info->shader, 0, 0, &instance_id))
	{
		SHMFATAL("Failed to acquire shader instance resources.");
		return;
	}

	shader_info->instances.emplace(true);
}

static void release_shader_instances(const RenderView* self)
{
	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

	for (uint32 i = 0; i < internal_data->material_phong_pick_shader.instances.count; i++)
		Renderer::shader_release_instance_resources(internal_data->material_phong_pick_shader.shader, i);

	for (uint32 i = 0; i < internal_data->terrain_pick_shader.instances.count; i++)
		Renderer::shader_release_instance_resources(internal_data->terrain_pick_shader.shader, i);

	for (uint32 i = 0; i < internal_data->ui_pick_shader.instances.count; i++)
		Renderer::shader_release_instance_resources(internal_data->ui_pick_shader.shader, i);

	internal_data->material_phong_pick_shader.instances.clear();
	internal_data->terrain_pick_shader.instances.clear();
	internal_data->ui_pick_shader.instances.clear();
}

bool32 render_view_pick_on_register(RenderView* self)
{

	self->internal_data.init(sizeof(RenderViewPickInternalData), 0, AllocationTag::RENDERER);
	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

	internal_data->material_phong_pick_shader.instances.init(64, 0);
	internal_data->terrain_pick_shader.instances.init(64, 0);
	internal_data->ui_pick_shader.instances.init(64, 0);

	internal_data->hovered_object_id = 0;

	internal_data->pass_3D = &self->renderpasses[0];
	internal_data->pass_2D = &self->renderpasses[1];

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_material_phong_pick, internal_data->pass_3D))
	{
		SHMERROR("Failed to create material phong pick shader.");
		return false;
	}

	internal_data->material_phong_pick_shader.shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_material_phong_pick);

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_terrain_pick, internal_data->pass_3D))
	{
		SHMERROR("Failed to create terrain pick shader.");
		return false;
	}

	internal_data->terrain_pick_shader.shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_terrain_pick);

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_ui_pick, internal_data->pass_2D))
	{
		SHMERROR("Failed to create world pick shader.");
		return false;
	}

	internal_data->ui_pick_shader.shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui_pick);

	// NOTE: Only retrieving uniform locations once, since shaders all use the same layout
	internal_data->id_color_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader.shader, "id_color");
	internal_data->model_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader.shader, "model");
	internal_data->projection_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader.shader, "projection");
	internal_data->view_location = ShaderSystem::get_uniform_index(internal_data->material_phong_pick_shader.shader, "view");

	internal_data->near_clip_3D = 0.1f;
	internal_data->far_clip_3D = 4000.0f;
	internal_data->fov_3D = Math::deg_to_rad(45.0f);
	internal_data->projection_3D = Math::mat_perspective(internal_data->fov_3D, 1280.0f / 720.0f, internal_data->near_clip_3D, internal_data->far_clip_3D);
	internal_data->view_3D = MAT4_IDENTITY;

	internal_data->near_clip_2D = -100.0f;
	internal_data->far_clip_2D = 100.0f;
	internal_data->fov_2D = 0.0f;
	internal_data->projection_2D = Math::mat_orthographic(0.0f, 1280.0f, 720.0f, 0.0f, internal_data->near_clip_2D, internal_data->far_clip_2D);
	internal_data->view_2D = MAT4_IDENTITY;

	Event::event_register((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);

	return true;

}

void render_view_pick_on_unregister(RenderView* self)
{
	RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;
	release_shader_instances(self);
	data->material_phong_pick_shader.instances.free_data();
	data->terrain_pick_shader.instances.free_data();
	data->ui_pick_shader.instances.free_data();

	Renderer::texture_destroy(&data->color_target_attachment_texture);
	Renderer::texture_destroy(&data->depth_target_attachment_texture);

	Event::event_unregister((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);		
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

bool32 render_view_pick_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data)
{
	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

	if (packet_data->renderpass_id + 1 > self->renderpasses.capacity)
	{
		SHMERROR("Invalid renderpass id supplied in packet data!");
		return false;
	}

	Camera* world_camera = CameraSystem::get_default_camera();
	internal_data->view_3D = world_camera->get_view();

	if (packet_data->renderpass_id == 0)
	{
		internal_data->geometries_3D_count = packet_data->geometries_count;
		internal_data->geometries_3D = packet_data->geometries;
	}
	else
	{
		internal_data->geometries_2D_count = packet_data->geometries_count;
		internal_data->geometries_2D = packet_data->geometries;
	}
		
	uint32 max_instance_id = 0;
	for (uint32 geo_i = 0; geo_i < packet_data->geometries_count; geo_i++)
	{
		if ((int32)packet_data->geometries[geo_i].unique_id > max_instance_id)
			max_instance_id = packet_data->geometries[geo_i].unique_id;
	}

	uint32 required_instances_count = max_instance_id + 1;

	if (required_instances_count <= internal_data->material_phong_pick_shader.instances.count)
		return true;
		
	uint32 diff = required_instances_count - internal_data->material_phong_pick_shader.instances.count;
	for (uint32 i = 0; i < diff; i++)
	{
		acquire_shader_instances(self, &internal_data->material_phong_pick_shader);
		acquire_shader_instances(self, &internal_data->terrain_pick_shader);
		acquire_shader_instances(self, &internal_data->ui_pick_shader);
	}

	return true;

}

void render_view_pick_on_end_frame(RenderView* self)
{
	self->geometries.clear();
}

bool32 render_view_pick_on_render(RenderView* self, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index)
{

	RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

	uint32 depth_pass_id = 0;
	uint32 ui_pass_id = 1;
	Renderer::RenderPass* depth_pass = &self->renderpasses[depth_pass_id];
	Renderer::RenderPass* ui_pass = &self->renderpasses[ui_pass_id];

	if (render_target_index == 0)
	{

		for (uint32 i = 0; i < internal_data->material_phong_pick_shader.instances.count; i++)
			internal_data->material_phong_pick_shader.instances[i] = { true };

		for (uint32 i = 0; i < internal_data->terrain_pick_shader.instances.count; i++)
			internal_data->terrain_pick_shader.instances[i] = { true };

		for (uint32 i = 0; i < internal_data->ui_pick_shader.instances.count; i++)
			internal_data->ui_pick_shader.instances[i] = { true };

		if (!Renderer::renderpass_begin(depth_pass, &depth_pass->render_targets[render_target_index]))
		{
			SHMERROR("Failed to begin renderpass!");
			return false;
		}

		int32 cur_instance_id = 0;

		uint32 shader_id = INVALID_ID;
		PickShaderInfo* pick_shader = 0;

		uint32 material_shader_id = ShaderSystem::get_material_shader_id();
		uint32 ui_shader_id = ShaderSystem::get_ui_shader_id();
		uint32 terrain_shader_id = ShaderSystem::get_terrain_shader_id();

		shader_id = INVALID_ID;

		for (uint32 geometry_i = 0; geometry_i < internal_data->geometries_3D_count; geometry_i++)
		{
			Renderer::ObjectRenderData* render_data = &internal_data->geometries_3D[geometry_i];

			if (render_data->shader_id != shader_id)
			{
				shader_id = render_data->shader_id;

				pick_shader = 0;
				if (shader_id == material_shader_id)
					pick_shader = &internal_data->material_phong_pick_shader;
				else if (shader_id == terrain_shader_id)
					pick_shader = &internal_data->terrain_pick_shader;
				else
					continue;

				ShaderSystem::use_shader(pick_shader->shader->id);

				ShaderSystem::set_uniform(internal_data->projection_location, &internal_data->projection_3D);
				ShaderSystem::set_uniform(internal_data->view_location, &internal_data->view_3D);
				Renderer::shader_apply_globals(pick_shader->shader);
			}

			if (pick_shader == 0)
				continue;

			cur_instance_id = render_data->unique_id;

			ShaderSystem::bind_instance(cur_instance_id);

			uint32 r, g, b = 0;
			Math::uint32_to_rgb(cur_instance_id, &r, &g, &b);
			Math::Vec3f id_color = Math::rgb_uint32_to_vec3(r, g, b);
			ShaderSystem::set_uniform(internal_data->id_color_location, &id_color);
			Renderer::shader_apply_instance(pick_shader->shader, pick_shader->instances[cur_instance_id].is_dirty);
			pick_shader->instances[cur_instance_id].is_dirty = false;

			ShaderSystem::set_uniform(internal_data->model_location, &render_data->model);

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

		shader_id = INVALID_ID;

		for (uint32 geometry_i = 0; geometry_i < internal_data->geometries_2D_count; geometry_i++)
		{
			Renderer::ObjectRenderData* render_data = &internal_data->geometries_2D[geometry_i];

			if (render_data->shader_id != shader_id)
			{
				shader_id = render_data->shader_id;

				pick_shader = 0;
				if (shader_id == ui_shader_id)
					pick_shader = &internal_data->ui_pick_shader;
				else
					continue;

				ShaderSystem::use_shader(pick_shader->shader->id);

				ShaderSystem::set_uniform(internal_data->projection_location, &internal_data->projection_2D);
				ShaderSystem::set_uniform(internal_data->view_location, &internal_data->view_2D);
				Renderer::shader_apply_globals(pick_shader->shader);
			}

			if (pick_shader == 0)
				continue;

			cur_instance_id = render_data->unique_id;

			ShaderSystem::bind_instance(cur_instance_id);

			uint32 r, g, b = 0;
			Math::uint32_to_rgb(cur_instance_id, &r, &g, &b);
			Math::Vec3f id_color = Math::rgb_uint32_to_vec3(r, g, b);
			ShaderSystem::set_uniform(internal_data->id_color_location, &id_color);
			Renderer::shader_apply_instance(pick_shader->shader, pick_shader->instances[cur_instance_id].is_dirty);
			pick_shader->instances[cur_instance_id].is_dirty = false;

			ShaderSystem::set_uniform(internal_data->model_location, &render_data->model);

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

	uint32 id = INVALID_ID;
	id = Math::rgb_to_uint32(pixel[0], pixel[1], pixel[2]);
	if ((id & 0xFFFFFF) == 0xFFFFFF)
		id = 0;

	if (internal_data->hovered_object_id != id)
	{
		EventData e_data;
		e_data.ui32[0] = id;
		Event::event_fire(SystemEventCode::OBJECT_HOVER_ID_CHANGED, 0, e_data);
		internal_data->hovered_object_id = id;
	}	*/

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

	attachment->texture->id = INVALID_ID;
	attachment->texture->type = TextureType::TYPE_2D;
	CString::copy(texture_name, attachment->texture->name, max_texture_name_length);
	attachment->texture->width = width;
	attachment->texture->height = height;
	attachment->texture->channel_count = 4;
	attachment->texture->generation = INVALID_ID;
	attachment->texture->flags |= TextureFlags::IS_WRITABLE | TextureFlags::IS_READABLE;
	attachment->texture->flags |= has_transparency ? TextureFlags::HAS_TRANSPARENCY : 0;
	if (attachment->type == Renderer::RenderTargetAttachmentType::DEPTH)
		attachment->texture->flags |= TextureFlags::IS_DEPTH;

	Renderer::texture_create_writable(attachment->texture);

	return true;
}