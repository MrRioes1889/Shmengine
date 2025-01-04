#include "RenderViewSkybox.hpp"

#include "core/Event.hpp"
#include "utility/Math.hpp"
#include "utility/math/Transform.hpp"
#include "resources/loaders/ShaderLoader.hpp"
#include "resources/Skybox.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/CameraSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "utility/Sort.hpp"

#include "optick.h"

struct RenderViewSkyboxInternalData {
	Renderer::Shader* shader;

	float32 near_clip;
	float32 far_clip;
	float32 fov;

	uint16 projection_location;
	uint16 view_location;
	uint16 cube_map_location;

	Math::Mat4 projection_matrix;

	Camera* camera;
};

namespace Renderer
{

	static bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{

		RenderView* self = (RenderView*)listener_inst;
		if (!self)
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

	bool32 render_view_skybox_on_create(RenderView* self)
	{

		self->internal_data.init(sizeof(RenderViewSkyboxInternalData), 0, AllocationTag::RENDERER);
		RenderViewSkyboxInternalData* data = (RenderViewSkyboxInternalData*)self->internal_data.data;

		ShaderConfig s_config = {};
		if (!ResourceSystem::shader_loader_load(Renderer::RendererConfig::builtin_shader_name_skybox, 0, &s_config))
		{
			SHMERROR("Failed to load skybox shader config.");
			return false;
		}

		if (!ShaderSystem::create_shader(&self->renderpasses[0], &s_config))
		{
			SHMERROR("Failed to create skybox shader.");
			ResourceSystem::shader_loader_unload(&s_config);
			return false;
		}
		ResourceSystem::shader_loader_unload(&s_config);

		data->shader = ShaderSystem::get_shader(self->custom_shader_name ? self->custom_shader_name : Renderer::RendererConfig::builtin_shader_name_skybox);

		data->projection_location = ShaderSystem::get_uniform_index(data->shader, "projection");
		data->view_location = ShaderSystem::get_uniform_index(data->shader, "view");
		data->cube_map_location = ShaderSystem::get_uniform_index(data->shader, "cube_texture");

		data->near_clip = 0.1f;
		data->far_clip = 1000.0f;
		data->fov = Math::deg_to_rad(45.0f);

		data->projection_matrix = Math::mat_perspective(data->fov, 1280.0f / 720.0f, data->near_clip, data->far_clip);
		data->camera = CameraSystem::get_default_camera();

		Event::event_register((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);

		return true;

	}

	void render_view_skybox_on_destroy(RenderView* self)
	{
		self->internal_data.free_data();
		Event::event_unregister((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);
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

	bool32 render_view_skybox_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, void* data, RenderViewPacket* out_packet)
	{

		SkyboxPacketData* skybox_data = (SkyboxPacketData*)data;
		RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;

		out_packet->view = self;

		out_packet->projection_matrix = &internal_data->projection_matrix;
		out_packet->view_matrix = &internal_data->camera->get_view();
		out_packet->view_position = internal_data->camera->get_position();

		out_packet->lighting = {};

		out_packet->geometries.init(skybox_data->geometries_count, 0, AllocationTag::RENDERER, skybox_data->geometries);
		out_packet->geometries.set_count(skybox_data->geometries_count);
	
		return true;

	}

	void render_view_skybox_on_destroy_packet(const RenderView* self, RenderViewPacket* packet)
	{
		packet->geometries.free_data();
		packet->extended_data = 0;
	}

	bool32 render_view_skybox_on_render(RenderView* self, RenderViewPacket& packet, uint32 frame_number, uint64 render_target_index)
	{

		OPTICK_EVENT();

		RenderViewSkyboxInternalData* data = (RenderViewSkyboxInternalData*)self->internal_data.data;
		SkyboxPacketData* skybox_data = (SkyboxPacketData*)packet.extended_data;

		Math::Mat4 view_matrix = {};
		if (packet.geometries.count)
		{
			view_matrix = *packet.view_matrix;
			view_matrix.data[12] = 0.0f;
			view_matrix.data[13] = 0.0f;
			view_matrix.data[14] = 0.0f;
		}

		for (uint32 rp = 0; rp < self->renderpasses.capacity; rp++)
		{

			RenderPass* renderpass = &self->renderpasses[rp];

			if (!renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
			{
				SHMERROR("Failed to begin renderpass!");
				return false;
			}

			uint32 shader_id = INVALID_ID;

			for (uint32 geometry_i = 0; geometry_i < packet.geometries.count; geometry_i++)
			{
				GeometryRenderData* g = &packet.geometries[geometry_i];

				if (g->shader_id != shader_id)
				{
					shader_id = g->shader_id;
					ShaderSystem::use_shader(shader_id);

					if (!ShaderSystem::apply_globals(shader_id, packet.lighting, frame_number,
						packet.projection_matrix, &view_matrix, 0, 0, 0))
					{
						SHMERROR("Failed to apply globals to shader.");
						return false;
					}
				}

				// Apply instance
				g->on_render(shader_id, packet.lighting, &g->model, g->render_object, frame_number);

				geometry_draw(packet.geometries[geometry_i].geometry_data);
			}

			if (!renderpass_end(renderpass))
			{
				SHMERROR("render_view_skybox_on_render - draw_frame - failed to end renderpass!");
				return false;
			}
		}

		return true;
	}

}