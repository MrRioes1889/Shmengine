#include "RenderViewUI.hpp"

#include "utility/Math.hpp"
#include "utility/math/Transform.hpp"
#include "systems/ShaderSystem.hpp"
#include "systems/MaterialSystem.hpp"
#include "renderer/RendererFrontend.hpp"
#include "resources/UIText.hpp"

struct RenderViewUIInternalData {
	float32 near_clip;
	float32 far_clip;
	Math::Mat4 projection_matrix;
	Math::Mat4 view_matrix;
	uint32 shader_id;
	Renderer::Shader* shader;
	uint16 diffuse_map_location;
	uint16 diffuse_color_location;
	uint16 model_location;
	// u32 render_mode;
};

namespace Renderer
{

	bool32 render_view_ui_on_create(RenderView* self)
	{
		
		self->internal_data.init(sizeof(RenderViewUIInternalData), 0, AllocationTag::RENDERER);
		RenderViewUIInternalData* data = (RenderViewUIInternalData*)self->internal_data.data;
	
		data->shader_id = ShaderSystem::get_id(self->custom_shader_name ? self->custom_shader_name : Renderer::RendererConfig::builtin_shader_name_ui);
		data->shader = ShaderSystem::get_shader(data->shader_id);
		data->diffuse_map_location = ShaderSystem::get_uniform_index(data->shader, "diffuse_texture");
		data->diffuse_color_location = ShaderSystem::get_uniform_index(data->shader, "diffuse_color");
		data->model_location = ShaderSystem::get_uniform_index(data->shader, "model");

		data->near_clip = -100.0f;
		data->far_clip = 100.0f;

		data->projection_matrix = Math::mat_orthographic(0.0f, 1280.0f, 720.0f, 0.0f, data->near_clip, data->far_clip);
		data->view_matrix = MAT4_IDENTITY;

		return true;

	}

	void render_view_ui_on_destroy(RenderView* self)
	{
		self->internal_data.free_data();
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
			self->renderpasses[i]->dim.width = width;
			self->renderpasses[i]->dim.height = height;
		}
	}

	bool32 render_view_ui_on_build_packet(const RenderView* self, void* data, RenderViewPacket* out_packet)
	{

		UIPacketData* packet_data = (UIPacketData*)data;
		RenderViewUIInternalData* internal_data = (RenderViewUIInternalData*)self->internal_data.data;

		uint32 total_geometry_count = 0;
		for (uint32 i = 0; i < packet_data->mesh_data.mesh_count; i++)
			total_geometry_count += packet_data->mesh_data.meshes[i]->geometries.count;

		out_packet->geometries.init(total_geometry_count, 0, AllocationTag::RENDERER);
		out_packet->view = self;

		out_packet->projection_matrix = internal_data->projection_matrix;
		out_packet->view_matrix = internal_data->view_matrix;

		out_packet->extended_data = data;

		for (uint32 i = 0; i < packet_data->mesh_data.mesh_count; i++)
		{
			Mesh* m = packet_data->mesh_data.meshes[i];
			for (uint32 g = 0; g < m->geometries.count; g++)
			{
				GeometryRenderData* render_data = out_packet->geometries.push({});
				render_data->geometry = m->geometries[g];
				render_data->model = Math::transform_get_world(m->transform);
			}
		}

		return true;

	}

	void render_view_ui_on_destroy_packet(const RenderView* self, RenderViewPacket* packet)
	{
		packet->geometries.free_data();
	}

	bool32 render_view_ui_on_render(const RenderView* self, const RenderViewPacket& packet, uint64 frame_number, uint64 render_target_index)
	{

		RenderViewUIInternalData* data = (RenderViewUIInternalData*)self->internal_data.data;

		for (uint32 rp = 0; rp < self->renderpasses.capacity; rp++)
		{

			Renderpass* renderpass = self->renderpasses[rp];

			if (!renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
			{
				SHMERROR("render_view_ui_on_render - failed to begin renderpass!");
				return false;
			}

			if (!ShaderSystem::use_shader(data->shader_id)) {
				SHMERROR("render_view_ui_on_render - Failed to use shader. Render frame failed.");
				return false;
			}

			// Apply globals
			if (!MaterialSystem::apply_globals(data->shader_id, frame_number, &packet.projection_matrix, &packet.view_matrix, 0, 0, 0)) {
				SHMERROR("render_view_ui_on_render - Failed to use apply globals for shader. Render frame failed.");
				return false;
			}

			for (uint32 i = 0; i < packet.geometries.count; i++)
			{
				Material* m = 0;
				if (packet.geometries[i].geometry->material) {
					m = packet.geometries[i].geometry->material;
				}
				else {
					m = MaterialSystem::get_default_material();
				}

				// Apply the material
				bool32 needs_update = (m->render_frame_number != frame_number);
				if (!MaterialSystem::apply_instance(m, needs_update))
				{
					SHMWARNV("render_view_ui_on_render - Failed to apply material '%s'. Skipping draw.", m->name);
					continue;
				}

				m->render_frame_number = (uint32)frame_number;

				// Apply the locals
				MaterialSystem::apply_local(m, packet.geometries[i].model);

				// Draw it.
				geometry_draw(packet.geometries[i]);
			}

			UIPacketData* packet_data = (UIPacketData*)packet.extended_data;  // array of texts
			for (uint32 i = 0; i < packet_data->text_count; ++i) {
				UIText* text = packet_data->texts[i];
				ShaderSystem::bind_instance(text->instance_id);

				if (!ShaderSystem::set_uniform(data->diffuse_map_location, &text->font_atlas->map)) {
					SHMERROR("Failed to apply bitmap font diffuse map uniform.");
					return false;
				}

				// TODO: font colour.
				static Math::Vec4f white_colour = { 1.0f, 1.0f, 1.0f, 1.0f };  // white
				if (!ShaderSystem::set_uniform(data->diffuse_color_location, &white_colour)) {
					SHMERROR("Failed to apply bitmap font diffuse colour uniform.");
					return false;
				}
				bool32 needs_update = text->render_frame_number != frame_number;
				ShaderSystem::apply_instance(needs_update);

				// Sync the frame number.
				text->render_frame_number = frame_number;

				// Apply the locals
				Math::Mat4 model = Math::transform_get_world(text->transform);
				if (!ShaderSystem::set_uniform(data->model_location, &model)) {
					SHMERROR("Failed to apply model matrix for text");
				}

				ui_text_draw(text);
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