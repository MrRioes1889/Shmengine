#include "RenderViewSkybox.hpp"

#include <core/Event.hpp>
#include <core/Identifier.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <resources/Skybox.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/CameraSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <utility/Sort.hpp>

#include <optick.h>

struct SkyboxShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 cube_map;
};

struct RenderViewSkyboxInternalData {
	Shader* skybox_shader;
	SkyboxShaderUniformLocations skybox_shader_u_locations;

	float32 near_clip;
	float32 far_clip;
	float32 fov;

	Math::Mat4 projection_matrix;

	Camera* camera;
};

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

bool32 render_view_skybox_on_register(RenderView* self)
{

	self->internal_data.init(sizeof(RenderViewSkyboxInternalData), 0, AllocationTag::RENDERER);
	RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;

	internal_data->skybox_shader_u_locations.projection = INVALID_ID16;
	internal_data->skybox_shader_u_locations.view = INVALID_ID16;
	internal_data->skybox_shader_u_locations.cube_map = INVALID_ID16;

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_skybox, &self->renderpasses[0]))
	{
		SHMERROR("Failed to create skybox shader.");
		return false;
	}

	internal_data->skybox_shader = ShaderSystem::get_shader(self->custom_shader_name ? self->custom_shader_name : Renderer::RendererConfig::builtin_shader_name_skybox);

	internal_data->skybox_shader_u_locations.projection = ShaderSystem::get_uniform_index(internal_data->skybox_shader, "projection");
	internal_data->skybox_shader_u_locations.view = ShaderSystem::get_uniform_index(internal_data->skybox_shader, "view");
	internal_data->skybox_shader_u_locations.cube_map = ShaderSystem::get_uniform_index(internal_data->skybox_shader, "cube_texture");

	internal_data->near_clip = 0.1f;
	internal_data->far_clip = 1000.0f;
	internal_data->fov = Math::deg_to_rad(45.0f);

	internal_data->projection_matrix = Math::mat_perspective(internal_data->fov, 1280.0f / 720.0f, internal_data->near_clip, internal_data->far_clip);
	internal_data->camera = CameraSystem::get_default_camera();

	Event::event_register((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);

	return true;

}

void render_view_skybox_on_unregister(RenderView* self)
{
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

bool32 render_view_skybox_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data)
{
	RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;

	if (packet_data->renderpass_id + 1 > self->renderpasses.capacity)
	{
		SHMERROR("Invalid renderpass id supplied in packet data!");
		return false;
	}

	self->geometries.copy_memory(packet_data->geometries, packet_data->geometries_count, self->geometries.count);
	
	return true;

}

void render_view_skybox_on_end_frame(RenderView* self)
{
	self->geometries.clear();
}

static bool32 set_globals_skybox(SkyboxShaderUniformLocations u_locations, Math::Mat4* projection, Math::Mat4* view)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, projection));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, view));

	return true;
}

static bool32 set_instance_skybox(SkyboxShaderUniformLocations u_locations, Renderer::InstanceRenderData instance, Math::Mat4* model)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.cube_map, instance.texture_maps[0]));

	return true;
}

bool32 render_view_skybox_on_render(RenderView* self, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index)
{
	OPTICK_EVENT();

	RenderViewSkyboxInternalData* internal_data = (RenderViewSkyboxInternalData*)self->internal_data.data;

	Math::Mat4 view_matrix = {};
	if (self->geometries.count)
	{
		view_matrix = internal_data->camera->get_view();
		view_matrix.data[12] = 0.0f;
		view_matrix.data[13] = 0.0f;
		view_matrix.data[14] = 0.0f;
	}

	const uint32 texture_maps_buffer_size = 12;
	TextureMap* texture_maps_buffer[texture_maps_buffer_size];

	for (uint32 rp = 0; rp < self->renderpasses.capacity; rp++)
	{

		Renderer::RenderPass* renderpass = &self->renderpasses[rp];

		if (!Renderer::renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
		{
			SHMERROR("Failed to begin renderpass!");
			return false;
		}

		uint32 shader_id = INVALID_ID;
		Shader* current_shader = 0;

		for (uint32 geometry_i = 0; geometry_i < self->geometries.count; geometry_i++)
		{
			Renderer::ObjectRenderData* object = &self->geometries[geometry_i];
				
			if (object->shader_id != shader_id)
			{
				shader_id = object->shader_id;
				ShaderSystem::use_shader(shader_id);

				bool32 globals_set = false;
				if (shader_id == internal_data->skybox_shader->id)
				{
					globals_set = set_globals_skybox(internal_data->skybox_shader_u_locations, &internal_data->projection_matrix, &view_matrix);
					current_shader = internal_data->skybox_shader;
				}						

				if (globals_set)
				{
					if (current_shader->renderer_frame_number != frame_number)
					{
						UNIFORM_APPLY_OR_FAIL(Renderer::shader_apply_globals(current_shader));
						current_shader->renderer_frame_number = frame_number;
					}			
				}
				else
				{
					SHMERROR("Unknown shader or failed to apply globals to shader.");
					shader_id = INVALID_ID;
					continue;
				}
			}
			
			Renderer::InstanceRenderData instance = {};
			instance.texture_maps = texture_maps_buffer;
			object->get_instance_render_data(object->render_object, &instance);
			ShaderSystem::bind_instance(instance.shader_instance_id);

			bool32 instance_set = false;
			if (shader_id == internal_data->skybox_shader->id)
				instance_set = set_instance_skybox(internal_data->skybox_shader_u_locations, instance, &object->model);
			else
				SHMERROR("Unknown shader or failed to apply instance to shader.");

			if (instance_set)
				UNIFORM_APPLY_OR_FAIL(Renderer::shader_apply_instance(ShaderSystem::get_shader(shader_id)));

			Renderer::geometry_draw(object->geometry_data);
		}

		if (!Renderer::renderpass_end(renderpass))
		{
			SHMERROR("render_view_skybox_on_render - draw_frame - failed to end renderpass!");
			return false;
		}
	}

	return true;
}