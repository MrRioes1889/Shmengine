#include "Sandbox.hpp"
#include "ApplicationState.hpp"
#include "Defines.hpp"
#include "Keybinds.hpp"
#include "DebugConsole.hpp"

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
#include <resources/Mesh.hpp>
// end

ApplicationState* app_state = 0;

static void register_events();
static void unregister_events();

static bool32 application_on_key_pressed(uint16 code, void* sender, void* listener_inst, EventData data)
{
	//SHMDEBUGV("Key pressed, Code: %u", data.ui32[0]);
	return false;
}

static bool32 application_on_debug_event(uint16 code, void* sender, void* listener_inst, EventData data)
{

	if (code == SystemEventCode::DEBUG0 && app_state->main_scene.state == SceneState::LOADED)
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
		if (app_state->main_scene.state == SceneState::INITIALIZED || app_state->main_scene.state == SceneState::UNLOADED)
		{
			SHMDEBUG("Loading main scene...");

			if (!scene_load(&app_state->main_scene))
				SHMERROR("Failed to load main_scene!");
		}
	}
	else if (code == SystemEventCode::DEBUG2)
	{
		if (app_state->main_scene.state == SceneState::LOADED)
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

	app_inst->config.render_view_configs.init(4, 0);
	Renderer::RenderViewConfig* skybox_view_config = &app_inst->config.render_view_configs[0];
	Renderer::RenderViewConfig* world_view_config = &app_inst->config.render_view_configs[1];
	Renderer::RenderViewConfig* ui_view_config = &app_inst->config.render_view_configs[2];
	Renderer::RenderViewConfig* pick_view_config = &app_inst->config.render_view_configs[3];

	const uint32 skybox_pass_count = 1;
	const uint32 world_pass_count = 1;
	const uint32 ui_pass_count = 1;
	const uint32 pick_pass_count = 2;

	skybox_view_config->pass_configs.init(skybox_pass_count, 0);
	world_view_config->pass_configs.init(world_pass_count, 0);
	ui_view_config->pass_configs.init(ui_pass_count, 0);
	pick_view_config->pass_configs.init(pick_pass_count, 0);

	skybox_view_config->type = Renderer::RenderViewType::SKYBOX;
	skybox_view_config->width = 0;
	skybox_view_config->height = 0;
	skybox_view_config->name = "skybox";
	skybox_view_config->view_matrix_source = Renderer::RenderViewViewMatrixSource::SCENE_CAMERA;

	Renderer::RenderPassConfig* skybox_pass = &skybox_view_config->pass_configs[0];
	skybox_pass->name = "Renderpass.Builtin.Skybox";
	skybox_pass->dim = { 1600, 900 };
	skybox_pass->offset = { 0, 0 };
	skybox_pass->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
	skybox_pass->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER;
	skybox_pass->depth = 1.0f;
	skybox_pass->stencil = 0;

	const uint32 skybox_target_att_count = 1;
	skybox_pass->target_config.attachment_configs.init(skybox_target_att_count, 0);
	skybox_pass->target_config.attachment_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	skybox_pass->target_config.attachment_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	skybox_pass->target_config.attachment_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	skybox_pass->target_config.attachment_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	skybox_pass->target_config.attachment_configs[0].present_after = false;

	skybox_pass->render_target_count = Renderer::get_window_attachment_count();

	world_view_config->type = Renderer::RenderViewType::WORLD;
	world_view_config->width = 0;
	world_view_config->height = 0;
	world_view_config->name = "world";
	world_view_config->view_matrix_source = Renderer::RenderViewViewMatrixSource::SCENE_CAMERA;

	Renderer::RenderPassConfig* world_pass = &world_view_config->pass_configs[0];
	world_pass->name = "Renderpass.Builtin.World";
	world_pass->dim = { 1600, 900 };
	world_pass->offset = { 0, 0 };
	world_pass->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
	world_pass->clear_flags = Renderer::RenderpassClearFlags::DEPTH_BUFFER | Renderer::RenderpassClearFlags::STENCIL_BUFFER;
	world_pass->depth = 1.0f;
	world_pass->stencil = 0;

	const uint32 world_target_att_count = 2;
	world_pass->target_config.attachment_configs.init(world_target_att_count, 0);
	world_pass->target_config.attachment_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	world_pass->target_config.attachment_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	world_pass->target_config.attachment_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	world_pass->target_config.attachment_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_pass->target_config.attachment_configs[0].present_after = false;

	world_pass->target_config.attachment_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
	world_pass->target_config.attachment_configs[1].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	world_pass->target_config.attachment_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	world_pass->target_config.attachment_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_pass->target_config.attachment_configs[1].present_after = false;

	world_pass->render_target_count = Renderer::get_window_attachment_count();


	ui_view_config->type = Renderer::RenderViewType::UI;
	ui_view_config->width = 0;
	ui_view_config->height = 0;
	ui_view_config->name = "ui";
	ui_view_config->view_matrix_source = Renderer::RenderViewViewMatrixSource::SCENE_CAMERA;

	Renderer::RenderPassConfig* ui_pass = &ui_view_config->pass_configs[0];
	ui_pass->name = "Renderpass.Builtin.UI";
	ui_pass->dim = { 1600, 900 };
	ui_pass->offset = { 0, 0 };
	ui_pass->clear_color = { 0.0f, 0.0f, 0.2f, 1.0f };
	ui_pass->clear_flags = Renderer::RenderpassClearFlags::NONE;
	ui_pass->depth = 1.0f;
	ui_pass->stencil = 0;

	const uint32 ui_target_att_count = 1;
	ui_pass->target_config.attachment_configs.init(ui_target_att_count, 0);
	ui_pass->target_config.attachment_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	ui_pass->target_config.attachment_configs[0].source = Renderer::RenderTargetAttachmentSource::DEFAULT;
	ui_pass->target_config.attachment_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	ui_pass->target_config.attachment_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	ui_pass->target_config.attachment_configs[0].present_after = true;

	ui_pass->render_target_count = Renderer::get_window_attachment_count();


	pick_view_config->type = Renderer::RenderViewType::PICK;
	pick_view_config->width = 0;
	pick_view_config->height = 0;
	pick_view_config->name = "pick";
	pick_view_config->view_matrix_source = Renderer::RenderViewViewMatrixSource::SCENE_CAMERA;

	Renderer::RenderPassConfig* world_pick_pass = &pick_view_config->pass_configs[0];
	Renderer::RenderPassConfig* ui_pick_pass = &pick_view_config->pass_configs[1];

	world_pick_pass->name = "Renderpass.Builtin.WorldPick";
	world_pick_pass->dim = { 1600, 900 };
	world_pick_pass->offset = { 0, 0 };
	world_pick_pass->clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	world_pick_pass->clear_flags = Renderer::RenderpassClearFlags::COLOR_BUFFER | Renderer::RenderpassClearFlags::DEPTH_BUFFER;
	world_pick_pass->depth = 1.0f;
	world_pick_pass->stencil = 0;

	const uint32 world_pick_target_att_count = 2;
	world_pick_pass->target_config.attachment_configs.init(world_pick_target_att_count, 0);
	world_pick_pass->target_config.attachment_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	world_pick_pass->target_config.attachment_configs[0].source = Renderer::RenderTargetAttachmentSource::VIEW;
	world_pick_pass->target_config.attachment_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	world_pick_pass->target_config.attachment_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_pick_pass->target_config.attachment_configs[0].present_after = false;

	world_pick_pass->target_config.attachment_configs[1].type = Renderer::RenderTargetAttachmentType::DEPTH;
	world_pick_pass->target_config.attachment_configs[1].source = Renderer::RenderTargetAttachmentSource::VIEW;
	world_pick_pass->target_config.attachment_configs[1].load_op = Renderer::RenderTargetAttachmentLoadOp::DONT_CARE;
	world_pick_pass->target_config.attachment_configs[1].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	world_pick_pass->target_config.attachment_configs[1].present_after = false;

	world_pick_pass->render_target_count = 1;

	ui_pick_pass->name = "Renderpass.Builtin.UIPick";
	ui_pick_pass->dim = { 1600, 900 };
	ui_pick_pass->offset = { 0, 0 };
	ui_pick_pass->clear_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	ui_pick_pass->clear_flags = Renderer::RenderpassClearFlags::NONE;
	ui_pick_pass->depth = 1.0f;
	ui_pick_pass->stencil = 0;

	const uint32 ui_pick_target_att_count = 1;
	ui_pick_pass->target_config.attachment_configs.init(ui_pick_target_att_count, 0);
	ui_pick_pass->target_config.attachment_configs[0].type = Renderer::RenderTargetAttachmentType::COLOR;
	ui_pick_pass->target_config.attachment_configs[0].source = Renderer::RenderTargetAttachmentSource::VIEW;
	ui_pick_pass->target_config.attachment_configs[0].load_op = Renderer::RenderTargetAttachmentLoadOp::LOAD;
	ui_pick_pass->target_config.attachment_configs[0].store_op = Renderer::RenderTargetAttachmentStoreOp::STORE;
	ui_pick_pass->target_config.attachment_configs[0].present_after = false;

	ui_pick_pass->render_target_count = 1;

	return true;

}

bool32 application_init(void* application_state)
{
	app_state = (ApplicationState*)application_state;

	register_events();
	add_keymaps();

	DebugConsole::init(&app_state->debug_console);
	DebugConsole::load(&app_state->debug_console);

	app_state->world_camera = CameraSystem::get_default_camera();
	app_state->world_camera->set_position({ 10.5f, 5.0f, 9.5f });
	app_state->allocation_count = 0;

	if (!ui_text_create(UITextType::BITMAP, "Roboto Mono 21px", 21, "Some test täext,\n\tyo!", &app_state->test_bitmap_text))
	{
		SHMERROR("Failed to load basic ui bitmap text.");
		return false;
	}
	ui_text_set_position(&app_state->test_bitmap_text, { 50, 300, 0 });

	if (!ui_text_create(UITextType::TRUETYPE, "Martian Mono", 21, "Some täest text,\n\tyo!", &app_state->test_truetype_text))
	{
		SHMERROR("Failed to load basic ui truetype text.");
		return false;
	}
	ui_text_set_position(&app_state->test_truetype_text, { 500, 550, 0 });

	Scene main_scene_test = {};
	if (!scene_init_from_resource("main_scene", &app_state->main_scene))
	{
		SHMERROR("Failed to initialize main scene");
		return false;
	}

	// Load up some test UI geometry.
	//MeshGeometryConfig ui_config = {};
	//ui_config.data_config.vertex_size = sizeof(Renderer::Vertex2D);
	//CString::copy("test_ui_material", ui_config.material_name, max_material_name_length);
	//CString::copy("test_ui_geometry", ui_config.data_config.name, max_geometry_name_length);

	//ui_config.data_config.vertex_count = 4;
	//ui_config.data_config.vertices.init(ui_config.data_config.vertex_size * ui_config.data_config.vertex_count, 0);
	//ui_config.data_config.indices.init(6, 0);
	//Renderer::Vertex2D* uiverts = (Renderer::Vertex2D*)&ui_config.data_config.vertices[0];

	//const float32 w = 100.0f;//1077.0f * 0.25f;
	//const float32 h = 100.0f;//1278.0f * 0.25f;
	//uiverts[0].position.x = 0.0f;  // 0    3
	//uiverts[0].position.y = 0.0f;  //
	//uiverts[0].tex_coordinates.x = 0.0f;  //
	//uiverts[0].tex_coordinates.y = 0.0f;  // 2    1

	//uiverts[1].position.y = h;
	//uiverts[1].position.x = w;
	//uiverts[1].tex_coordinates.x = 1.0f;
	//uiverts[1].tex_coordinates.y = 1.0f;

	//uiverts[2].position.x = 0.0f;
	//uiverts[2].position.y = h;
	//uiverts[2].tex_coordinates.x = 0.0f;
	//uiverts[2].tex_coordinates.y = 1.0f;

	//uiverts[3].position.x = w;
	//uiverts[3].position.y = 0.0;
	//uiverts[3].tex_coordinates.x = 1.0f;
	//uiverts[3].tex_coordinates.y = 0.0f;

	//// Indices - counter-clockwise
	//ui_config.data_config.indices[0] = 2;
	//ui_config.data_config.indices[1] = 1;
	//ui_config.data_config.indices[2] = 0;
	//ui_config.data_config.indices[3] = 3;
	//ui_config.data_config.indices[4] = 0;
	//ui_config.data_config.indices[5] = 1;

	// Get UI geometry from config.
	app_state->ui_meshes.init(1, 0);
	/*Mesh* ui_mesh = state->ui_meshes.emplace();
	ui_mesh->unique_id = identifier_acquire_new_id(ui_mesh);
	ui_mesh->geometries.init(1, 0);
	ui_mesh->geometries.push(0);
	ui_mesh->geometries[0] = GeometrySystem::acquire_from_config(ui_config, true);
	ui_mesh->transform = Math::transform_create();
	ui_mesh->generation = 0;*/

	return true;
}

void application_shutdown()
{
	// TODO: temp
	scene_destroy(&app_state->main_scene);
	ui_text_destroy(&app_state->test_bitmap_text);
	ui_text_destroy(&app_state->test_truetype_text);

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

	if (app_state->main_scene.state == SceneState::LOADED)
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
			clamp(Math::sin((float32)frame_data->total_time) * 0.75f + 0.5f, 0.0f, 1.0f),
			clamp(Math::sin((float32)frame_data->total_time) - (PI * 2 / 3) * 0.75f + 0.5f, 0.0f, 1.0f),
			clamp(Math::sin((float32)frame_data->total_time) * (PI * 4 / 3) * 0.75f + 0.5f, 0.0f, 1.0f),
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

	ui_text_set_text(&app_state->test_truetype_text, ui_text_buffer);

	DebugConsole::update(&app_state->debug_console);

	return true;
}

bool32 application_render(Renderer::RenderPacket* packet, FrameData* frame_data)
{

	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	frame_data->drawn_geometry_count = 0;

	const uint32 view_count = 3;
	Renderer::RenderViewPacket* render_view_packets = (Renderer::RenderViewPacket*)frame_data->frame_allocator->allocate(view_count * sizeof(Renderer::RenderViewPacket));
	packet->views.init(view_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, render_view_packets);

	packet->views[0].view = RenderViewSystem::get("skybox");
	packet->views[1].view = RenderViewSystem::get("world");
	packet->views[2].view = RenderViewSystem::get("ui");

	if (app_state->main_scene.state == SceneState::LOADED)
	{
		if (!scene_build_render_packet(&app_state->main_scene, &app_state->camera_frustum, frame_data, packet))
		{
			SHMERROR("Failed to build main scene render packet.");
			return false;
		}
	}

	uint32 ui_shader_id = ShaderSystem::get_ui_shader_id();

	Renderer::UIPacketData* ui_packet_data = (Renderer::UIPacketData*)frame_data->frame_allocator->allocate(sizeof(Renderer::UIPacketData));
	ui_packet_data->geometries = 0;
	ui_packet_data->geometries_count = 0;

	for (uint32 mesh_i = 0; mesh_i < app_state->ui_meshes.count; mesh_i++)
	{
		if (app_state->ui_meshes[mesh_i].generation == INVALID_ID8)
			continue;

		Mesh* mesh = &app_state->ui_meshes[mesh_i];

		for (uint32 geo_i = 0; geo_i < app_state->ui_meshes[mesh_i].geometries.count; geo_i++)
		{
			MeshGeometry* geometry = &mesh->geometries[geo_i];
			Renderer::GeometryRenderData* render_data = (Renderer::GeometryRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::GeometryRenderData));
			render_data->model = Math::transform_get_world(mesh->transform);
			render_data->shader_id = ui_shader_id;
			render_data->render_object = geometry->material;
			render_data->on_render = MaterialSystem::material_on_render;
			render_data->geometry_data = geometry->g_data;
			render_data->has_transparency = (geometry->material->maps[0].texture->flags & TextureFlags::HAS_TRANSPARENCY);
			render_data->unique_id = mesh->unique_id;

			ui_packet_data->geometries_count++;

			if (!ui_packet_data->geometries)
				ui_packet_data->geometries = render_data;
		}
	}

	GeometryData* geometry = &app_state->test_truetype_text.geometry;
	Renderer::GeometryRenderData* render_data = (Renderer::GeometryRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::GeometryRenderData));
	render_data->model = Math::transform_get_world(app_state->test_truetype_text.transform);
	render_data->shader_id = ui_shader_id;
	render_data->render_object = &app_state->test_truetype_text;
	render_data->on_render = ui_text_on_render;
	render_data->geometry_data = &app_state->test_truetype_text.geometry;
	render_data->has_transparency = true;
	render_data->unique_id = app_state->test_truetype_text.unique_id;

	ui_packet_data->geometries_count++;

	if (!ui_packet_data->geometries)
		ui_packet_data->geometries = render_data;

	if (DebugConsole::is_visible(&app_state->debug_console))
	{
		UIText* console_text = DebugConsole::get_text(&app_state->debug_console);
		geometry = &app_state->test_truetype_text.geometry;
		render_data = (Renderer::GeometryRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::GeometryRenderData));
		render_data->model = Math::transform_get_world(console_text->transform);
		render_data->shader_id = ui_shader_id;
		render_data->render_object = console_text;
		render_data->on_render = ui_text_on_render;
		render_data->geometry_data = &console_text->geometry;
		render_data->has_transparency = true;
		render_data->unique_id = console_text->unique_id;

		ui_packet_data->geometries_count++;

		if (!ui_packet_data->geometries)
			ui_packet_data->geometries = render_data;

		UIText* entry_text = DebugConsole::get_entry_text(&app_state->debug_console);
		geometry = &app_state->test_truetype_text.geometry;
		render_data = (Renderer::GeometryRenderData*)frame_data->frame_allocator->allocate(sizeof(Renderer::GeometryRenderData));
		render_data->model = Math::transform_get_world(entry_text->transform);
		render_data->shader_id = ui_shader_id;
		render_data->render_object = entry_text;
		render_data->on_render = ui_text_on_render;
		render_data->geometry_data = &entry_text->geometry;
		render_data->has_transparency = true;
		render_data->unique_id = entry_text->unique_id;

		ui_packet_data->geometries_count++;
	}

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("ui"), frame_data->frame_allocator, ui_packet_data, &packet->views[2]))
	{
		SHMERROR("Failed to build packet for view 'ui'.");
		return false;
	}

	/*Renderer::PickPacketData* pick_packet = (Renderer::PickPacketData*)Memory::linear_allocator_allocate(&app_inst->frame_allocator, sizeof(Renderer::PickPacketData));
	pick_packet->ui_mesh_data = ui_mesh_data->mesh_data;
	pick_packet->world_geometries = app_inst->frame_data.world_geometries.data;
	pick_packet->world_geometries_count = app_inst->frame_data.world_geometries.count;
	pick_packet->texts = ui_mesh_data->texts;
	pick_packet->text_count = ui_mesh_data->text_count;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("pick"), &app_inst->frame_allocator, pick_packet, &packet->views[3]))
	{
		SHMERROR("Failed to build packet for view 'pick'.");
		return false;
	}*/

	return true;
}

void application_on_resize(uint32 width, uint32 height)
{

	if (!app_state)
		return;

	app_state->width = width;
	app_state->height = height;

	ui_text_set_position(&app_state->test_truetype_text, { 20.0f, (float32)app_state->height - 150.0f, 0.0f });

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

static void register_events()
{
	Event::event_register(SystemEventCode::KEY_PRESSED, 0, application_on_key_pressed);

	Event::event_register(SystemEventCode::DEBUG0, app_state, application_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG1, app_state, application_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG2, app_state, application_on_debug_event);
}

static void unregister_events()
{
	Event::event_unregister(SystemEventCode::KEY_PRESSED, 0, application_on_key_pressed);

	Event::event_unregister(SystemEventCode::DEBUG0, app_state, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG1, app_state, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG2, app_state, application_on_debug_event);
}