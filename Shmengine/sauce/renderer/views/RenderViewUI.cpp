#include "RenderViewUI.hpp"

#include "core/Event.hpp"
#include "utility/Math.hpp"
#include "utility/math/Transform.hpp"
#include "resources/loaders/ShaderLoader.hpp"
#include "resources/Mesh.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/UIText.hpp"

#include "optick.h"

struct RenderViewUIInternalData {
	float32 near_clip;
	float32 far_clip;
	Math::Mat4 projection_matrix;
	Math::Mat4 view_matrix;
	Shader* shader;
	uint16 diffuse_map_location;
	uint16 properties_location;
	uint16 model_location;
	// u32 render_mode;
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

	bool32 render_view_ui_on_create(RenderView* self)
	{
		
		self->internal_data.init(sizeof(RenderViewUIInternalData), 0, AllocationTag::RENDERER);
		RenderViewUIInternalData* data = (RenderViewUIInternalData*)self->internal_data.data;

		if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_ui, &self->renderpasses[0]))
		{
			SHMERROR("Failed to create world pick shader.");
			return false;
		}
	
		data->shader = ShaderSystem::get_shader(self->custom_shader_name ? self->custom_shader_name : Renderer::RendererConfig::builtin_shader_name_ui);
		data->diffuse_map_location = ShaderSystem::get_uniform_index(data->shader, "diffuse_texture");
		data->properties_location = ShaderSystem::get_uniform_index(data->shader, "properties");
		data->model_location = ShaderSystem::get_uniform_index(data->shader, "model");

		data->near_clip = -100.0f;
		data->far_clip = 100.0f;

		data->projection_matrix = Math::mat_orthographic(0.0f, 1280.0f, 720.0f, 0.0f, data->near_clip, data->far_clip);
		data->view_matrix = MAT4_IDENTITY;

		Event::event_register((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);

		return true;

	}

	void render_view_ui_on_destroy(RenderView* self)
	{
		self->internal_data.free_data();
		Event::event_unregister((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);
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

	bool32 render_view_ui_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, void* data, RenderViewPacket* out_packet)
	{		
		UIPacketData* packet_data = (UIPacketData*)data;
		RenderViewUIInternalData* internal_data = (RenderViewUIInternalData*)self->internal_data.data;

		out_packet->geometries.init(packet_data->geometries_count, 0, AllocationTag::RENDERER, packet_data->geometries);
		out_packet->geometries.set_count(packet_data->geometries_count);
		out_packet->view = self;

		out_packet->projection_matrix = &internal_data->projection_matrix;
		out_packet->view_matrix = &internal_data->view_matrix;

		return true;

	}

	void render_view_ui_on_destroy_packet(const RenderView* self, RenderViewPacket* packet)
	{
		packet->geometries.free_data();
		packet->extended_data = 0;
	}

	bool32 render_view_ui_on_render(RenderView* self, RenderViewPacket& packet, uint32 frame_number, uint64 render_target_index)
	{

		OPTICK_EVENT();

		RenderViewUIInternalData* data = (RenderViewUIInternalData*)self->internal_data.data;

		for (uint32 rp = 0; rp < self->renderpasses.capacity; rp++)
		{

			RenderPass* renderpass = &self->renderpasses[rp];

			if (!renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
			{
				SHMERROR("render_view_ui_on_render - failed to begin renderpass!");
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
						packet.projection_matrix, packet.view_matrix, 0, 0, 0))
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
				SHMERROR("render_view_ui_on_render - draw_frame - failed to end renderpass!");
				return false;
			}
		}

		return true;
	}

}