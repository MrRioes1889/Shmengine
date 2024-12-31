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

struct RenderViewWorldInternalData {
	Renderer::Shader* material_shader;
	Renderer::Shader* terrain_shader;
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


		ShaderConfig s_config = {};
		if (!ResourceSystem::shader_loader_load(Renderer::RendererConfig::builtin_shader_name_material, 0, &s_config))
		{
			SHMERROR("Failed to load world shader config.");
			return false;
		}

		bool32 created = ShaderSystem::create_shader(&self->renderpasses[0], &s_config);
		ResourceSystem::shader_loader_unload(&s_config);
		if (!created)
		{
			SHMERROR("Failed to create world shader.");
			return false;
		}


		if (!ResourceSystem::shader_loader_load(Renderer::RendererConfig::builtin_shader_name_terrain, 0, &s_config))
		{
			SHMERROR("Failed to load terrain shader config.");
			return false;
		}

		created = ShaderSystem::create_shader(&self->renderpasses[0], &s_config);
		ResourceSystem::shader_loader_unload(&s_config);
		if (!created)
		{
			SHMERROR("Failed to create world shader.");
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

		uint32 total_geometry_count = 0;
		total_geometry_count += packet_data->mesh_geometries_count + packet_data->terrain_geometries_count;

		out_packet->extended_data = frame_allocator->allocate(sizeof(WorldPacketData));
		Memory::copy_memory(packet_data, out_packet->extended_data, sizeof(WorldPacketData));

		out_packet->geometries.init(total_geometry_count, 0, AllocationTag::RENDERER);
		out_packet->view = self;

		out_packet->projection_matrix = internal_data->projection_matrix;
		out_packet->view_matrix = internal_data->camera->get_view();
		out_packet->view_position = internal_data->camera->get_position();
		out_packet->ambient_color = internal_data->ambient_color;

		Darray<GeometryDistance> transparent_geometries(total_geometry_count, 0, AllocationTag::RENDERER);

		for (uint32 i = 0; i < packet_data->mesh_geometries_count; i++)
		{
			GeometryRenderData* g_data = &packet_data->geometries[i];
			if (!g_data->geometry)
				continue;

			bool32 has_transparency = false;
			if (g_data->texture_maps_count)
				has_transparency = (g_data->texture_maps[0].texture->flags & TextureFlags::HAS_TRANSPARENCY);

			if (!has_transparency)
			{
				out_packet->geometries.emplace(*g_data);
			}
			else
			{
				Math::Vec3f center = Math::vec_transform(g_data->geometry->center, g_data->model);
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

		for (uint32 i = packet_data->mesh_geometries_count; i < total_geometry_count; i++)
		{
			GeometryRenderData* g_data = &packet_data->geometries[i];
			if (!g_data->geometry)
				continue;

			out_packet->geometries.emplace(*g_data);
		}

		return true;

	}

	void render_view_world_on_destroy_packet(const RenderView* self, RenderViewPacket* packet)
	{
		packet->geometries.free_data();
		packet->extended_data = 0;
	}

	bool32 render_view_world_on_render(RenderView* self, RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index)
	{

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

			if (packet_data)
			{

				ShaderSystem::LightingInfo lighting =
				{
					.dir_light = packet_data->dir_light,
					.p_lights_count = packet_data->p_lights_count,
					.p_lights = packet_data->p_lights
				};

				uint32 geometry_i = 0;

				if (packet_data->terrain_geometries_count)
				{
					uint32 terrain_shader_id = ShaderSystem::get_terrain_shader_id();
					if (terrain_shader_id == INVALID_ID)
					{
						SHMERROR("Failed to retrieve terrain shader id.");
						return false;
					}
					ShaderSystem::use_shader(terrain_shader_id);

					if (!ShaderSystem::apply_globals(terrain_shader_id, lighting, frame_number,
						&packet.projection_matrix, &packet.view_matrix, &packet.ambient_color, &packet.view_position, data->render_mode))
					{
						SHMERROR("Failed to apply globals to terrain shader.");
						return false;
					}

					for (uint32 i = 0; i < packet_data->terrain_geometries_count; i++, geometry_i++)
					{

						GeometryRenderData* g = &packet.geometries[geometry_i];

						// Apply the material
						bool32 needs_update = (*g->render_frame_number != (uint32)frame_number);
						if (!ShaderSystem::apply_instance(terrain_shader_id, g->shader_instance_id, g->properties,
							g->texture_maps, g->texture_maps_count, lighting, needs_update))
						{
							SHMWARN("Failed to apply instance. Skipping draw.");
							continue;
						}

						*g->render_frame_number = (uint32)frame_number;

						// Apply the locals
						ShaderSystem::apply_local(terrain_shader_id, packet.geometries[geometry_i].model);

						// Draw it.
						geometry_draw(packet.geometries[geometry_i]);

					}
				}

				if (packet_data->mesh_geometries_count)
				{

					uint32 material_shader_id = ShaderSystem::get_material_shader_id();
					if (!ShaderSystem::use_shader(material_shader_id))
					{
						SHMERROR("render_view_world_on_render - Failed to use shader. Render frame failed.");
						return false;
					}

					// Apply globals
					if (!ShaderSystem::apply_globals(material_shader_id, lighting, frame_number, &packet.projection_matrix, &packet.view_matrix, &packet.ambient_color, &packet.view_position, data->render_mode))
					{
						SHMERROR("render_view_world_on_render - Failed to use apply globals for shader. Render frame failed.");
						return false;
					}

					for (uint32 i = 0; i < packet_data->mesh_geometries_count; i++, geometry_i++)
					{

						GeometryRenderData* g = &packet.geometries[geometry_i];

						// Apply the material
						bool32 needs_update = (*g->render_frame_number != (uint32)frame_number);
						if (!ShaderSystem::apply_instance(material_shader_id, g->shader_instance_id, g->properties,
							g->texture_maps, g->texture_maps_count, lighting, needs_update))
						{
							SHMWARN("Failed to apply instance. Skipping draw.");
							continue;
						}

						*g->render_frame_number = (uint32)frame_number;

						// Apply the locals
						ShaderSystem::apply_local(material_shader_id, packet.geometries[geometry_i].model);

						// Draw it.
						geometry_draw(packet.geometries[geometry_i]);

					}

				}		
				
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