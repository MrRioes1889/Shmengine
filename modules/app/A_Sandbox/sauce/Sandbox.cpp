#include "Sandbox.hpp"
#include "ApplicationState.hpp"
#include "Defines.hpp"
#include "Keybinds.hpp"
#include "DebugConsole.hpp"

#include "views/RenderViewSkybox.hpp"
#include "views/RenderViewWorld.hpp"
#include "views/RenderViewUI.hpp"
#include "views/RenderViewPick.hpp"

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
#include <renderer/RendererGeometry.hpp>
#include <utility/math/Transform.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/GeometrySystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <systems/FontSystem.hpp>
#include <systems/ShaderSystem.hpp>
// end

ApplicationState* app_state = 0;

static bool32 init_render_views(Application* app_inst);

static void register_events();
static void unregister_events();

static bool32 application_on_key_pressed(uint16 code, void* sender, void* listener_inst, EventData data)
{
	//SHMDEBUGV("Key pressed, Code: %u", data.ui32[0]);
	return false;
}

static bool32 application_on_event(uint16 code, void* sender, void* listener_inst, EventData data)
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



static bool32 application_on_debug_event(uint16 code, void* sender, void* listener_inst, EventData data)
{

	if (code == SystemEventCode::DEBUG0 && app_state->main_scene.state == ResourceState::LOADED)
	{
		const char* names[3] = {
		"cobblestone",
		"paving",
		"paving2"
		};

		static int32 choice = 2;
		const char* old_name = names[choice];
		choice++;
		choice %= 3;
		
		Mesh* m = scene_get_mesh(&app_state->main_scene, "test_cube1");
		if (!m || !m->geometries.count)
			return false;

		MeshGeometry* g = &m->geometries[0];
		g->material = MaterialSystem::acquire(names[choice]);
		if (!g->material)
		{
			SHMWARNV("event_on_debug_event - Failed to acquire material '%s'! Using default.", names[choice]);
			g->material = MaterialSystem::get_default_material();
		}

		MaterialSystem::release(old_name);
	}
	else if (code == SystemEventCode::DEBUG1)
	{
		if (app_state->main_scene.state == ResourceState::INITIALIZED || app_state->main_scene.state == ResourceState::UNLOADED)
		{
			SHMDEBUG("Loading main scene...");

			if (!scene_load(&app_state->main_scene))
				SHMERROR("Failed to load main_scene!");
		}
	}
	else if (code == SystemEventCode::DEBUG2)
	{
		if (app_state->main_scene.state == ResourceState::LOADED)
		{
			SHMDEBUG("Unloading main scene...");
			scene_unload(&app_state->main_scene);
		}
	}

	return true;
}

bool32 application_boot(Application* app_inst)
{

	app_inst->config.app_frame_data_size = sizeof(ApplicationFrameData);
	app_inst->config.state_size = sizeof(ApplicationState);
	app_inst->config.frame_allocator_size = Mebibytes(64);

	FontSystem::SystemConfig* font_sys_config = &app_inst->config.fontsystem_config;
	font_sys_config->auto_release = false;
	font_sys_config->max_bitmap_font_config_count = 15;
	font_sys_config->max_truetype_font_config_count = 15;

	font_sys_config->default_bitmap_font_count = 2;
	app_inst->config.bitmap_font_configs.init(font_sys_config->default_bitmap_font_count, 0);
	font_sys_config->bitmap_font_configs = app_inst->config.bitmap_font_configs.data;

	font_sys_config->bitmap_font_configs[0].name = "Noto Serif 21px";
	font_sys_config->bitmap_font_configs[0].resource_name = "NotoSerif_21";
	font_sys_config->bitmap_font_configs[0].size = 21;

	font_sys_config->bitmap_font_configs[1].name = "Roboto Mono 21px";
	font_sys_config->bitmap_font_configs[1].resource_name = "RobotoMono_21";
	font_sys_config->bitmap_font_configs[1].size = 21;

	font_sys_config->default_truetype_font_count = 1;
	app_inst->config.truetype_font_configs.init(font_sys_config->default_truetype_font_count, 0);
	font_sys_config->truetype_font_configs = app_inst->config.truetype_font_configs.data;

	font_sys_config->truetype_font_configs[0].name = "Martian Mono";
	font_sys_config->truetype_font_configs[0].resource_name = "MartianMono";
	font_sys_config->truetype_font_configs[0].default_size = 21;

	if (!init_render_views(app_inst))
	{
		SHMFATAL("Failed to initialize render views!");
		return false;
	}

	return true;

}

bool32 application_init(Application* app_inst)
{
	app_state = (ApplicationState*)app_inst->state;

	register_events();
	add_keymaps();

	DebugConsole::init(&app_state->debug_console);
	DebugConsole::load(&app_state->debug_console);

	app_state->world_camera = CameraSystem::get_default_camera();
	app_state->world_camera->set_position({ 10.5f, 5.0f, 9.5f });
	app_state->allocation_count = 0;

	UITextConfig ui_text_config = {};
	ui_text_config.type = UITextType::TRUETYPE;
	ui_text_config.font_name = "Martian Mono";
	ui_text_config.font_size = 21;
	ui_text_config.text_content = "Some täest text,\n\tyo!";

	if (!ui_text_init(&ui_text_config, &app_state->debug_info_text) || !ui_text_load(&app_state->debug_info_text))
	{
		SHMERROR("Failed to load basic ui truetype text.");
		return false;
	}
	ui_text_set_position(&app_state->debug_info_text, { 500, 550, 0 });

	Scene main_scene_test = {};
	if (!scene_init_from_resource("main_scene", &app_state->main_scene))
	{
		SHMERROR("Failed to initialize main scene");
		return false;
	}

	// Load up some test UI geometry.
	MeshGeometryConfig ui_config = {};
	GeometrySystem::GeometryConfig ui_g_config = {};
	ui_config.data_config = &ui_g_config;
		
	ui_config.material_name = "test_ui_material";
	CString::copy("test_ui_mesh", ui_config.data_config->name, max_mesh_name_length);

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
	ui_mesh->unique_id = identifier_acquire_new_id(ui_mesh);
	ui_mesh->geometries.init(1, 0);
	ui_mesh->geometries.emplace();
	ui_mesh->geometries[0].g_data = GeometrySystem::acquire_from_config(ui_config.data_config, true);
	CString::copy(ui_config.material_name, ui_mesh->geometries[0].material_name, max_material_name_length);
	ui_mesh->geometries[0].material = MaterialSystem::acquire(ui_mesh->geometries[0].material_name);
	ui_mesh->transform = Math::transform_create();
	ui_mesh->generation = 0;

	return true;
}

void application_shutdown()
{
	// TODO: temp
	scene_destroy(&app_state->main_scene);
	ui_text_destroy(&app_state->debug_info_text);

	DebugConsole::destroy(&app_state->debug_console);

	app_state->ui_meshes.free_data();

	unregister_events();
	// end

}

bool32 application_update(FrameData* frame_data)
{
	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	scene_update(&app_state->main_scene);
	frame_data->frame_allocator->free_all_data();

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

	if (app_state->main_scene.state == ResourceState::LOADED)
	{
		Mesh* cube1 = scene_get_mesh(&app_state->main_scene, "cube_1");
		Mesh* cube2 = scene_get_mesh(&app_state->main_scene, "cube_2");
		Mesh* cube3 = scene_get_mesh(&app_state->main_scene, "cube_3");
		PointLight* p_light = scene_get_point_light(&app_state->main_scene, 0);
		
		Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_UP, 1.5f * (float32)frame_data->delta_time, true);
		/*Math::transform_rotate(cube1->transform, rotation);
		Math::transform_rotate(cube2->transform, rotation);
		Math::transform_rotate(cube3->transform, rotation);*/

		p_light->color =
		{
			clamp(Math::sin((float32)frame_data->total_time * 0.75f), 0.0f, 1.0f),
			clamp(Math::sin((float32)frame_data->total_time * 0.25f), 0.0f, 1.0f),
			clamp(Math::sin((float32)frame_data->total_time * 0.5f), 0.0f, 1.0f),
			1.0f
		};
		static float32 starting_position = p_light->position.z;
		p_light->position.z = starting_position + Math::sin((float32)frame_data->total_time);
	}

	Math::Vec2i mouse_pos = Input::get_mouse_position();

	Camera* world_camera = CameraSystem::get_default_camera();
	Math::Vec3f pos = world_camera->get_position();
	Math::Vec3f rot = world_camera->get_rotation();

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

	Math::Vec3f fwd = app_state->world_camera->get_forward();
	Math::Vec3f right = app_state->world_camera->get_right();
	Math::Vec3f up = app_state->world_camera->get_up();
	app_state->camera_frustum = Math::frustum_create(app_state->world_camera->get_position(), fwd, right, up, (float32)app_state->width / (float32)app_state->height, Math::deg_to_rad(45.0f), 0.1f, 1000.0f);

	char ui_text_buffer[512];
	CString::safe_print_s<uint32, uint32, int32, int32, float32, float32, float32, float32, float32, float32, float64, float64, float64>
		(ui_text_buffer, 512, "Object Hovered ID: %u\nWorld geometry count: %u\nMouse Pos : [%i, %i]\tCamera Pos : [%f3, %f3, %f3]\nCamera Rot : [%f3, %f3, %f3]\n\nLast frametime: %lf4 ms\nLogic: %lf4 / Render: %lf4",
			app_state->hovered_object_id, frame_data->drawn_geometry_count, mouse_pos.x, mouse_pos.y, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, last_frametime * 1000.0, last_logictime * 1000.0, last_rendertime * 1000.0);

	ui_text_set_text(&app_state->debug_info_text, ui_text_buffer);
	ui_text_update(&app_state->debug_info_text);

	DebugConsole::update(&app_state->debug_console);

	return true;
}

bool32 application_render(Renderer::RenderPacket* packet, FrameData* frame_data)
{

	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	frame_data->drawn_geometry_count = 0;

	const uint32 view_count = 4;
	RenderView** render_views = (RenderView**)frame_data->frame_allocator->allocate(view_count * sizeof(RenderView*));
	packet->views.init(view_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, render_views);

	uint32 skybox_view_i = packet->views.emplace(RenderViewSystem::get("skybox"));
	uint32 world_view_i = packet->views.emplace(RenderViewSystem::get("world"));
	uint32 ui_view_i = packet->views.emplace(RenderViewSystem::get("ui"));
	uint32 pick_view_i = packet->views.emplace(RenderViewSystem::get("pick"));

	if (app_state->main_scene.state == ResourceState::LOADED)
		scene_draw(&app_state->main_scene, packet->views[skybox_view_i], packet->views[world_view_i], &app_state->camera_frustum, frame_data);

	uint32 ui_shader_id = ShaderSystem::get_ui_shader_id();

	Renderer::meshes_draw(app_state->ui_meshes.data, app_state->ui_meshes.count, packet->views[ui_view_i], 0, ui_shader_id, {}, frame_data, 0);

	Renderer::ui_text_draw(&app_state->debug_info_text, packet->views[ui_view_i], 0, ui_shader_id, frame_data);

	if (DebugConsole::is_visible(&app_state->debug_console))
	{
		UIText* console_text = DebugConsole::get_text(&app_state->debug_console);
		Renderer::ui_text_draw(console_text, packet->views[ui_view_i], 0, ui_shader_id, frame_data);

		UIText* entry_text = DebugConsole::get_entry_text(&app_state->debug_console);
		Renderer::ui_text_draw(entry_text, packet->views[ui_view_i], 0, ui_shader_id, frame_data);
	}

	RenderViewPacketData* pick_packet = (RenderViewPacketData*)frame_data->frame_allocator->allocate(sizeof(RenderViewPacketData));
	pick_packet->geometries = packet->views[1]->geometries.data;
	pick_packet->geometries_count = packet->views[1]->geometries.count;
	pick_packet->renderpass_id = 0;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("pick"), frame_data->frame_allocator, pick_packet))
	{
		SHMERROR("Failed to build packet for view 'pick'.");
		return false;
	}

	pick_packet->geometries = packet->views[2]->geometries.data;
	pick_packet->geometries_count = packet->views[2]->geometries.count;
	pick_packet->renderpass_id = 1;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("pick"), frame_data->frame_allocator, pick_packet))
	{
		SHMERROR("Failed to build packet for view 'pick'.");
		return false;
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
	DebugConsole::on_module_reload(&app_state->debug_console);
	add_keymaps();
}

void application_on_module_unload()
{
	unregister_events();
	DebugConsole::on_module_unload(&app_state->debug_console);
	remove_keymaps();
}

static bool32 init_render_views(Application* app_inst)
{
	app_inst->render_views.init(SandboxRenderViews::VIEW_COUNT, 0);
	RenderView* skybox_view = &app_inst->render_views[SandboxRenderViews::SKYBOX];
	RenderView* world_view = &app_inst->render_views[SandboxRenderViews::WORLD];
	RenderView* ui_view = &app_inst->render_views[SandboxRenderViews::UI];
	RenderView* pick_view = &app_inst->render_views[SandboxRenderViews::PICK];

	skybox_view->width = 0;
	skybox_view->height = 0;
	skybox_view->name = "skybox";

	skybox_view->on_build_packet = render_view_skybox_on_build_packet;
	skybox_view->on_end_frame = render_view_skybox_on_end_frame;
	skybox_view->on_render = render_view_skybox_on_render;
	skybox_view->on_register = render_view_skybox_on_register;
	skybox_view->on_unregister = render_view_skybox_on_unregister;
	skybox_view->on_resize = render_view_skybox_on_resize;
	skybox_view->regenerate_attachment_target = 0;

	const uint32 skybox_pass_count = 1;
	Renderer::RenderPassConfig skybox_pass_configs[skybox_pass_count];

	Renderer::RenderPassConfig* skybox_pass_config = &skybox_pass_configs[0];
	skybox_pass_config->name = "Renderpass.Builtin.Skybox";
	skybox_pass_config->dim = { app_inst->config.start_width, app_inst->config.start_height };
	skybox_pass_config->offset = { 0, 0 };
	skybox_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
	skybox_pass_config->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER;
	skybox_pass_config->depth = 1.0f;
	skybox_pass_config->stencil = 0;

	const uint32 skybox_target_att_count = 1;
	Renderer::RenderTargetAttachmentConfig skybox_att_configs[skybox_target_att_count];
	skybox_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	skybox_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	skybox_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	skybox_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	skybox_att_configs[0].present_after = false;

	skybox_pass_config->target_config.attachment_count = skybox_target_att_count;
	skybox_pass_config->target_config.attachment_configs = skybox_att_configs;
	skybox_pass_config->render_target_count = Renderer::get_window_attachment_count();

	RenderViewSystem::register_view(skybox_view, skybox_pass_count, skybox_pass_configs);


	world_view->width = 0;
	world_view->height = 0;
	world_view->name = "world";

	world_view->on_build_packet = render_view_world_on_build_packet;
	world_view->on_end_frame = render_view_world_on_end_frame;
	world_view->on_render = render_view_world_on_render;
	world_view->on_register = render_view_world_on_register;
	world_view->on_unregister = render_view_world_on_unregister;
	world_view->on_resize = render_view_world_on_resize;
	world_view->regenerate_attachment_target = 0;

	const uint32 world_pass_count = 2;
	Renderer::RenderPassConfig world_pass_configs[world_pass_count];

	Renderer::RenderPassConfig* world_objects_pass_config = &world_pass_configs[0];
	Renderer::RenderPassConfig* world_grid_pass_config = &world_pass_configs[1];

	world_objects_pass_config->name = "Builtin.WorldObjects";
	world_objects_pass_config->dim = { app_inst->config.start_width, app_inst->config.start_height };
	world_objects_pass_config->offset = { 0, 0 };
	world_objects_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
	world_objects_pass_config->clear_flags = Renderer::RenderpassClearFlags::DEPTH_BUFFER | Renderer::RenderpassClearFlags::STENCIL_BUFFER;
	world_objects_pass_config->depth = 1.0f;
	world_objects_pass_config->stencil = 0;

	const uint32 world_objects_target_att_count = 2;
	Renderer::RenderTargetAttachmentConfig world_objects_att_configs[world_objects_target_att_count];
	world_objects_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	world_objects_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	world_objects_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	world_objects_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_objects_att_configs[0].present_after = false;

	world_objects_att_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
	world_objects_att_configs[1].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	world_objects_att_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	world_objects_att_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_objects_att_configs[1].present_after = false;

	world_objects_pass_config->target_config.attachment_count = world_objects_target_att_count;
	world_objects_pass_config->target_config.attachment_configs = world_objects_att_configs;
	world_objects_pass_config->render_target_count = Renderer::get_window_attachment_count();

	world_grid_pass_config->name = "Builtin.WorldCoordinateGrid";
	world_grid_pass_config->dim = { app_inst->config.start_width, app_inst->config.start_height };
	world_grid_pass_config->offset = { 0, 0 };
	world_grid_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
	world_grid_pass_config->clear_flags = Renderer::RenderpassClearFlags::NONE;
	world_grid_pass_config->depth = 1.0f;
	world_grid_pass_config->stencil = 0;

	const uint32 world_grid_target_att_count = 2;
	Renderer::RenderTargetAttachmentConfig world_grid_att_configs[world_grid_target_att_count];
	world_grid_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	world_grid_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	world_grid_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	world_grid_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_grid_att_configs[0].present_after = false;

	world_grid_att_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
	world_grid_att_configs[1].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	world_grid_att_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	world_grid_att_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_grid_att_configs[1].present_after = false;

	world_grid_pass_config->target_config.attachment_count = world_grid_target_att_count;
	world_grid_pass_config->target_config.attachment_configs = world_grid_att_configs;
	world_grid_pass_config->render_target_count = Renderer::get_window_attachment_count();

	RenderViewSystem::register_view(world_view, world_pass_count, world_pass_configs);


	ui_view->width = 0;
	ui_view->height = 0;
	ui_view->name = "ui";

	ui_view->on_build_packet = render_view_ui_on_build_packet;
	ui_view->on_end_frame = render_view_ui_on_end_frame;
	ui_view->on_render = render_view_ui_on_render;
	ui_view->on_register = render_view_ui_on_register;
	ui_view->on_unregister = render_view_ui_on_unregister;
	ui_view->on_resize = render_view_ui_on_resize;
	ui_view->regenerate_attachment_target = 0;

	const uint32 ui_pass_count = 1;
	Renderer::RenderPassConfig ui_pass_configs[ui_pass_count];

	Renderer::RenderPassConfig* ui_pass_config = &ui_pass_configs[0];
	ui_pass_config->name = "Renderpass.Builtin.UI";
	ui_pass_config->dim = { app_inst->config.start_width, app_inst->config.start_height };
	ui_pass_config->offset = { 0, 0 };
	ui_pass_config->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
	ui_pass_config->clear_flags = Renderer::RenderpassClearFlags::NONE;
	ui_pass_config->depth = 1.0f;
	ui_pass_config->stencil = 0;

	const uint32 ui_target_att_count = 1;
	Renderer::RenderTargetAttachmentConfig ui_att_configs[ui_target_att_count];
	ui_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	ui_att_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	ui_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	ui_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	ui_att_configs[0].present_after = true;

	ui_pass_config->target_config.attachment_count = ui_target_att_count;
	ui_pass_config->target_config.attachment_configs = ui_att_configs;
	ui_pass_config->render_target_count = Renderer::get_window_attachment_count();

	RenderViewSystem::register_view(ui_view, ui_pass_count, ui_pass_configs);


	pick_view->width = 0;
	pick_view->height = 0;
	pick_view->name = "pick";

	pick_view->on_build_packet = render_view_pick_on_build_packet;
	pick_view->on_end_frame = render_view_pick_on_end_frame;
	pick_view->on_render = render_view_pick_on_render;
	pick_view->on_register = render_view_pick_on_register;
	pick_view->on_unregister = render_view_pick_on_unregister;
	pick_view->on_resize = render_view_pick_on_resize;
	pick_view->regenerate_attachment_target = render_view_pick_regenerate_attachment_target;

	const uint32 pick_pass_count = 2;
	Renderer::RenderPassConfig pick_pass_configs[pick_pass_count];

	Renderer::RenderPassConfig* world_pick_pass_config = &pick_pass_configs[0];
	Renderer::RenderPassConfig* ui_pick_pass_config = &pick_pass_configs[1];

	world_pick_pass_config->name = "Renderpass.Builtin.WorldPick";
	world_pick_pass_config->dim = { app_inst->config.start_width, app_inst->config.start_height };
	world_pick_pass_config->offset = { 0, 0 };
	world_pick_pass_config->clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	world_pick_pass_config->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER | Renderer::RenderpassClearFlags::DEPTH_BUFFER;
	world_pick_pass_config->depth = 1.0f;
	world_pick_pass_config->stencil = 0;

	const uint32 world_pick_target_att_count = 2;
	Renderer::RenderTargetAttachmentConfig world_pick_att_configs[world_pick_target_att_count];
	world_pick_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	world_pick_att_configs[0].source = Renderer::RenderTargetAttachmentSource::VIEW;
	world_pick_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	world_pick_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_pick_att_configs[0].present_after = false;

	world_pick_att_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
	world_pick_att_configs[1].source = Renderer::RenderTargetAttachmentSource::VIEW;
	world_pick_att_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	world_pick_att_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_pick_att_configs[1].present_after = false;

	world_pick_pass_config->target_config.attachment_count = world_pick_target_att_count;
	world_pick_pass_config->target_config.attachment_configs = world_pick_att_configs;
	world_pick_pass_config->render_target_count = 1;

	ui_pick_pass_config->name = "Renderpass.Builtin.UIPick";
	ui_pick_pass_config->dim = { app_inst->config.start_width, app_inst->config.start_height };
	ui_pick_pass_config->offset = { 0, 0 };
	ui_pick_pass_config->clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	ui_pick_pass_config->clear_flags = Renderer::RenderpassClearFlags::NONE;
	ui_pick_pass_config->depth = 1.0f;
	ui_pick_pass_config->stencil = 0;

	const uint32 ui_pick_target_att_count = 1;
	Renderer::RenderTargetAttachmentConfig ui_pick_att_configs[ui_pick_target_att_count];
	ui_pick_att_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	ui_pick_att_configs[0].source = Renderer::RenderTargetAttachmentSource::VIEW;
	ui_pick_att_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	ui_pick_att_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	ui_pick_att_configs[0].present_after = false;

	ui_pick_pass_config->target_config.attachment_count = ui_pick_target_att_count;
	ui_pick_pass_config->target_config.attachment_configs = ui_pick_att_configs;
	ui_pick_pass_config->render_target_count = 1;

	RenderViewSystem::register_view(pick_view, pick_pass_count, pick_pass_configs);

	return true;
}

static void register_events()
{
	Event::event_register(SystemEventCode::KEY_PRESSED, 0, application_on_key_pressed);

	Event::event_register(SystemEventCode::OBJECT_HOVER_ID_CHANGED, app_state, application_on_event);

	Event::event_register(SystemEventCode::DEBUG0, app_state, application_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG1, app_state, application_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG2, app_state, application_on_debug_event);
}

static void unregister_events()
{
	Event::event_unregister(SystemEventCode::KEY_PRESSED, 0, application_on_key_pressed);

	Event::event_unregister(SystemEventCode::OBJECT_HOVER_ID_CHANGED, app_state, application_on_event);

	Event::event_unregister(SystemEventCode::DEBUG0, app_state, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG1, app_state, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG2, app_state, application_on_debug_event);
}