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

	if (code == SystemEventCode::DEBUG0)
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

		Geometry* g = app_state->world_meshes[0].geometries[0];
		if (g)
		{
			g->material = MaterialSystem::acquire(names[choice]);
			if (!g->material)
			{
				SHMWARNV("event_on_debug_event - Failed to acquire material '%s'! Using default.", names[choice]);
				g->material = MaterialSystem::get_default_material();
			}

			MaterialSystem::release(old_name);
		}
	}
	else if (code == SystemEventCode::DEBUG1)
	{
		if (!app_state->world_meshes_loaded)
		{
			SHMDEBUG("Loading models...");

			if (!mesh_load(app_state->car_mesh))
				SHMERROR("Failed to load car mesh!");

			if (!mesh_load(app_state->sponza_mesh))
				SHMERROR("Failed to load sponza mesh!");

			app_state->world_meshes_loaded = true;
		}
	}
	else if (code == SystemEventCode::DEBUG2)
	{
		if (app_state->world_meshes_loaded)
		{
			SHMDEBUG("Unloading models...");
			mesh_unload(app_state->sponza_mesh);
			mesh_unload(app_state->car_mesh);
			app_state->world_meshes_loaded = false;
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

	Renderer::RenderpassConfig* skybox_pass = &skybox_view_config->pass_configs[0];
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

	Renderer::RenderpassConfig* world_pass = &world_view_config->pass_configs[0];
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

	Renderer::RenderpassConfig* ui_pass = &ui_view_config->pass_configs[0];
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

	Renderer::RenderpassConfig* world_pick_pass = &pick_view_config->pass_configs[0];
	Renderer::RenderpassConfig* ui_pick_pass = &pick_view_config->pass_configs[1];

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

	app_state->world_meshes_loaded = false;

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

	// Skybox
	SkyboxConfig skybox_config;
	skybox_config.cubemap_name = "skybox";
	if (!skybox_create(&skybox_config, &app_state->skybox) ||
		!skybox_init(&app_state->skybox) ||
		!skybox_load(&app_state->skybox))
	{
		SHMERROR("Failed to load skybox.");
		return false;
	}


	// Meshes
	app_state->world_meshes.init(5, DarrayFlags::NON_RESIZABLE);

	MeshConfig cube_config = {};
	cube_config.g_configs.init(1, 0);
	cube_config.g_configs.emplace();

	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, "test_cube", "test_material", cube_config.g_configs[0]);
	Mesh* cube_mesh = app_state->world_meshes.emplace();
	if (!mesh_create(&cube_config, cube_mesh) || !mesh_init(cube_mesh) || !mesh_load(cube_mesh))
	{
		SHMERROR("Failed to load cube mesh");
		return false;
	}
	cube_mesh->transform = Math::transform_create();

	cube_config.g_configs.init(1, 0);
	cube_config.g_configs.emplace();
	Renderer::generate_cube_config(5.0f, 5.0f, 5.0f, 1.0f, 1.0f, "test_cube_2", "test_material", cube_config.g_configs[0]);
	Mesh* cube_mesh2 = app_state->world_meshes.emplace();
	if (!mesh_create(&cube_config, cube_mesh2) || !mesh_init(cube_mesh2) || !mesh_load(cube_mesh2))
	{
		SHMERROR("Failed to load cube mesh");
		return false;
	}
	cube_mesh2->transform = Math::transform_from_position({ 10.0f, 0.0f, 1.0f });

	cube_config.g_configs.init(1, 0);
	cube_config.g_configs.emplace();
	Renderer::generate_cube_config(2.0f, 2.0f, 2.0f, 1.0f, 1.0f, "test_cube_3", "test_material", cube_config.g_configs[0]);
	Mesh* cube_mesh3 = app_state->world_meshes.emplace();
	if (!mesh_create(&cube_config, cube_mesh3) || !mesh_init(cube_mesh3) || !mesh_load(cube_mesh3))
	{
		SHMERROR("Failed to load cube mesh");
		return false;
	}
	cube_mesh3->transform = Math::transform_from_position({ 15.0f, 0.0f, 1.0f });

	app_state->world_meshes[1].transform.parent = &app_state->world_meshes[0].transform;
	app_state->world_meshes[2].transform.parent = &app_state->world_meshes[0].transform;

	MeshConfig falcon_config = {};
	falcon_config.resource_name = "falcon";	
	app_state->car_mesh = app_state->world_meshes.emplace();
	if (!mesh_create(&falcon_config, app_state->car_mesh) || !mesh_init(app_state->car_mesh))
	{
		SHMERROR("Failed to create and initialize car mesh.");
		return false;
	}
	app_state->car_mesh->transform = Math::transform_from_position({ 15.0f, 0.0f, 1.0f });

	MeshConfig sponza_config = {};
	sponza_config.resource_name = "sponza";
	app_state->sponza_mesh = app_state->world_meshes.emplace();
	if (!mesh_create(&sponza_config, app_state->sponza_mesh) || !mesh_init(app_state->sponza_mesh))
	{
		SHMERROR("Failed to create and initialize sponza mesh.");
		return false;
	}
	app_state->sponza_mesh->transform = Math::transform_from_position_rotation_scale({ 15.0f, 0.0f, 1.0f }, QUAT_IDENTITY, { 0.1f, 0.1f, 0.1f });

	// Load up some test UI geometry.
	GeometrySystem::GeometryConfig ui_config = {};
	ui_config.vertex_size = sizeof(Renderer::Vertex2D);
	CString::copy(Material::max_name_length, ui_config.material_name, "test_ui_material");
	CString::copy(Geometry::max_name_length, ui_config.name, "test_ui_geometry");

	ui_config.vertex_count = 4;
	ui_config.vertices.init(ui_config.vertex_size * ui_config.vertex_count, 0);
	ui_config.indices.init(6, 0);
	Renderer::Vertex2D* uiverts = (Renderer::Vertex2D*)&ui_config.vertices[0];

	const float32 w = 100.0f;//1077.0f * 0.25f;
	const float32 h = 100.0f;//1278.0f * 0.25f;
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
	ui_config.indices[0] = 2;
	ui_config.indices[1] = 1;
	ui_config.indices[2] = 0;
	ui_config.indices[3] = 3;
	ui_config.indices[4] = 0;
	ui_config.indices[5] = 1;

	// Get UI geometry from config.
	app_state->ui_meshes.init(1, 0);
	/*Mesh* ui_mesh = state->ui_meshes.emplace();
	ui_mesh->unique_id = identifier_acquire_new_id(ui_mesh);
	ui_mesh->geometries.init(1, 0);
	ui_mesh->geometries.push(0);
	ui_mesh->geometries[0] = GeometrySystem::acquire_from_config(ui_config, true);
	ui_mesh->transform = Math::transform_create();
	ui_mesh->generation = 0;*/

	app_state->dir_light =
	{
		.color = { 0.4f, 0.4f, 0.2f, 1.0f },
		.direction = { -0.57735f, -0.57735f, -0.57735f, 0.0f }
	};

	app_state->p_lights[0] =
	{
		.color = { 1.0f, 0.0f, 0.0f, 1.0f },
		.position = { -5.5f, 0.0f, -5.5f, 0.0f },
		.constant_f = 1.0f,
		.linear = 0.35f,
		.quadratic = 0.44f
	};

	app_state->p_lights[1] =
	{
		.color = { 0.0f, 1.0f, 0.0f, 1.0f },
		.position = { 5.5f, 0.0f, -5.5f, 0.0f },
		.constant_f = 1.0f,
		.linear = 0.35f,
		.quadratic = 0.44f
	};

	app_state->p_lights[2] =
	{
		.color = { 0.0f, 0.0f, 1.0f, 1.0f },
		.position = { 5.5f, 0.0f, 5.5f, 0.0f },
		.constant_f = 1.0f,
		.linear = 0.35f,
		.quadratic = 0.44f
	};

	return true;
}

void application_shutdown()
{
	// TODO: temp
	skybox_destroy(&app_state->skybox);
	ui_text_destroy(&app_state->test_bitmap_text);
	ui_text_destroy(&app_state->test_truetype_text);

	DebugConsole::destroy(&app_state->debug_console);

	app_state->world_meshes.free_data();
	app_state->ui_meshes.free_data();
	app_state->car_mesh = 0;
	app_state->sponza_mesh = 0;

	unregister_events();
	// end

}

bool32 application_update(FrameData* frame_data)
{
	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	if (!app_frame_data->world_geometries.data)
		// TODO: Figure out a way to make this resizable without messing up pointers in render packets on resize.
		app_frame_data->world_geometries.init(512, DarrayFlags::NON_RESIZABLE);

	app_frame_data->world_geometries.clear();
	frame_data->frame_allocator->free_all_data();

	uint32 allocation_count = Memory::get_current_allocation_count();
	app_state->allocation_count = allocation_count;

	if (Input::is_cursor_clipped())
	{
		Math::Vec2i mouse_offset = Input::get_internal_mouse_offset();
		float32 mouse_sensitivity = 3.0f;
		float32 yaw = -mouse_offset.x * (float32)frame_data->delta_time * mouse_sensitivity;
		float32 pitch = -mouse_offset.y * (float32)frame_data->delta_time * mouse_sensitivity;
		app_state->world_camera->yaw(yaw);
		app_state->world_camera->pitch(pitch);
	}

	Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_UP, 5.5f * (float32)frame_data->delta_time, true);
	Math::transform_rotate(app_state->world_meshes[0].transform, rotation);
	Math::transform_rotate(app_state->world_meshes[1].transform, rotation);
	Math::transform_rotate(app_state->world_meshes[2].transform, rotation);

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

	for (uint32 i = 0; i < app_state->world_meshes.count; i++)
	{
		Mesh* m = &app_state->world_meshes[i];
		if (m->generation == INVALID_ID8)
			continue;

		Math::Mat4 model = Math::transform_get_world(m->transform);
		for (uint32 j = 0; j < m->geometries.count; j++)
		{
			Geometry* g = m->geometries[j];
			Math::Vec3f extents_max = Math::vec_mul_mat(g->extents.max, model);

			Math::Vec3f center = Math::vec_mul_mat(g->center, model);
			Math::Vec3f half_extents = { Math::abs(extents_max.x - center.x), Math::abs(extents_max.y - center.y), Math::abs(extents_max.z - center.z) };

			if (Math::frustum_intersects_aabb(app_state->camera_frustum, center, half_extents))
			{
				Renderer::GeometryRenderData* render_data = app_frame_data->world_geometries.emplace();
				render_data->model = model;
				render_data->geometry = g;
				render_data->unique_id = m->unique_id;
			}
		}
	}

	char ui_text_buffer[512];
	CString::safe_print_s<uint32, uint32, int32, int32, float32, float32, float32, float32, float32, float32, float64, float64, float64>
		(ui_text_buffer, 512, "Object Hovered ID: %u\nWorld geometry count: %u\nMouse Pos : [%i, %i]\tCamera Pos : [%f3, %f3, %f3]\nCamera Rot : [%f3, %f3, %f3]\n\nLast frametime: %lf4 ms\nLogic: %lf4 / Render: %lf4",
			app_state->hovered_object_id, app_frame_data->world_geometries.count, mouse_pos.x, mouse_pos.y, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, last_frametime * 1000.0, last_logictime * 1000.0, last_rendertime * 1000.0);

	ui_text_set_text(&app_state->test_truetype_text, ui_text_buffer);

	DebugConsole::update(&app_state->debug_console);

	return true;
}

bool32 application_render(Renderer::RenderPacket* packet, FrameData* frame_data)
{

	ApplicationFrameData* app_frame_data = (ApplicationFrameData*)frame_data->app_data;

	const uint32 view_count = 3;
	Renderer::RenderViewPacket* render_view_packets = (Renderer::RenderViewPacket*)frame_data->frame_allocator->allocate(view_count * sizeof(Renderer::RenderViewPacket));
	packet->views.init(view_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, render_view_packets);

	Renderer::SkyboxPacketData* skybox_data = (Renderer::SkyboxPacketData*)frame_data->frame_allocator->allocate(sizeof(Renderer::SkyboxPacketData));
	skybox_data->skybox = &app_state->skybox;
	if (!RenderViewSystem::build_packet(RenderViewSystem::get("skybox"), frame_data->frame_allocator, skybox_data, &packet->views[0]))
	{
		SHMERROR("Failed to build packet for view 'skybox'.");
		return false;
	}

	Renderer::WorldPacketData* world_packet = (Renderer::WorldPacketData*)frame_data->frame_allocator->allocate(sizeof(Renderer::WorldPacketData));
	world_packet->geometries = app_frame_data->world_geometries.data;
	world_packet->geometries_count = app_frame_data->world_geometries.count;
	world_packet->dir_light = &app_state->dir_light;
	world_packet->p_lights_count = app_state->p_lights_count;
	world_packet->p_lights = app_state->p_lights;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("world"), frame_data->frame_allocator, world_packet, &packet->views[1]))
	{
		SHMERROR("Failed to build packet for view 'world'.");
		return false;
	}

	Mesh** ui_meshes_ptr = (Mesh**)frame_data->frame_allocator->allocate(app_state->ui_meshes.count * sizeof(Mesh*));
	Sarray<Mesh*> ui_meshes(app_state->ui_meshes.count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, ui_meshes_ptr);

	Renderer::UIPacketData* ui_mesh_data = (Renderer::UIPacketData*)frame_data->frame_allocator->allocate(sizeof(Renderer::UIPacketData));
	ui_mesh_data->mesh_data.mesh_count = 0;
	for (uint32 i = 0; i < app_state->ui_meshes.count; i++)
	{
		if (app_state->ui_meshes[i].generation != INVALID_ID8)
		{
			ui_meshes[ui_mesh_data->mesh_data.mesh_count] = &app_state->ui_meshes[i];
			ui_mesh_data->mesh_data.mesh_count++;
		}
	}
	ui_mesh_data->mesh_data.meshes = ui_meshes.data;

	const uint32 max_text_count = 3;
	UIText** ui_texts_ptr = (UIText**)frame_data->frame_allocator->allocate(max_text_count * sizeof(UIText*));
	Sarray<UIText*> ui_texts(max_text_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, ui_texts_ptr);
	ui_texts[0] = &app_state->test_truetype_text;
	ui_texts[1] = &app_state->test_bitmap_text;

	ui_mesh_data->text_count = 1;
	ui_mesh_data->texts = ui_texts.data;

	if (DebugConsole::is_visible(&app_state->debug_console))
	{
		ui_mesh_data->text_count += 2;
		ui_texts[1] = DebugConsole::get_text(&app_state->debug_console);
		ui_texts[2] = DebugConsole::get_entry_text(&app_state->debug_console);
	}

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("ui"), frame_data->frame_allocator, ui_mesh_data, &packet->views[2]))
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