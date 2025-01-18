#include "RenderViewWorld.hpp"

#include <core/Event.hpp>
#include <core/Identifier.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <resources/Mesh.hpp>
#include <systems/GeometrySystem.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/CameraSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <utility/Sort.hpp>

#include <optick.h>

struct MaterialPhongShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 model;
	uint16 ambient_color;
	uint16 camera_position;
	uint16 diffuse_texture;
	uint16 specular_texture;
	uint16 normal_texture;
	uint16 render_mode;
	uint16 dir_light;
	uint16 p_lights;
	uint16 p_lights_count;

	uint16 properties;
};

struct TerrainShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 model;
	uint16 ambient_color;
	uint16 camera_position;
	uint16 render_mode;
	uint16 dir_light;
	uint16 p_lights;
	uint16 p_lights_count;

	uint16 properties;
	uint16 samplers[max_terrain_materials_count * 3];
};

struct Color3DShaderUniformLocations
{
	uint16 projection;
	uint16 view;
	uint16 model;
};

struct CoordinateGridShaderUniformLocations
{
	uint16 projection;
	uint16 view;
};

struct VertexCoordinateGrid
{
	uint32 index;
};

struct CoordinateGrid
{
	GeometryData geometry;
};

struct RenderViewWorldInternalData {
	Shader* material_phong_shader;
	MaterialPhongShaderUniformLocations material_phong_u_locations;

	Shader* terrain_shader;
	TerrainShaderUniformLocations terrain_u_locations;

	Shader* color3D_shader;
	Color3DShaderUniformLocations color3D_shader_u_locations;

	Shader* coordinate_grid_shader;
	CoordinateGridShaderUniformLocations coordinate_grid_shader_u_locations;

	float32 near_clip;
	float32 far_clip;
	float32 fov;
	uint32 render_mode;
	Math::Mat4 projection_matrix;
	Math::Vec4f ambient_color;

	LightingInfo lighting;

	CoordinateGrid coordinate_grid;

	Camera* camera;
};

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
		case Renderer::ViewMode::DEFAULT:
		{
			SHMDEBUG("Renderer mode set to default.");
			internal_data->render_mode = Renderer::ViewMode::DEFAULT;
			break;
		}
		case Renderer::ViewMode::LIGHTING:
		{
			SHMDEBUG("Renderer mode set to lighting.");
			internal_data->render_mode = Renderer::ViewMode::LIGHTING;
			break;
		}
		case Renderer::ViewMode::NORMALS:
		{
			SHMDEBUG("Renderer mode set to normals.");
			internal_data->render_mode = Renderer::ViewMode::NORMALS;
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

bool32 render_view_world_on_register(RenderView* self)
{

	self->internal_data.init(sizeof(RenderViewWorldInternalData), 0, AllocationTag::RENDERER);
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	internal_data->material_phong_u_locations.view = INVALID_ID16;
	internal_data->material_phong_u_locations.projection = INVALID_ID16;
	internal_data->material_phong_u_locations.diffuse_texture = INVALID_ID16;
	internal_data->material_phong_u_locations.specular_texture = INVALID_ID16;
	internal_data->material_phong_u_locations.normal_texture = INVALID_ID16;
	internal_data->material_phong_u_locations.camera_position = INVALID_ID16;
	internal_data->material_phong_u_locations.ambient_color = INVALID_ID16;
	internal_data->material_phong_u_locations.model = INVALID_ID16;
	internal_data->material_phong_u_locations.render_mode = INVALID_ID16;
	internal_data->material_phong_u_locations.dir_light = INVALID_ID16;
	internal_data->material_phong_u_locations.p_lights = INVALID_ID16;
	internal_data->material_phong_u_locations.p_lights_count = INVALID_ID16;
	internal_data->material_phong_u_locations.properties = INVALID_ID16;

	internal_data->terrain_u_locations.view = INVALID_ID16;
	internal_data->terrain_u_locations.projection = INVALID_ID16;
	internal_data->terrain_u_locations.camera_position = INVALID_ID16;
	internal_data->terrain_u_locations.ambient_color = INVALID_ID16;
	internal_data->terrain_u_locations.model = INVALID_ID16;
	internal_data->terrain_u_locations.render_mode = INVALID_ID16;
	internal_data->terrain_u_locations.dir_light = INVALID_ID16;
	internal_data->terrain_u_locations.p_lights = INVALID_ID16;
	internal_data->terrain_u_locations.p_lights_count = INVALID_ID16;
	internal_data->terrain_u_locations.properties = INVALID_ID16;
	for (uint32 i = 0; i < max_terrain_materials_count * 3; i++)
		internal_data->terrain_u_locations.samplers[i] = INVALID_ID16;

	internal_data->color3D_shader_u_locations.view = INVALID_ID16;
	internal_data->color3D_shader_u_locations.projection = INVALID_ID16;
	internal_data->color3D_shader_u_locations.model = INVALID_ID16;

	internal_data->coordinate_grid_shader_u_locations.view = INVALID_ID16;
	internal_data->coordinate_grid_shader_u_locations.projection = INVALID_ID16;

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_material_phong, &self->renderpasses[0]))
	{
		SHMERROR("Failed to create material phong shader.");
		return false;
	}

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_terrain, &self->renderpasses[0]))
	{
		SHMERROR("Failed to create terrain shader.");
		return false;
	}

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_color3D, &self->renderpasses[0]))
	{
		SHMERROR("Failed to create color 3d shader.");
		return false;
	}

	if (!ShaderSystem::create_shader_from_resource(Renderer::RendererConfig::builtin_shader_name_coordinate_grid, &self->renderpasses[1]))
	{
		SHMERROR("Failed to create coordinate grid shader.");
		return false;
	}

	internal_data->material_phong_shader = ShaderSystem::get_shader(self->custom_shader_name ? self->custom_shader_name : Renderer::RendererConfig::builtin_shader_name_material_phong);
	internal_data->terrain_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_terrain);
	internal_data->color3D_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_color3D);
	internal_data->coordinate_grid_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_coordinate_grid);

	internal_data->material_phong_u_locations.projection = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "projection");
	internal_data->material_phong_u_locations.view = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "view");
	internal_data->material_phong_u_locations.ambient_color = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "ambient_color");
	internal_data->material_phong_u_locations.camera_position = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "camera_position");
	internal_data->material_phong_u_locations.diffuse_texture = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "diffuse_texture");
	internal_data->material_phong_u_locations.specular_texture = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "specular_texture");
	internal_data->material_phong_u_locations.normal_texture = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "normal_texture");
	internal_data->material_phong_u_locations.model = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "model");
	internal_data->material_phong_u_locations.render_mode = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "mode");
	internal_data->material_phong_u_locations.dir_light = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "dir_light");
	internal_data->material_phong_u_locations.p_lights = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "p_lights");
	internal_data->material_phong_u_locations.p_lights_count = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "p_lights_count");
	internal_data->material_phong_u_locations.properties = ShaderSystem::get_uniform_index(internal_data->material_phong_shader, "properties");

	internal_data->terrain_u_locations.projection = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "projection");
	internal_data->terrain_u_locations.view = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "view");
	internal_data->terrain_u_locations.ambient_color = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "ambient_color");
	internal_data->terrain_u_locations.camera_position = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "camera_position");
	internal_data->terrain_u_locations.model = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "model");
	internal_data->terrain_u_locations.render_mode = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "mode");
	internal_data->terrain_u_locations.dir_light = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "dir_light");
	internal_data->terrain_u_locations.p_lights = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "p_lights");
	internal_data->terrain_u_locations.p_lights_count = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "p_lights_count");
	internal_data->terrain_u_locations.properties = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "properties");
	internal_data->terrain_u_locations.samplers[0] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "diffuse_texture_0");
	internal_data->terrain_u_locations.samplers[1] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "specular_texture_0");
	internal_data->terrain_u_locations.samplers[2] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "normal_texture_0");
	internal_data->terrain_u_locations.samplers[3] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "diffuse_texture_1");
	internal_data->terrain_u_locations.samplers[4] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "specular_texture_1");
	internal_data->terrain_u_locations.samplers[5] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "normal_texture_1");
	internal_data->terrain_u_locations.samplers[6] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "diffuse_texture_2");
	internal_data->terrain_u_locations.samplers[7] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "specular_texture_2");
	internal_data->terrain_u_locations.samplers[8] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "normal_texture_2");
	internal_data->terrain_u_locations.samplers[9] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "diffuse_texture_3");
	internal_data->terrain_u_locations.samplers[10] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "specular_texture_3");
	internal_data->terrain_u_locations.samplers[11] = ShaderSystem::get_uniform_index(internal_data->terrain_shader, "normal_texture_3");

	internal_data->color3D_shader_u_locations.projection = ShaderSystem::get_uniform_index(internal_data->color3D_shader, "projection");
	internal_data->color3D_shader_u_locations.view = ShaderSystem::get_uniform_index(internal_data->color3D_shader, "view");
	internal_data->color3D_shader_u_locations.model = ShaderSystem::get_uniform_index(internal_data->color3D_shader, "model");

	internal_data->coordinate_grid_shader_u_locations.projection = ShaderSystem::get_uniform_index(internal_data->coordinate_grid_shader, "projection");
	internal_data->coordinate_grid_shader_u_locations.view = ShaderSystem::get_uniform_index(internal_data->coordinate_grid_shader, "view");

	internal_data->near_clip = 0.1f;
	internal_data->far_clip = 4000.0f;
	internal_data->fov = Math::deg_to_rad(45.0f);

	internal_data->projection_matrix = Math::mat_perspective(internal_data->fov, 1280.0f / 720.0f, internal_data->near_clip, internal_data->far_clip);
	internal_data->camera = CameraSystem::get_default_camera();
	internal_data->ambient_color = { 0.25f, 0.25f, 0.25f, 1.0f };

	GeometryData* grid_geometry = &internal_data->coordinate_grid.geometry;
	grid_geometry->id = INVALID_ID;
	grid_geometry->vertex_size = sizeof(VertexCoordinateGrid);
	grid_geometry->vertex_count = 6;
	grid_geometry->vertices.init(grid_geometry->vertex_size * grid_geometry->vertex_count, 0);
	SarrayRef<byte, VertexCoordinateGrid> grid_vertices(&grid_geometry->vertices);
	for (uint32 i = 0; i < grid_vertices.capacity; i++)
		grid_vertices[i].index = i;

	Renderer::geometry_load(grid_geometry);

	Event::event_register((uint16)SystemEventCode::SET_RENDER_MODE, self, on_event);
	Event::event_register((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);

	return true;

}

void render_view_world_on_unregister(RenderView* self)
{
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	Renderer::geometry_unload(&internal_data->coordinate_grid.geometry);

	Event::event_unregister((uint16)SystemEventCode::SET_RENDER_MODE, self, on_event);
	Event::event_unregister((uint16)SystemEventCode::DEFAULT_RENDERTARGET_REFRESH_REQUIRED, self, on_event);
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

static bool32 set_globals_material_phong(RenderViewWorldInternalData* internal_data)
{
	MaterialPhongShaderUniformLocations u_locations = internal_data->material_phong_u_locations;

	Math::Vec3f camera_position = internal_data->camera->get_position();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, &internal_data->camera->get_view()));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.ambient_color, &internal_data->ambient_color));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.camera_position, &camera_position));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.render_mode, &internal_data->render_mode));

	if (internal_data->lighting.dir_light)
	{
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.dir_light, internal_data->lighting.dir_light));
	}

	if (internal_data->lighting.p_lights)
	{
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, &internal_data->lighting.p_lights_count));
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights, internal_data->lighting.p_lights));
	}
	else
	{
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, 0));
	}

	return true;
}

static bool32 set_instance_material_phong(MaterialPhongShaderUniformLocations u_locations, Renderer::InstanceRenderData instance, Math::Mat4* model)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.properties, instance.instance_properties));

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.diffuse_texture, instance.texture_maps[0]));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.specular_texture, instance.texture_maps[1]));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.normal_texture, instance.texture_maps[2]));

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.model, model));

	return true;
}

static bool32 set_globals_terrain(RenderViewWorldInternalData* internal_data)
{
	TerrainShaderUniformLocations u_locations = internal_data->terrain_u_locations;

	Math::Vec3f camera_position = internal_data->camera->get_position();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, &internal_data->camera->get_view()));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.ambient_color, &internal_data->ambient_color));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.camera_position, &camera_position));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.render_mode, &internal_data->render_mode));

	if (internal_data->lighting.dir_light)
	{
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.dir_light, internal_data->lighting.dir_light));
	}

	if (internal_data->lighting.p_lights)
	{
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, &internal_data->lighting.p_lights_count));
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights, internal_data->lighting.p_lights));
	}
	else
	{
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, 0));
	}

	return true;
}

static bool32 set_instance_terrain(TerrainShaderUniformLocations u_locations, Renderer::InstanceRenderData instance, Math::Mat4* model)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.properties, instance.instance_properties));
	for (uint32 i = 0; i < instance.texture_maps_count; i++)
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.samplers[i], instance.texture_maps[i]));

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.model, model));

	return true;
}

static bool32 set_globals_color3D(RenderViewWorldInternalData* internal_data)
{
	Color3DShaderUniformLocations u_locations = internal_data->color3D_shader_u_locations;

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, &internal_data->camera->get_view()));

	return true;
}

static bool32 set_instance_color3D(Color3DShaderUniformLocations u_locations, Math::Mat4* model)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.model, model));

	return true;
}

static bool32 set_globals_coordinate_grid(RenderViewWorldInternalData* internal_data)
{
	CoordinateGridShaderUniformLocations u_locations = internal_data->coordinate_grid_shader_u_locations;

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, &internal_data->camera->get_view()));

	return true;
}

bool32 render_view_world_on_build_packet(RenderView* self, Memory::LinearAllocator* frame_allocator, const RenderViewPacketData* packet_data)
{
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	if (packet_data->renderpass_id + 1 > self->renderpasses.capacity)
	{
		SHMERROR("Invalid renderpass id supplied in packet data!");
		return false;
	}

	// TODO: Find a way to properly handle per object lighting
	if (!internal_data->lighting.dir_light)
		internal_data->lighting = packet_data->lighting;
		
	self->geometries.copy_memory(packet_data->geometries, packet_data->geometries_count, self->geometries.count);

	return true;

}

void render_view_world_on_end_frame(RenderView* self)
{
	self->geometries.clear();
}

bool32 render_view_world_on_render(RenderView* self, Memory::LinearAllocator* frame_allocator, uint32 frame_number, uint64 render_target_index)
{

	struct GeometryDistance
	{
		Renderer::ObjectRenderData g;
		float32 dist;

		SHMINLINE bool8 operator<=(const GeometryDistance& other) { return dist <= other.dist; }
		SHMINLINE bool8 operator>=(const GeometryDistance& other) { return dist >= other.dist; }
	};

	OPTICK_EVENT();

	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	void* sorted_geometries_block = frame_allocator->allocate(sizeof(Renderer::ObjectRenderData) * self->geometries.count);
	Darray<Renderer::ObjectRenderData> sorted_geometries(self->geometries.count, 0, AllocationTag::RENDERER, sorted_geometries_block);

	void* transparent_geometries_block = frame_allocator->allocate(sizeof(Renderer::ObjectRenderData) * self->geometries.count);
	Darray<GeometryDistance> transparent_geometries(self->geometries.count, 0, AllocationTag::RENDERER, transparent_geometries_block);

	for (uint32 i = 0; i < self->geometries.count; i++)
	{
		Renderer::ObjectRenderData* g_data = &self->geometries[i];

		if (!g_data->has_transparency)
		{
			sorted_geometries.emplace(*g_data);
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
		Renderer::ObjectRenderData* render_data = &sorted_geometries[sorted_geometries.emplace()];
		*render_data = transparent_geometries[i].g;
	}

	const uint32 texture_maps_buffer_size = 12;
	TextureMap* texture_maps_buffer[texture_maps_buffer_size];

	Math::Vec3f view_position = internal_data->camera->get_position();

	Renderer::RenderPass* objects_renderpass = &self->renderpasses[0];
	Renderer::RenderPass* coordinate_grid_renderpass = &self->renderpasses[1];
		
	uint32 shader_id = INVALID_ID;
	Shader* current_shader = 0;

	if (!Renderer::renderpass_begin(objects_renderpass, &objects_renderpass->render_targets[render_target_index]))
	{
		SHMERROR("render_view_world_on_render - failed to begin renderpass!");
		return false;
	}

	for (uint32 geometry_i = 0; geometry_i < sorted_geometries.count; geometry_i++)
	{
		Renderer::ObjectRenderData* object = &sorted_geometries[geometry_i];

		if (object->shader_id != shader_id)
		{
			shader_id = object->shader_id;
			ShaderSystem::use_shader(shader_id);

			bool32 globals_set = false;
			if (shader_id == internal_data->material_phong_shader->id)
			{
				globals_set = set_globals_material_phong(internal_data);
				current_shader = internal_data->material_phong_shader;
			}
			else if (shader_id == internal_data->terrain_shader->id)
			{
				globals_set = set_globals_terrain(internal_data);
				current_shader = internal_data->terrain_shader;
			}
			else if (shader_id == internal_data->color3D_shader->id)
			{
				globals_set = set_globals_color3D(internal_data);
				current_shader = internal_data->color3D_shader;
			}

			if (globals_set)
			{
				Renderer::shader_apply_globals(current_shader);
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
		if (object->get_instance_render_data)
		{
			object->get_instance_render_data(object->render_object, &instance);
			ShaderSystem::bind_instance(instance.shader_instance_id);
		}

		bool32 instance_set = false;
		if (shader_id == internal_data->material_phong_shader->id)
			instance_set = set_instance_material_phong(internal_data->material_phong_u_locations, instance, &object->model);
		else if (shader_id == internal_data->terrain_shader->id)
			instance_set = set_instance_terrain(internal_data->terrain_u_locations, instance, &object->model);
		else if (shader_id == internal_data->color3D_shader->id)
			set_instance_color3D(internal_data->color3D_shader_u_locations, &object->model);
		else
			SHMERROR("Unknown shader or failed to apply instance to shader.");

		if (instance_set)
			Renderer::shader_apply_instance(ShaderSystem::get_shader(shader_id));

		Renderer::geometry_draw(object->geometry_data);
	}

	if (!Renderer::renderpass_end(objects_renderpass))
	{
		SHMERROR("render_view_world_on_render - draw_frame - failed to end renderpass!");
		return false;
	}


	if (!renderpass_begin(coordinate_grid_renderpass, &coordinate_grid_renderpass->render_targets[render_target_index]))
	{
		SHMERROR("render_view_world_on_render - failed to begin renderpass!");
		return false;
	}

	// NOTE: Drawing coordinate grid separately
	current_shader = internal_data->coordinate_grid_shader;
	ShaderSystem::use_shader(current_shader->id);

	set_globals_coordinate_grid(internal_data);
	Renderer::shader_apply_globals(current_shader);

	Renderer::geometry_draw(&internal_data->coordinate_grid.geometry);

	if (!renderpass_end(coordinate_grid_renderpass))
	{
		SHMERROR("render_view_world_on_render - draw_frame - failed to end renderpass!");
		return false;
	}

	current_shader = 0;

	return true;
}
