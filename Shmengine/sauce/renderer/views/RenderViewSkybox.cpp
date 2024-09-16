#include "RenderViewSkybox.hpp"

#include "core/Event.hpp"
#include "utility/Math.hpp"
#include "utility/math/Transform.hpp"
#include "systems/ResourceSystem.hpp"
#include "systems/RenderViewSystem.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "systems/CameraSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "utility/Sort.hpp"

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

		Resource config_resource;
		if (!ResourceSystem::load(Renderer::RendererConfig::builtin_shader_name_skybox, ResourceType::SHADER, 0, &config_resource))
		{
			SHMERROR("Failed to load world skybox shader config.");
			return false;
		}
		ShaderConfig* config = (ShaderConfig*)config_resource.data;

		if (!ShaderSystem::create_shader(&self->renderpasses[0], config))
		{
			SHMERROR("Failed to create skybox shader.");
			ResourceSystem::unload(&config_resource);
			return false;
		}
		ResourceSystem::unload(&config_resource);

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

	bool32 render_view_skybox_on_build_packet(RenderView* self, void* data, RenderViewPacket* out_packet)
	{

		SkyboxPacketData* skybox_data = (SkyboxPacketData*)data;
		RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;

		out_packet->view = self;

		out_packet->projection_matrix = internal_data->projection_matrix;
		out_packet->view_matrix = internal_data->camera->get_view();
		out_packet->view_position = internal_data->camera->get_position();

		out_packet->extended_data = skybox_data;
		return true;

	}

	void render_view_skybox_on_destroy_packet(const RenderView* self, RenderViewPacket* packet)
	{

	}

	bool32 render_view_skybox_on_render(RenderView* self, const RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index)
	{

		RenderViewSkyboxInternalData* data = (RenderViewSkyboxInternalData*)self->internal_data.data;
		SkyboxPacketData* skybox_data = (SkyboxPacketData*)packet.extended_data;

		Math::Mat4 view_matrix = data->camera->get_view();
		view_matrix.data[12] = 0.0f;
		view_matrix.data[13] = 0.0f;
		view_matrix.data[14] = 0.0f;

		for (uint32 rp = 0; rp < self->renderpasses.capacity; rp++)
		{

			Renderpass* renderpass = &self->renderpasses[rp];

			if (!renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
			{
				SHMERROR("render_view_skybox_on_render - failed to begin renderpass!");
				return false;
			}

			if (!ShaderSystem::use_shader(data->shader->id)) {
				SHMERROR("render_view_skybox_on_render - Failed to use shader. Render frame failed.");
				return false;
			}

			// Apply globals
			Renderer::shader_bind_globals(ShaderSystem::get_shader(data->shader->id));
			if (!ShaderSystem::set_uniform(data->projection_location, &packet.projection_matrix)) {
				SHMERROR("render_view_skybox_on_render - Failed to apply skybox projection uniform.");
				return false;
			}
			if (!ShaderSystem::set_uniform(data->view_location, &view_matrix)) {
				SHMERROR("render_view_skybox_on_render - Failed to apply skybox view uniform.");
				return false;
			}
			ShaderSystem::apply_global();

			// Instance
			ShaderSystem::bind_instance(skybox_data->skybox->instance_id);
			if (!ShaderSystem::set_uniform(data->cube_map_location, &skybox_data->skybox->cubemap)) {
				SHMERROR("render_view_skybox_on_render - Failed to apply skybox cube map uniform.");
				return false;
			}
			bool8 needs_update = skybox_data->skybox->renderer_frame_number != frame_number;
			ShaderSystem::apply_instance(needs_update);

			// Sync the frame number.
			skybox_data->skybox->renderer_frame_number = frame_number;

			// Draw it.
			GeometryRenderData render_data = {};
			render_data.geometry = skybox_data->skybox->g;
			Renderer::geometry_draw(render_data);

			if (!renderpass_end(renderpass))
			{
				SHMERROR("render_view_skybox_on_render - draw_frame - failed to end renderpass!");
				return false;
			}
		}

		return true;
	}

}