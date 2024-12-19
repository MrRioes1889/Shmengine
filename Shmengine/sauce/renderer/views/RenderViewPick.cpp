#include "RenderViewPick.hpp"

#include "core/Event.hpp"
#include "utility/Math.hpp"
#include "utility/math/Transform.hpp"
#include "resources/loaders/ShaderLoader.hpp"
#include "resources/Mesh.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/CameraSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/UIText.hpp"
#include "core/Input.hpp"

struct RenderViewPickShaderInfo {
	Renderer::Shader* shader;
	Renderer::RenderPass* pass;

	uint16 id_color_location;
	uint16 model_location;
	uint16 projection_location;
	uint16 view_location;

	Math::Mat4 projection;
	Math::Mat4 view;
	float32 near_clip;
	float32 far_clip;
	float32 fov;
};

struct RenderViewPickShaderInstance
{
	bool8 is_dirty;
};

struct RenderViewPickInternalData {
	RenderViewPickShaderInfo world_shader_info;
	RenderViewPickShaderInfo ui_shader_info;

	Texture color_target_attachment_texture;
	Texture depth_target_attachment_texture;

	//Math::Vec2i mouse_pos;
	Darray<RenderViewPickShaderInstance> instances;
};

namespace Renderer
{

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

	static void acquire_shader_instances(const RenderView* self)
	{
		RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

		uint32 instance_id;
		if (!Renderer::shader_acquire_instance_resources(data->ui_shader_info.shader, 0, 0, &instance_id))
		{
			SHMFATAL("Failed to acquire shader instance resources.");
			return;
		}

		if (!Renderer::shader_acquire_instance_resources(data->world_shader_info.shader, 0, 0, &instance_id))
		{
			SHMFATAL("Failed to acquire shader instance resources.");
			return;
		}

		data->instances.push({ true });
	}

	static void release_shader_instances(const RenderView* self)
	{
		RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

		for (uint32 i = 0; i < data->instances.count; i++)
		{
			Renderer::shader_release_instance_resources(data->ui_shader_info.shader, i);
			Renderer::shader_release_instance_resources(data->world_shader_info.shader, i);
			data->instances.pop();
		}
	}

	bool32 render_view_pick_on_create(RenderView* self)
	{

		self->internal_data.init(sizeof(RenderViewPickInternalData), 0, AllocationTag::RENDERER);
		RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

		data->instances.init(64, 0);

		data->world_shader_info.pass = &self->renderpasses[0];
		data->ui_shader_info.pass = &self->renderpasses[1];	

		ShaderConfig world_pick_config = {};
		if (!ResourceSystem::shader_loader_load(Renderer::RendererConfig::builtin_shader_name_world_pick, 0, &world_pick_config))
		{
			SHMERROR("Failed to load world pick shader config.");
			return false;
		}	

		if (!ShaderSystem::create_shader(&self->renderpasses[0], &world_pick_config))
		{
			SHMERROR("Failed to create world pick shader.");
			ResourceSystem::shader_loader_unload(&world_pick_config);
			return false;
		}
		ResourceSystem::shader_loader_unload(&world_pick_config);

		data->world_shader_info.shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_world_pick);

		data->world_shader_info.id_color_location = ShaderSystem::get_uniform_index(data->world_shader_info.shader, "id_color");
		data->world_shader_info.model_location = ShaderSystem::get_uniform_index(data->world_shader_info.shader, "model");
		data->world_shader_info.projection_location = ShaderSystem::get_uniform_index(data->world_shader_info.shader, "projection");
		data->world_shader_info.view_location = ShaderSystem::get_uniform_index(data->world_shader_info.shader, "view");

		data->world_shader_info.near_clip = 0.1f;
		data->world_shader_info.far_clip = 1000.0f;
		data->world_shader_info.fov = Math::deg_to_rad(45.0f);
		data->world_shader_info.projection = Math::mat_perspective(data->world_shader_info.fov, 1280.0f / 720.0f, data->world_shader_info.near_clip, data->world_shader_info.far_clip);
		data->world_shader_info.view = MAT4_IDENTITY;


		ShaderConfig ui_pick_config = {};
		if (!ResourceSystem::shader_loader_load(Renderer::RendererConfig::builtin_shader_name_ui_pick, 0, &ui_pick_config))
		{
			SHMERROR("Failed to load ui pick shader config.");
			return false;
		}

		if (!ShaderSystem::create_shader(&self->renderpasses[1], &ui_pick_config))
		{
			SHMERROR("Failed to create ui pick shader.");
			ResourceSystem::shader_loader_unload(&ui_pick_config);
			return false;
		}
		ResourceSystem::shader_loader_unload(&ui_pick_config);

		data->ui_shader_info.shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_ui_pick);

		data->ui_shader_info.id_color_location = ShaderSystem::get_uniform_index(data->ui_shader_info.shader, "id_color");
		data->ui_shader_info.model_location = ShaderSystem::get_uniform_index(data->ui_shader_info.shader, "model");
		data->ui_shader_info.projection_location = ShaderSystem::get_uniform_index(data->ui_shader_info.shader, "projection");
		data->ui_shader_info.view_location = ShaderSystem::get_uniform_index(data->ui_shader_info.shader, "view");

		data->ui_shader_info.near_clip = -100.0f;
		data->ui_shader_info.far_clip = 100.0f;
		data->ui_shader_info.fov = 0.0f;
		data->ui_shader_info.projection = Math::mat_orthographic(0.0f, 1280.0f, 720.0f, 0.0f, data->ui_shader_info.near_clip, data->ui_shader_info.far_clip);
		data->ui_shader_info.view = MAT4_IDENTITY;

		Event::event_register((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);

		return true;

	}

	void render_view_pick_on_destroy(RenderView* self)
	{
		RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;
		release_shader_instances(self);
		data->instances.free_data();

		Renderer::texture_destroy(&data->color_target_attachment_texture);
		Renderer::texture_destroy(&data->depth_target_attachment_texture);

		self->internal_data.free_data();
		Event::event_unregister((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);		
	}

	void render_view_pick_on_resize(RenderView* self, uint32 width, uint32 height)
	{
		if (self->width == width && self->height == height)
			return;

		RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

		self->width = (uint16)width;
		self->height = (uint16)height;

		data->ui_shader_info.projection = Math::mat_orthographic(0.0f, self->width, self->height, 0.0f, data->ui_shader_info.near_clip, data->ui_shader_info.far_clip);

		float32 aspect = (float32)width / (float32)height;
		data->world_shader_info.projection = Math::mat_perspective(data->world_shader_info.fov, aspect, data->world_shader_info.near_clip, data->world_shader_info.far_clip);

		for (uint32 i = 0; i < self->renderpasses.capacity; i++)
		{
			self->renderpasses[i].dim.width = width;
			self->renderpasses[i].dim.height = height;
		}
	}

	bool32 render_view_pick_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, void* data, RenderViewPacket* out_packet)
	{
		RenderViewPickInternalData* internal_data = (RenderViewPickInternalData*)self->internal_data.data;

		out_packet->extended_data = frame_allocator->allocate(sizeof(PickPacketData));
		Memory::copy_memory(data, out_packet->extended_data, sizeof(PickPacketData));
		PickPacketData* packet_data = (PickPacketData*)out_packet->extended_data;

		uint32 total_geometry_count = 0;
		total_geometry_count += packet_data->world_geometries_count;
		for (uint32 i = 0; i < packet_data->ui_mesh_data.mesh_count; i++)
			total_geometry_count += packet_data->ui_mesh_data.meshes[i]->geometries.count;

		out_packet->geometries.init(total_geometry_count, 0, AllocationTag::RENDERER);
		out_packet->view = self;

		Camera* world_camera = CameraSystem::get_default_camera();
		internal_data->world_shader_info.view = world_camera->get_view();

		packet_data->world_geometries_count = 0;
		packet_data->ui_geometries_count = 0;
		
		int32 max_instance_id = 0;
		for (uint32 i = 0; i < packet_data->world_geometries_count; i++)
		{
			out_packet->geometries.emplace(packet_data->world_geometries[i]);

			if ((int32)packet_data->world_geometries[i].unique_id > max_instance_id)
				max_instance_id = packet_data->world_geometries[i].unique_id;
		}

		for (uint32 i = 0; i < packet_data->ui_mesh_data.mesh_count; i++)
		{
			Mesh* m = packet_data->ui_mesh_data.meshes[i];
			for (uint32 j = 0; j < m->geometries.count; j++)
			{
				GeometryRenderData* render_data = out_packet->geometries.emplace();
				render_data->geometry = m->geometries[j].g_data;
				render_data->material = m->geometries[j].material;
				render_data->model = Math::transform_get_world(m->transform);
				render_data->unique_id = m->unique_id;
				packet_data->ui_geometries_count++;
			}

			if ((int32)m->unique_id > max_instance_id)
				max_instance_id = m->unique_id;
		}

		for (uint32 i = 0; i < packet_data->text_count; i++)
		{
			if ((int32)packet_data->texts[i]->unique_id > max_instance_id)
				max_instance_id = packet_data->texts[i]->unique_id;
		}

		uint32 required_instances_count = max_instance_id + 1;

		if (required_instances_count <= internal_data->instances.count)
			return true;
		
		uint32 diff = required_instances_count - internal_data->instances.count;
		for (uint32 i = 0; i < diff; i++)
			acquire_shader_instances(self);
		

		return true;

	}

	void render_view_pick_on_destroy_packet(const RenderView* self, RenderViewPacket* packet)
	{
		packet->geometries.free_data();
		packet->extended_data = 0;
	}

	bool32 render_view_pick_on_render(RenderView* self, const RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index)
	{

		RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

		uint32 pass_i = 0;
		RenderPass* pass = &self->renderpasses[pass_i];

		if (render_target_index == 0)
		{

			for (uint32 i = 0; i < data->instances.count; i++)
				data->instances[i] = { true };

			if (!renderpass_begin(pass, &pass->render_targets[render_target_index]))
			{
				SHMERROR("render_view_pick_on_render - failed to begin renderpass!");
				return false;
			}

			PickPacketData* packet_data = (PickPacketData*)packet.extended_data;

			int32 cur_instance_id = 0;

			if (!ShaderSystem::use_shader(data->world_shader_info.shader->id))
			{
				SHMERROR("render_view_pick_on_render - Failed to use shader. Render frame failed.");
				return false;
			}

			ShaderSystem::set_uniform(data->world_shader_info.projection_location, &data->world_shader_info.projection);
			ShaderSystem::set_uniform(data->world_shader_info.view_location, &data->world_shader_info.view);
			ShaderSystem::apply_global();

			for (uint32 i = 0; i < packet_data->world_geometries_count; i++)
			{
				const GeometryRenderData* render_data = &packet.geometries[i];
				cur_instance_id = render_data->unique_id;

				ShaderSystem::bind_instance(cur_instance_id);
			
				uint32 r, g, b = 0;
				Math::uint32_to_rgb(cur_instance_id, &r, &g, &b);
				Math::Vec3f id_color = Math::rgb_uint32_to_vec3(r, g, b);
				ShaderSystem::set_uniform(data->world_shader_info.id_color_location, &id_color);
				ShaderSystem::apply_instance(data->instances[cur_instance_id].is_dirty);
				data->instances[cur_instance_id].is_dirty = false;

				ShaderSystem::set_uniform(data->world_shader_info.model_location, &render_data->model);

				Renderer::geometry_draw(packet.geometries[i]);
			}

			if (!renderpass_end(pass))
			{
				SHMERROR("render_view_pick_on_render - draw_frame - failed to end renderpass!");
				return false;
			}

			pass_i++;
			pass = &self->renderpasses[pass_i];

			if (!renderpass_begin(pass, &pass->render_targets[render_target_index]))
			{
				SHMERROR("render_view_pick_on_render - failed to begin renderpass!");
				return false;
			}

			if (!ShaderSystem::use_shader(data->ui_shader_info.shader->id))
			{
				SHMERROR("render_view_pick_on_render - Failed to use shader. Render frame failed.");
				return false;
			}

			ShaderSystem::set_uniform(data->ui_shader_info.projection_location, &data->ui_shader_info.projection);
			ShaderSystem::set_uniform(data->ui_shader_info.view_location, &data->ui_shader_info.view);
			ShaderSystem::apply_global();

			for (uint32 i = packet_data->world_geometries_count; i < packet.geometries.count; i++)
			{
				const GeometryRenderData* render_data = &packet.geometries[i];
				cur_instance_id = render_data->unique_id;

				ShaderSystem::bind_instance(cur_instance_id);
			
				uint32 r, g, b = 0;
				Math::uint32_to_rgb(cur_instance_id, &r, &g, &b);
				Math::Vec3f id_color = Math::rgb_uint32_to_vec3(r, g, b);
				ShaderSystem::set_uniform(data->ui_shader_info.id_color_location, &id_color);
				ShaderSystem::apply_instance(data->instances[cur_instance_id].is_dirty);
				data->instances[cur_instance_id].is_dirty = false;

				ShaderSystem::set_uniform(data->ui_shader_info.model_location, &render_data->model);

				Renderer::geometry_draw(packet.geometries[i]);
			}

			for (uint32 i = 0; i < packet_data->text_count; i++)
			{
				UIText* text = packet_data->texts[i];
				cur_instance_id = text->unique_id;
				ShaderSystem::bind_instance(cur_instance_id);

				
				uint32 r, g, b = 0;
				Math::uint32_to_rgb(cur_instance_id, &r, &g, &b);
				Math::Vec3f id_color = Math::rgb_uint32_to_vec3(r, g, b);
				ShaderSystem::set_uniform(data->ui_shader_info.id_color_location, &id_color);
				ShaderSystem::apply_instance(true);

				Math::Mat4 model = Math::transform_get_world(text->transform);
				ShaderSystem::set_uniform(data->ui_shader_info.model_location, &model);

				ui_text_draw(text);
			}

			if (!renderpass_end(pass))
			{
				SHMERROR("render_view_pick_on_render - draw_frame - failed to end renderpass!");
				return false;
			}

		}

		Texture* t = &data->color_target_attachment_texture;

		uint8 pixel[4] = {};
		Math::Vec2i mouse_pos = { 800, 450 };//Input::get_mouse_position();
		uint32 mouse_x = (uint32)clamp(mouse_pos.x, 0, self->width - 1);
		uint32 mouse_y = (uint32)clamp(mouse_pos.y, 0, self->height - 1);	
		Renderer::texture_read_pixel(t, mouse_x, mouse_y, (uint32*)&pixel);

		/*Sarray<uint32> test(t->width * t->height, 0);
		Renderer::texture_read_data(t, 0, t->width * t->height * sizeof(uint32), test.data);

		for (uint32 i = 0; i < test.capacity; i++)
		{
			if (test[i] != 0)
				SHMDEBUG("Halt!");
		}*/

		uint32 id = INVALID_ID;
		id = Math::rgb_to_uint32(pixel[0], pixel[1], pixel[2]);
		if (id != 0)
			__debugbreak();
		if ((id & 0xFFFFFF) == 0xFFFFFF)
			id = INVALID_ID;

		EventData e_data;
		e_data.ui32[0] = id;
		Event::event_fire(SystemEventCode::OBJECT_HOVER_ID_CHANGED, 0, e_data);

		return true;

	}

	bool32 render_view_pick_regenerate_attachment_target(const RenderView* self, uint32 pass_index, RenderTargetAttachment* attachment)
	{
		RenderViewPickInternalData* data = (RenderViewPickInternalData*)self->internal_data.data;

		if (attachment->type == RenderTargetAttachmentType::COLOR)
			attachment->texture = &data->color_target_attachment_texture;
		else if (attachment->type == RenderTargetAttachmentType::DEPTH)
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
		attachment->texture->flags |= TextureFlags::IS_WRITABLE;
		attachment->texture->flags |= has_transparency ? TextureFlags::HAS_TRANSPARENCY : 0;
		if (attachment->type == RenderTargetAttachmentType::DEPTH)
			attachment->texture->flags |= TextureFlags::IS_DEPTH;

		Renderer::texture_create_writable(attachment->texture);

		return true;
	}

}