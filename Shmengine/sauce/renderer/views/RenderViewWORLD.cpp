#include "RenderViewWorld.hpp"

#include "core/Event.hpp"
#include "utility/Math.hpp"
#include "utility/math/Transform.hpp"
#include "resources/loaders/ShaderLoader.hpp"
#include "resources/Mesh.hpp"
#include "systems/GeometrySystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/CameraSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "utility/Sort.hpp"

#include "optick.h"

struct RenderViewWorldInternalData {
	Shader* material_shader;
	Shader* terrain_shader;
	float32 near_clip;
	float32 far_clip;
	float32 fov;
	uint32 render_mode;
	Math::Mat4 projection_matrix;
	Math::Vec4f ambient_color;

	DirectionalLight* dir_light;
	uint32 p_lights_count;
	PointLight* p_lights;

	Camera* camera;
};

namespace Renderer
{

	static bool32 on_event(uint16 code, void* sender, void* listener_inst, EventData data)
	{

		RenderView* self = (RenderView*)listener_inst;
		if (!self) 
			return false;

		RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;
		if (!internal_data) 
			return false;

		switch (code) 
		{
		case SystemEventCode::SET_RENDER_MODE: {
			int32 mode = data.i32[0];
			switch (mode) {
			case ViewMode::DEFAULT:
			{
				SHMDEBUG("Renderer mode set to default.");
				internal_data->render_mode = ViewMode::DEFAULT;
				break;
			}
			case ViewMode::LIGHTING:
			{
				SHMDEBUG("Renderer mode set to lighting.");
				internal_data->render_mode = ViewMode::LIGHTING;
				break;
			}
			case ViewMode::NORMALS:
			{
				SHMDEBUG("Renderer mode set to normals.");
				internal_data->render_mode = ViewMode::NORMALS;
				break;
			}			
			}
			return true;
		}
		case SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED:
		{
			RenderViewSystem::regenerate_render_targets(self);
			return false;
		}
		}

		return false;
	}

	bool32 render_view_world_on_create(RenderView* self)
	{

		self->internal_data.init(sizeof(RenderViewWorldInternalData), 0, AllocationTag::RENDERER);
		RenderViewWorldInternalData* data = (RenderViewWorldInternalData*)self->internal_data.data;

		if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_material, &self->renderpasses[0]))
		{
			SHMERROR("Failed to create world pick shader.");
			return false;
		}

		if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_terrain, &self->renderpasses[0]))
		{
			SHMERROR("Failed to create world pick shader.");
			return false;
		}

		data->material_shader = ShaderSystem::get_shader(self->custom_shader_name ? self->custom_shader_name : Renderer::RendererConfig::builtin_shader_name_material);
		data->terrain_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_terrain);

		data->near_clip = 0.1f;
		data->far_clip = 4000.0f;
		data->fov = Math::deg_to_rad(45.0f);

		data->projection_matrix = Math::mat_perspective(data->fov, 1280.0f / 720.0f, data->near_clip, data->far_clip);
		data->camera = CameraSystem::get_default_camera();
		data->ambient_color = { 0.25f, 0.25f, 0.25f, 1.0f };	

		Event::event_register((uint16)SystemEventCode::SET_RENDER_MODE, self, on_event);
		Event::event_register((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);

		return true;

	}

	void render_view_world_on_destroy(RenderView* self)
	{
		Event::event_unregister((uint16)SystemEventCode::SET_RENDER_MODE, self, on_event);
		Event::event_unregister((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);
		self->internal_data.free_data();
	}

	void render_view_world_on_resize(RenderView* self, uint32 width, uint32 height)
	{
		if (self->width == width && self->height == height)
			return;

		RenderViewWorldInternalData* data = (RenderViewWorldInternalData*)self->internal_data.data;

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

	bool32 render_view_world_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, void* data, RenderViewPacket* out_packet)
	{

		struct GeometryDistance
		{
			GeometryRenderData g;
			float32 dist;

			SHMINLINE bool8 operator<=(const GeometryDistance& other) { return dist <= other.dist; }
			SHMINLINE bool8 operator>=(const GeometryDistance& other) { return dist >= other.dist; }
		};

		WorldPacketData* packet_data = (WorldPacketData*)data;
		RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

		out_packet->extended_data = frame_allocator->allocate(sizeof(WorldPacketData));
		Memory::copy_memory(packet_data, out_packet->extended_data, sizeof(WorldPacketData));

		// TODO: frame_allocator for packet geometries
		void* geometries_block = frame_allocator->allocate(sizeof(GeometryRenderData) * packet_data->geometries_count);
		out_packet->geometries.init(packet_data->geometries_count, 0, AllocationTag::RENDERER, geometries_block);
		out_packet->view = self;

		out_packet->projection_matrix = &internal_data->projection_matrix;
		out_packet->view_matrix = &internal_data->camera->get_view();
		out_packet->view_position = internal_data->camera->get_position();
		out_packet->ambient_color = internal_data->ambient_color;

		out_packet->lighting =
		{
			.dir_light = packet_data->dir_light,
			.p_lights_count = packet_data->p_lights_count,
			.p_lights = packet_data->p_lights
		};

		void* transparent_geometries_block = frame_allocator->allocate(sizeof(GeometryRenderData) * packet_data->geometries_count);
		Darray<GeometryDistance> transparent_geometries(packet_data->geometries_count, 0, AllocationTag::RENDERER, transparent_geometries_block);

		for (uint32 i = 0; i < packet_data->geometries_count; i++)
		{
			GeometryRenderData* g_data = &packet_data->geometries[i];

			if (!g_data->has_transparency)
			{
				out_packet->geometries.emplace(*g_data);
			}
			else
			{
				Math::Vec3f center = Math::vec_transform(g_data->geometry_data->center, g_data->model);
				float32 distance = Math::vec_distance(center, internal_data->camera->get_position());

				GeometryDistance* g_dist = &transparent_geometries[transparent_geometries.emplace()];
				g_dist->dist = Math::abs(distance);
				g_dist->g = *g_data;
			}
		}		

		quick_sort(transparent_geometries.data, 0, transparent_geometries.count - 1, false);
		for (uint32 i = 0; i < transparent_geometries.count; i++)
		{
			GeometryRenderData* render_data = &out_packet->geometries[out_packet->geometries.emplace()];
			*render_data = transparent_geometries[i].g;
		}

		return true;

	}

	void render_view_world_on_destroy_packet(const RenderView* self, RenderViewPacket* packet)
	{
		packet->geometries.free_data();
		packet->extended_data = 0;
	}

	bool32 render_view_world_on_render(RenderView* self, RenderViewPacket& packet, uint32 frame_number, uint64 render_target_index)
	{

		OPTICK_EVENT();

		RenderViewWorldInternalData* data = (RenderViewWorldInternalData*)self->internal_data.data;
		WorldPacketData* packet_data = (WorldPacketData*)packet.extended_data;

		for (uint32 rp = 0; rp < self->renderpasses.capacity; rp++)
		{

			RenderPass* renderpass = &self->renderpasses[rp];

			if (!renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
			{
				SHMERROR("render_view_world_on_render - failed to begin renderpass!");
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
						packet.projection_matrix, packet.view_matrix, &packet.ambient_color, &packet.view_position, data->render_mode))
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
				SHMERROR("render_view_world_on_render - draw_frame - failed to end renderpass!");
				return false;
			}
		}

		return true;
	}

}