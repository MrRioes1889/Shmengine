#include "RenderViewWorld.hpp"

#include <core/Event.hpp>
#include <core/Identifier.hpp>
#include <core/FrameData.hpp>
#include <utility/Math.hpp>
#include <utility/math/Transform.hpp>
#include <resources/loaders/ShaderLoader.hpp>
#include <resources/Mesh.hpp>
#include <systems/GeometrySystem.hpp>
#include <systems/ShaderSystem.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <renderer/RendererFrontend.hpp>
#include <renderer/Camera.hpp>
#include <utility/Sort.hpp>

#include <optick.h>

struct RenderViewWorldInternalData {
	Shader* material_phong_shader;
	MaterialPhongShaderUniformLocations material_phong_u_locations;

	Shader* terrain_shader;
	TerrainShaderUniformLocations terrain_u_locations;

	Shader* color3D_shader;
	Color3DShaderUniformLocations color3D_shader_u_locations;

	float32 near_clip;
	float32 far_clip;
	float32 fov;
	uint32 render_mode;
	Math::Mat4 projection_matrix;
	Math::Vec4f ambient_color;

	LightingInfo lighting;
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
	}

	return false;
}

bool32 render_view_world_on_create(RenderView* self)
{

	self->internal_data.init(sizeof(RenderViewWorldInternalData), 0, AllocationTag::Renderer);
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	internal_data->material_phong_u_locations.view = Constants::max_u16;
	internal_data->material_phong_u_locations.projection = Constants::max_u16;
	internal_data->material_phong_u_locations.diffuse_texture = Constants::max_u16;
	internal_data->material_phong_u_locations.specular_texture = Constants::max_u16;
	internal_data->material_phong_u_locations.normal_texture = Constants::max_u16;
	internal_data->material_phong_u_locations.camera_position = Constants::max_u16;
	internal_data->material_phong_u_locations.ambient_color = Constants::max_u16;
	internal_data->material_phong_u_locations.model = Constants::max_u16;
	internal_data->material_phong_u_locations.render_mode = Constants::max_u16;
	internal_data->material_phong_u_locations.dir_light = Constants::max_u16;
	internal_data->material_phong_u_locations.p_lights = Constants::max_u16;
	internal_data->material_phong_u_locations.p_lights_count = Constants::max_u16;
	internal_data->material_phong_u_locations.properties = Constants::max_u16;

	internal_data->terrain_u_locations.view = Constants::max_u16;
	internal_data->terrain_u_locations.projection = Constants::max_u16;
	internal_data->terrain_u_locations.camera_position = Constants::max_u16;
	internal_data->terrain_u_locations.ambient_color = Constants::max_u16;
	internal_data->terrain_u_locations.model = Constants::max_u16;
	internal_data->terrain_u_locations.render_mode = Constants::max_u16;
	internal_data->terrain_u_locations.dir_light = Constants::max_u16;
	internal_data->terrain_u_locations.p_lights = Constants::max_u16;
	internal_data->terrain_u_locations.p_lights_count = Constants::max_u16;
	internal_data->terrain_u_locations.properties = Constants::max_u16;
	for (uint32 i = 0; i < Constants::max_terrain_materials_count * 3; i++)
		internal_data->terrain_u_locations.samplers[i] = Constants::max_u16;

	internal_data->color3D_shader_u_locations.view = Constants::max_u16;
	internal_data->color3D_shader_u_locations.projection = Constants::max_u16;
	internal_data->color3D_shader_u_locations.model = Constants::max_u16;

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

	internal_data->material_phong_shader = ShaderSystem::get_shader(self->custom_shader_name ? self->custom_shader_name : Renderer::RendererConfig::builtin_shader_name_material_phong);
	internal_data->terrain_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_terrain);
	internal_data->color3D_shader = ShaderSystem::get_shader(Renderer::RendererConfig::builtin_shader_name_color3D);

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

	internal_data->near_clip = 0.1f;
	internal_data->far_clip = 4000.0f;
	internal_data->fov = Math::deg_to_rad(45.0f);

	internal_data->projection_matrix = Math::mat_perspective(internal_data->fov, 1280.0f / 720.0f, internal_data->near_clip, internal_data->far_clip);
	internal_data->ambient_color = { 0.25f, 0.25f, 0.25f, 1.0f };

	Event::event_register((uint16)SystemEventCode::SET_RENDER_MODE, self, on_event);

	return true;

}

void render_view_world_on_destroy(RenderView* self)
{
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	Event::event_unregister((uint16)SystemEventCode::SET_RENDER_MODE, self, on_event);
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

static bool32 set_globals_material_phong(RenderViewWorldInternalData* internal_data, Camera* camera)
{
	MaterialPhongShaderUniformLocations u_locations = internal_data->material_phong_u_locations;
	ShaderSystem::bind_shader(internal_data->material_phong_shader->id);
	ShaderSystem::bind_globals();

	Math::Vec3f camera_position = camera->get_position();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, &camera->get_view()));
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
		uint32 no_lights = 0;
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, &no_lights));
	}

	return Renderer::shader_apply_globals(internal_data->material_phong_shader);
}

static bool32 set_instance_material_phong(RenderViewWorldInternalData* internal_data, RenderViewInstanceData instance)
{
	MaterialPhongShaderUniformLocations u_locations = internal_data->material_phong_u_locations;
	ShaderSystem::bind_shader(internal_data->material_phong_shader->id);
	ShaderSystem::bind_instance(instance.shader_instance_id);

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.properties, instance.instance_properties));

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.diffuse_texture, instance.texture_maps[0]));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.specular_texture, instance.texture_maps[1]));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.normal_texture, instance.texture_maps[2]));

	return Renderer::shader_apply_instance(internal_data->material_phong_shader);
}

static bool32 set_locals_material_phong(RenderViewWorldInternalData* internal_data, Math::Mat4* model)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->material_phong_u_locations.model, model));
	return true;
}

static bool32 set_globals_terrain(RenderViewWorldInternalData* internal_data, Camera* camera)
{

	TerrainShaderUniformLocations u_locations = internal_data->terrain_u_locations;
	ShaderSystem::bind_shader(internal_data->terrain_shader->id);
	ShaderSystem::bind_globals();

	Math::Vec3f camera_position = camera->get_position();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.view, &camera->get_view()));
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
		uint32 no_lights = 0;
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(u_locations.p_lights_count, &no_lights));
	}

	return Renderer::shader_apply_globals(internal_data->terrain_shader);
}

static bool32 set_instance_terrain(RenderViewWorldInternalData* internal_data, RenderViewInstanceData instance)
{
	ShaderSystem::bind_shader(internal_data->terrain_shader->id);
	ShaderSystem::bind_instance(instance.shader_instance_id);

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->terrain_u_locations.properties, instance.instance_properties));
	for (uint32 i = 0; i < instance.texture_maps_count; i++)
		UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->terrain_u_locations.samplers[i], instance.texture_maps[i]));

	return Renderer::shader_apply_instance(internal_data->terrain_shader);
}

static bool32 set_locals_terrain(RenderViewWorldInternalData* internal_data, Math::Mat4* model)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->terrain_u_locations.model, model));
	return true;
}

static bool32 set_globals_color3D(RenderViewWorldInternalData* internal_data, Camera* camera)
{
	ShaderSystem::bind_shader(internal_data->color3D_shader->id);
	ShaderSystem::bind_globals();

	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->color3D_shader_u_locations.projection, &internal_data->projection_matrix));
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->color3D_shader_u_locations.view, &camera->get_view()));

	return Renderer::shader_apply_globals(internal_data->color3D_shader);
}

static bool32 set_locals_color3D(RenderViewWorldInternalData* internal_data, Math::Mat4* model)
{
	UNIFORM_APPLY_OR_FAIL(ShaderSystem::set_uniform(internal_data->color3D_shader_u_locations.model, model));
	return true;
}

bool32 render_view_world_on_build_packet(RenderView* self, FrameData* frame_data, const RenderViewPacketData* packet_data)
{
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	// TODO: Figure out how to do real per object lighting
	if (!internal_data->lighting.dir_light)
	{
		for (uint32 i = self->objects.count - packet_data->objects_pushed_count; i < self->objects.count; i++)
		{
			if (self->objects[i].lighting.dir_light)
				internal_data->lighting = self->objects[i].lighting;
		}
	}

	return true;

}

void render_view_world_on_end_frame(RenderView* self)
{
	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;

	internal_data->lighting = {};
}

bool32 render_view_world_on_render(RenderView* self, FrameData* frame_data, uint32 frame_number, uint64 render_target_index)
{

	struct GeometryDistance
	{
		RenderViewGeometryData g;
		float32 dist;

		SHMINLINE bool8 operator<=(const GeometryDistance& other) { return dist <= other.dist; }
		SHMINLINE bool8 operator>=(const GeometryDistance& other) { return dist >= other.dist; }
	};

	OPTICK_EVENT();	

	RenderViewWorldInternalData* internal_data = (RenderViewWorldInternalData*)self->internal_data.data;
	Camera* world_camera = RenderViewSystem::get_bound_world_camera();

	{
		void* sorted_geometries_block = frame_data->frame_allocator.allocate(sizeof(RenderViewGeometryData) * self->geometries.count);
		Darray<RenderViewGeometryData> sorted_geometries(self->geometries.count, 0, AllocationTag::Renderer, sorted_geometries_block);

		void* transparent_geometries_block = frame_data->frame_allocator.allocate(sizeof(RenderViewGeometryData) * self->geometries.count);
		Darray<GeometryDistance> transparent_geometries(self->geometries.count, 0, AllocationTag::Renderer, transparent_geometries_block);

		for (uint32 i = 0; i < self->geometries.count; i++)
		{
			RenderViewGeometryData* g_data = &self->geometries[i];

			if (!g_data->has_transparency)
			{
				sorted_geometries.emplace(*g_data);
			}
			else
			{
				Math::Vec3f center = Math::vec_transform(g_data->geometry_data->center, self->objects[g_data->object_index].model);
				float32 distance = Math::vec_distance(center, world_camera->get_position());

				GeometryDistance* g_dist = &transparent_geometries[transparent_geometries.emplace()];
				g_dist->dist = Math::abs(distance);
				g_dist->g = *g_data;
			}
		}

		quick_sort(transparent_geometries.data, 0, transparent_geometries.count - 1, false);
		for (uint32 i = 0; i < transparent_geometries.count; i++)
			sorted_geometries.emplace(transparent_geometries[i].g);			

		self->geometries.copy_memory(sorted_geometries.data, sorted_geometries.count, 0);
	}
	
	if (!set_globals_material_phong(internal_data, world_camera))
		SHMERROR("Failed to apply globals to material phong shader.");
	if (!set_globals_terrain(internal_data, world_camera))
		SHMERROR("Failed to apply globals to terrain shader.");
	if (!set_globals_color3D(internal_data, world_camera))
		SHMERROR("Failed to apply globals to color3D shader.");

	for (uint32 instance_i = 0; instance_i < self->instances.count; instance_i++)
	{
		RenderViewInstanceData* instance_data = &self->instances[instance_i];

		if (instance_data->shader_instance_id == Constants::max_u32)
			continue;

		bool32 instance_set = true;
		if (instance_data->shader_id == internal_data->material_phong_shader->id)
			instance_set = set_instance_material_phong(internal_data, *instance_data);
		else if (instance_data->shader_id == internal_data->terrain_shader->id)
			instance_set = set_instance_terrain(internal_data, *instance_data);
		else
			SHMERROR("Unknown shader for applying instance.");

		if (!instance_set)
			SHMERROR("Failed to apply instance.");
	}

	Renderer::RenderPass* renderpass = &self->renderpasses[0];
		
	ShaderId shader_id = ShaderId::invalid_value;

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
			ShaderSystem::use_shader(shader_id);
			ShaderSystem::bind_globals();
		}

		if (render_data->shader_instance_id != Constants::max_u32)
			ShaderSystem::bind_instance(render_data->shader_instance_id);

		if (render_data->object_index != Constants::max_u32)
		{
			Math::Mat4* model = &self->objects[render_data->object_index].model;
			if (shader_id == internal_data->material_phong_shader->id)
				set_locals_material_phong(internal_data, model);
			else if (shader_id == internal_data->terrain_shader->id)
				set_locals_terrain(internal_data, model);
			else if (shader_id == internal_data->color3D_shader->id)
				set_locals_color3D(internal_data, model);
		}

		Renderer::geometry_draw(render_data->geometry_data);
	}

	if (!Renderer::renderpass_end(renderpass))
	{
		SHMERROR("render_view_world_on_render - draw_frame - failed to end renderpass!");
		return false;
	}

	return true;
}
