#include "RenderViewWorldEditor.hpp"

#include <core/Event.hpp>
#include <core/Identifier.hpp>
#include <core/FrameData.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <renderer/Camera.hpp>
#include <utility/Sort.hpp>

#include <optick.h>

struct VertexCoordinateGrid
{
	uint32 index;
};

struct CoordinateGrid
{
	GeometryData geometry;
};

struct RenderViewWorldInternalData {
	ShaderId color3D_shader_id;
	Color3DShaderUniformLocations color3D_shader_u_locations;

	ShaderId coordinate_grid_shader_id;
	CoordinateGridShaderUniformLocations coordinate_grid_shader_u_locations;

	float32 near_clip;
	float32 far_clip;
	float32 fov;

	Math::Mat4 projection_matrix;

	CoordinateGrid coordinate_grid;
};

bool8 render_view_world_editor_on_create(RenderView* self)
{

	self->internal_data.init(sizeof(RenderViewWorldInternalData), 0, AllocationTag::Renderer);
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	internal_data->color3D_shader_id = ShaderSystem::get_shader_id(Renderer::RendererConfig::builtin_shader_name_color3D);
	Shader* color3D_shader = ShaderSystem::get_shader(internal_data->color3D_shader_id);

	Shader* coordinate_grid_shader = 0;
	internal_data->coordinate_grid_shader_id = ShaderSystem::acquire_shader_id(Renderer::RendererConfig::builtin_shader_name_coordinate_grid, &coordinate_grid_shader);
	if (coordinate_grid_shader)
		Renderer::shader_init_from_resource(Renderer::RendererConfig::builtin_shader_name_coordinate_grid, &self->renderpasses[0], coordinate_grid_shader);

	internal_data->color3D_shader_u_locations.projection = Renderer::shader_get_uniform_index(color3D_shader, "projection");
	internal_data->color3D_shader_u_locations.view = Renderer::shader_get_uniform_index(color3D_shader, "view");
	internal_data->color3D_shader_u_locations.model = Renderer::shader_get_uniform_index(color3D_shader, "model");

	internal_data->coordinate_grid_shader_u_locations.projection = Renderer::shader_get_uniform_index(coordinate_grid_shader, "projection");
	internal_data->coordinate_grid_shader_u_locations.view = Renderer::shader_get_uniform_index(coordinate_grid_shader, "view");
	internal_data->coordinate_grid_shader_u_locations.near_clip = Renderer::shader_get_uniform_index(coordinate_grid_shader, "near");
	internal_data->coordinate_grid_shader_u_locations.far_clip = Renderer::shader_get_uniform_index(coordinate_grid_shader, "far");

	internal_data->near_clip = 0.1f;
	internal_data->far_clip = 4000.0f;
	internal_data->fov = Math::deg_to_rad(45.0f);

	internal_data->projection_matrix = Math::mat_perspective(internal_data->fov, 1280.0f / 720.0f, internal_data->near_clip, internal_data->far_clip);

	GeometryConfig grid_geometry_config = {};
	grid_geometry_config.type = GeometryConfigType::Default;
	grid_geometry_config.default_config.vertex_size = sizeof(VertexCoordinateGrid);
	grid_geometry_config.default_config.vertex_count = 6;
	Renderer::geometry_init(&grid_geometry_config, &internal_data->coordinate_grid.geometry);
	SarrayRef<VertexCoordinateGrid> grid_vertices(&internal_data->coordinate_grid.geometry.vertices);
	for (uint32 i = 0; i < grid_vertices.capacity; i++)
		grid_vertices[i].index = i;

	Renderer::geometry_load(&internal_data->coordinate_grid.geometry);

	return true;

}

void render_view_world_editor_on_destroy(RenderView* self)
{
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	Renderer::geometry_unload(&internal_data->coordinate_grid.geometry);
	Renderer::geometry_destroy(&internal_data->coordinate_grid.geometry);
}

void render_view_world_editor_on_resize(RenderView* self, uint32 width, uint32 height)
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

static bool8 set_globals_color3D(RenderViewWorldInternalData* internal_data, Camera* camera)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->color3D_shader_id);
	Renderer::shader_bind_globals(shader);

	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->color3D_shader_u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->color3D_shader_u_locations.view, &camera->get_view()));

	return Renderer::shader_apply_globals(ShaderSystem::get_shader(internal_data->color3D_shader_id));
}

static bool8 set_locals_color3D(RenderViewWorldInternalData* internal_data, Math::Mat4* model)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->color3D_shader_id);
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->color3D_shader_u_locations.model, model));
	return true;
}

static bool8 set_globals_coordinate_grid(RenderViewWorldInternalData* internal_data, Camera* camera)
{
	Shader* shader = ShaderSystem::get_shader(internal_data->coordinate_grid_shader_id);
	Renderer::shader_bind_globals(shader);

	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->coordinate_grid_shader_u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->coordinate_grid_shader_u_locations.view, &camera->get_view()));
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->coordinate_grid_shader_u_locations.near_clip, &internal_data->near_clip));
	UNIFORM_APPLY_OR_FAIL(Renderer::shader_set_uniform(shader, internal_data->coordinate_grid_shader_u_locations.far_clip, &internal_data->far_clip));

	return Renderer::shader_apply_globals(ShaderSystem::get_shader(internal_data->coordinate_grid_shader_id));
}

bool8 render_view_world_editor_on_build_packet(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data)
{
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	return true;
}

void render_view_world_editor_on_end_frame(RenderView* self)
{
}

bool8 render_view_world_editor_on_render(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
{
	OPTICK_EVENT();

	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;
	Camera* world_camera = RenderViewSystem::get_bound_world_camera();

	if (!set_globals_color3D(internal_data, world_camera))
		SHMERROR("Failed to apply globals to color3D shader.");
	if (!set_globals_coordinate_grid(internal_data, world_camera))
		SHMERROR("Failed to apply globals to coordinate grid shader.");

	for (uint32 instance_i = 0; instance_i < self->instances.count; instance_i++)
	{
		RenderViewInstanceData* instance_data = &self->instances[instance_i];

		if (instance_data->shader_instance_id == Constants::max_u32)
			continue;

		bool8 instance_set = true;
		SHMERROR("Unknown shader for applying instance.");

		if (!instance_set)
			SHMERROR("Failed to apply instance.");
	}

	Renderer::RenderPass* renderpass = &self->renderpasses[0];

	ShaderId shader_id = ShaderId::invalid_value;
	Shader* shader = 0;

	if (!Renderer::renderpass_begin(renderpass, &renderpass->render_targets[render_target_index]))
	{
		SHMERROR("render_view_world_on_render - failed to begin renderpass!");
		return false;
	}

	for (uint32 geometry_i = 0; geometry_i < self->geometries.count; geometry_i++)
	{
		RenderViewGeometryData* render_data = &self->geometries[geometry_i];

		if (render_data->shader_id != shader_id)
		{
			shader_id = render_data->shader_id;
			shader = ShaderSystem::get_shader(shader_id);
			Renderer::shader_use(shader);
			Renderer::shader_bind_globals(shader);
		}

		if (render_data->shader_instance_id != Constants::max_u32)
			Renderer::shader_bind_instance(shader, render_data->shader_instance_id);

		if (render_data->object_index != Constants::max_u32)
		{
			Math::Mat4* model = &self->objects[render_data->object_index].model;
			if (shader_id == internal_data->color3D_shader_id)
				set_locals_color3D(internal_data, model);
		}

		Renderer::geometry_draw(render_data->geometry_data);
	}

	// NOTE: Drawing coordinate grid separately
	Shader* coordinate_grid_shader = ShaderSystem::get_shader(internal_data->coordinate_grid_shader_id);
	Renderer::shader_use(coordinate_grid_shader);
	Renderer::shader_bind_globals(coordinate_grid_shader);

	//Renderer::geometry_draw(&internal_data->coordinate_grid.geometry);

	if (!Renderer::renderpass_end(renderpass))
	{
		SHMERROR("render_view_world_on_render - draw_frame - failed to end renderpass!");
		return false;
	}

	return true;
}
