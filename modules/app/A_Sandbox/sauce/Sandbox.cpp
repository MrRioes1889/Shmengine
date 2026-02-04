#include "Sandbox.hpp"
#include "ApplicationState.hpp"
#include "Defines.hpp"
#include "Keybinds.hpp"
#include "ui/DebugConsole.hpp"

#include <containers/Darray.hpp>
#include <core/Logging.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <core/Clock.hpp>
#include <core/FrameData.hpp>
#include <core/Identifier.hpp>
#include <utility/Utility.hpp>
#include <utility/String.hpp>
#include <renderer/RendererFrontend.hpp>
#include <utility/Sort.hpp>

// TODO: temp
#include <utility/math/Transform.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/FontSystem.hpp>
#include <systems/ShaderSystem.hpp>
// end

ApplicationState* app_state = 0;

static void register_events();
static void unregister_events();

bool8 application_load_config(ApplicationConfig* out_config)
{
	out_config->app_frame_data_size = sizeof(ApplicationFrameData);
	out_config->state_size = sizeof(ApplicationState);

	out_config->start_pos_x = 100;
	out_config->start_pos_y = 100;
	out_config->start_width = 1600;
	out_config->start_height = 900;
	out_config->name = "Shmengine Sandbox";   
	out_config->renderer_module_name = "M_VulkanRenderer";

	out_config->limit_framerate = true;

	return true;
}

bool8 application_init(Application* app_inst)
{
	app_state = (ApplicationState*)app_inst->state;

	register_events();
	add_keymaps();

	app_state->world_camera = RenderViewSystem::get_bound_world_camera();
	app_state->world_camera->set_position({ 10.5f, 5.0f, 9.5f });
	app_state->allocation_count = 0;

	if (!FontSystem::load_font("Noto Serif 21px", "NotoSerif_21", 21) ||
		!FontSystem::load_font("Roboto Mono 21px", "RobotoMono_21", 21) ||
		!FontSystem::load_font("Martian Mono", "MartianMono", 21))
	{
		SHMERROR("Failed to load default fonts.");
		return false;
	}

	app_state->debug_console.init();

	UITextConfig ui_text_config = {};
	ui_text_config.font_name = "Martian Mono";
	ui_text_config.font_size = 21;
	ui_text_config.text_content = "Some täest text,\n\tyo!";

	if (!ui_text_init(&ui_text_config, &app_state->debug_info_text))
	{
		SHMERROR("Failed to load basic ui truetype text.");
		return false;
	}
	ui_text_set_position(&app_state->debug_info_text, { 500, 550, 0 });

	if (!gizmo3D_init(&app_state->editor_gizmo))
	{
		SHMERROR("Failed to load editor gizmo text.");
		return false;
	}
	Math::transform_translate(app_state->editor_gizmo.xform, { 0.0f, 5.0f, 1.0f });

	// Load up some test UI geometry.
	/*MeshGeometryConfig ui_config = {};
	GeometryConfig ui_g_config = {};
	ui_config.data_config = &ui_g_config;
		
	ui_config.material_name = "test_ui_material";
	CString::copy("test_ui_mesh", ui_config.data_config->name, Constants::max_mesh_name_length);

	ui_config.data_config->vertex_size = sizeof(Renderer::Vertex2D);
	ui_config.data_config->vertex_count = 4;
	ui_config.data_config->vertices.init(ui_config.data_config->vertex_size * ui_config.data_config->vertex_count, 0);
	ui_config.data_config->index_count = 6;
	ui_config.data_config->indices.init(ui_config.data_config->index_count, 0);
	
	Renderer::Vertex2D* uiverts = (Renderer::Vertex2D*)&ui_config.data_config->vertices[0];

	const float32 w = 200.0f;//1077.0f * 0.25f;
	const float32 h = 300.0f;//1278.0f * 0.25f;
	uiverts[0].position.x = 0.0f;  // 0    3
	uiverts[0].position.y = 0.0f;  //
	uiverts[0].tex_coordinates.x = 0.0f;  //
	uiverts[0].tex_coordinates.y = 0.0f;  // 2    1

	uiverts[1].position.y = h;
	uiverts[1].position.x = w;
	uiverts[1].tex_coordinates.x = 1.0f;
	uiverts[1].tex_coordinates.y = 1.0f;

	uiverts[2].position.x = 0.0f;
	uiverts[2].position.y = h;
	uiverts[2].tex_coordinates.x = 0.0f;
	uiverts[2].tex_coordinates.y = 1.0f;

	uiverts[3].position.x = w;
	uiverts[3].position.y = 0.0;
	uiverts[3].tex_coordinates.x = 1.0f;
	uiverts[3].tex_coordinates.y = 0.0f;

	// Indices - counter-clockwise
	ui_config.data_config->indices[0] = 2;
	ui_config.data_config->indices[1] = 1;
	ui_config.data_config->indices[2] = 0;
	ui_config.data_config->indices[3] = 3;
	ui_config.data_config->indices[4] = 0;
	ui_config.data_config->indices[5] = 1;

	// Get UI geometry from config.
	app_state->ui_meshes.init(1, 0);
	Mesh* ui_mesh = &app_state->ui_meshes[app_state->ui_meshes.emplace()];
	*mesh_init()
	ui_mesh->unique_id = identifier_acquire_new_id(ui_mesh);
	ui_mesh->geometries.init(1, 0);
	ui_mesh->geometries.emplace();
	ui_mesh->geometries[0].g_data = GeometrySystem::acquire_from_config(ui_config.data_config, true);
	CString::copy(ui_config.material_name, ui_mesh->geometries[0].material_name, Constants::max_material_name_length);
	ui_mesh->geometries[0].material = MaterialSystem::acquire(ui_mesh->geometries[0].material_name, true);
	ui_mesh->transform = Math::transform_create();
	ui_mesh->generation = 0;*/

	app_state->test_raycast_lines.init(32, 0);

	return true;
}

void application_shutdown()
{
	// TODO: temp
	scene_destroy(&app_state->main_scene);
	ui_text_destroy(&app_state->debug_info_text);
	gizmo3D_destroy(&app_state->editor_gizmo);

	for (uint32 i = 0; i < app_state->test_raycast_lines.count; i++)
		line3D_destroy(&app_state->test_raycast_lines[i]);

	app_state->debug_console.destroy();

	app_state->ui_meshes.free_data();

	unregister_events();
	// end

}

bool8 application_update(FrameData* frame_data)
{
	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	scene_update(&app_state->main_scene);
	frame_data->frame_allocator.free_all_data();

	uint32 allocation_count = Memory::get_current_allocation_count();
	app_state->allocation_count = allocation_count;

	if (Input::is_cursor_clipped())
	{
		Math::Vec2i mouse_offset = Input::get_internal_mouse_offset();
		const float32 mouse_sensitivity = 0.02f;
		if (mouse_offset.x || mouse_offset.y)
		{
			float32 yaw = -mouse_offset.x * mouse_sensitivity;
			float32 pitch = -mouse_offset.y * mouse_sensitivity * ((float32)app_state->height / (float32)app_state->width);
			app_state->world_camera->yaw(yaw);
			app_state->world_camera->pitch(pitch);
		}	
	}

	if (app_state->main_scene.state == ResourceState::Initialized)
	{
		Mesh* cube1 = scene_get_mesh(&app_state->main_scene, "cube_1");
		Mesh* cube2 = scene_get_mesh(&app_state->main_scene, "cube_2");
		Mesh* cube3 = scene_get_mesh(&app_state->main_scene, "cube_3");
		//PointLight* p_light = scene_get_point_light(&app_state->main_scene, 0);
		
		Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_UP, 1.5f * (float32)frame_data->delta_time, true);
		/*Math::transform_rotate(cube1->transform, rotation);
		Math::transform_rotate(cube2->transform, rotation);
		Math::transform_rotate(cube3->transform, rotation);*/

		/*
		p_light->color =
		{
			clamp(Math::sin((float32)frame_data->total_time * 0.75f), 0.0f, 1.0f),
			clamp(Math::sin((float32)frame_data->total_time * 0.25f), 0.0f, 1.0f),
			clamp(Math::sin((float32)frame_data->total_time * 0.5f), 0.0f, 1.0f),
			1.0f
		};
		static float32 starting_position = p_light->position.z;
		p_light->position.z = starting_position + Math::sin((float32)frame_data->total_time);
		*/
	}	

	static float64 last_frametime = 0.0;
	static float64 last_logictime = 0.0;
	static float64 last_rendertime = 0.0;

	static float64 times_update_timer = 0;
	times_update_timer += metrics_last_frametime();
	if (times_update_timer > 1.0)
	{
		last_frametime = metrics_last_frametime();
		last_logictime = metrics_logic_time();
		last_rendertime = metrics_render_time();
		times_update_timer = 0.0;
	}

	Math::Vec2i mouse_pos = Input::get_mouse_position();

	Math::Vec3f pos = app_state->world_camera->get_position();
	Math::Vec3f rot = app_state->world_camera->get_rotation();

	Math::Vec3f fwd = app_state->world_camera->get_forward();
	Math::Vec3f right = app_state->world_camera->get_right();
	Math::Vec3f up = app_state->world_camera->get_up();
	app_state->camera_frustum = Math::frustum_create(app_state->world_camera->get_position(), fwd, right, up, (float32)app_state->width / (float32)app_state->height, Math::deg_to_rad(45.0f), 0.1f, 1000.0f);

	char ui_text_buffer[512];
	CString::safe_print_s<uint32, uint32, int32, int32, float32, float32, float32, float32, float32, float32, float64, float64, float64>
		(ui_text_buffer, 512, "Object Hovered ID: %u\nWorld geometry count: %u\nMouse Pos : [%i, %i]\tCamera Pos : [%f3, %f3, %f3]\nCamera Rot : [%f3, %f3, %f3]\n\nLast frametime: %lf4 ms\nLogic: %lf4 / Render: %lf4",
			app_state->hovered_object_id, frame_data->drawn_geometry_count, mouse_pos.x, mouse_pos.y, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, last_frametime * 1000.0, last_logictime * 1000.0, last_rendertime * 1000.0);

	ui_text_set_text(&app_state->debug_info_text, ui_text_buffer);

	gizmo3D_update(&app_state->editor_gizmo);

	app_state->debug_console.update();

	return true;
}

bool8 application_render(FrameData* frame_data)
{
	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	frame_data->drawn_geometry_count = 0;

	ShaderId ui_shader_id = ShaderSystem::get_ui_shader_id();
	ShaderId color3D_shader_id = ShaderSystem::get_color3D_shader_id();

	RenderViewId ui_view_id = RenderViewSystem::get_id("Builtin.UI");

	if (app_state->main_scene.state == ResourceState::Initialized)
		scene_draw(&app_state->main_scene, &app_state->camera_frustum, frame_data);

	RenderViewSystem::lines3D_draw(app_state->test_raycast_lines.data, app_state->test_raycast_lines.count, frame_data);

	//RenderViewSystem::gizmo3D_draw(&app_state->editor_gizmo, frame_data, app_state->world_camera);

	//RenderViewSystem::meshes_draw(app_state->ui_meshes.data, app_state->ui_meshes.count, {}, frame_data, 0, ui_view_id, ui_shader_id);

	RenderViewSystem::ui_text_draw(&app_state->debug_info_text, frame_data);

	if (app_state->debug_console.is_visible())
	{
		UIText* console_text = app_state->debug_console.get_text();
		RenderViewSystem::ui_text_draw(console_text, frame_data);

		UIText* entry_text = app_state->debug_console.get_entry_text();
		RenderViewSystem::ui_text_draw(entry_text, frame_data);
	}

	return true;
}

void application_on_resize(uint32 width, uint32 height)
{

	if (!app_state)
		return;

	app_state->width = width;
	app_state->height = height;

	ui_text_set_position(&app_state->debug_info_text, { 20.0f, (float32)app_state->height - 150.0f, 0.0f });

}

void application_on_module_reload(void* application_state)
{
	app_state = (ApplicationState*)application_state;

	register_events();
	app_state->debug_console.on_module_reload();
	add_keymaps();
}

void application_on_module_unload()
{
	unregister_events();
	app_state->debug_console.on_module_unload();
	Input::clear_keymaps();
}

static bool8 application_on_mousebutton_released(uint16 code, void* sender, void* listener_inst, EventData data)
{
	if (app_state->main_scene.state != ResourceState::Initialized)
		return false;

	if (data.ui8[0] == MouseButton::LMB)
	{
		int16 x = data.i16[1];
		int16 y = data.i16[2];

		Math::Mat4 view = app_state->world_camera->get_view();
		Math::Vec3f origin = app_state->world_camera->get_position();
		Math::Mat4 projection = Math::mat_perspective(Math::deg_to_rad(45.0f), (float32)app_state->width / app_state->height, 0.1f, 4000.0f);

		Math::Ray3D ray = Math::ray3D_create_from_screen({ (float32)x, (float32)y }, { (float32)app_state->width, (float32)app_state->height }, origin, view, projection);
		Math::Ray3DHitInfo hit_info = scene_raycast(&app_state->main_scene, ray);

		if (hit_info.type == Math::Ray3DHitType::NONE)
		{
			SHMDEBUG("Raycast: No object hit.");
			return false;
		}

		SHMDEBUGV("Raycast: Hit object %u at %f/%f/%f.", hit_info.unique_id, hit_info.position.x, hit_info.position.y, hit_info.position.z);

		Line3D* new_line = &app_state->test_raycast_lines[app_state->test_raycast_lines.emplace()];
		if (!line3D_init(origin, hit_info.position, { 1.0f, 1.0f, 0.0f, 1.0f }, new_line))
			SHMERROR("Failed to init or load new test line!");
	}
	
	
	return false;
}

static bool8 application_on_event(uint16 code, void* sender, void* listener_inst, EventData data)
{
	switch (code)
	{
	case SystemEventCode::OBJECT_HOVER_ID_CHANGED:
	{
		app_state->hovered_object_id = data.ui32[0];
		return true;
	}
	}

	return true;
}

static bool8 application_on_debug_event(uint16 code, void* sender, void* listener_inst, EventData data)
{

	if (code == SystemEventCode::DEBUG0 && app_state->main_scene.state == ResourceState::Initialized)
	{
		const char* names[3] = 
		{
		"cobblestone",
		"paving",
		"paving2"
		};

		static int32 choice = 2;
		const char* old_name = names[choice];
		choice++;
		choice %= 3;
		
		Mesh* m = scene_get_mesh(&app_state->main_scene, "cube_1");
		if (!m || !m->geometries.capacity)
			return false;

		MeshGeometry* g = &m->geometries[0];
		Material* old_material = MaterialSystem::get_material(g->material_id);
		if (old_material)
			MaterialSystem::release_material_id(old_material->name, &old_material);
		if (old_material)
			Renderer::material_destroy(old_material);

		Material* new_material;
		g->material_id = MaterialSystem::acquire_material_id(names[choice], &new_material);
		if (new_material)
			Renderer::material_init_from_resource_async(names[choice], new_material);

		if (!g->material_id.is_valid())
			SHMWARNV("Failed to acquire material '%s'! Using default.", names[choice]);
	}
	else if (code == SystemEventCode::DEBUG1)
	{
		if (app_state->main_scene.state < ResourceState::Initialized)
		{
			SHMDEBUG("Loading main scene...");
			if (!scene_init_from_resource("main_scene", &app_state->main_scene))
			{
				SHMERROR("Failed to initialize main scene");
				return false;
			}
		}
	}
	else if (code == SystemEventCode::DEBUG2)
	{
		if (app_state->main_scene.state == ResourceState::Initialized)
		{
			SHMDEBUG("Unloading main scene...");
			scene_destroy(&app_state->main_scene);
			for (uint32 i = 0; i < app_state->test_raycast_lines.count; i++)
				line3D_destroy(&app_state->test_raycast_lines[i]);
			app_state->test_raycast_lines.clear();
		}
	}

	return true;
}

static void register_events()
{
	Event::event_register(SystemEventCode::BUTTON_RELEASED, app_state, application_on_mousebutton_released);

	Event::event_register(SystemEventCode::OBJECT_HOVER_ID_CHANGED, app_state, application_on_event);

	Event::event_register(SystemEventCode::DEBUG0, app_state, application_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG1, app_state, application_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG2, app_state, application_on_debug_event);
}

static void unregister_events()
{
	Event::event_unregister(SystemEventCode::BUTTON_RELEASED, app_state, application_on_mousebutton_released);

	Event::event_unregister(SystemEventCode::OBJECT_HOVER_ID_CHANGED, app_state, application_on_event);

	Event::event_unregister(SystemEventCode::DEBUG0, app_state, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG1, app_state, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG2, app_state, application_on_debug_event);
}
