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
#include <resources/Mesh.hpp>
// end

static void register_events(Application* app_inst);
static void unregister_events(Application* app_inst);

static bool32 application_on_key_pressed(uint16 code, void* sender, void* listener_inst, EventData data)
{
	//SHMDEBUGV("Key pressed, Code: %u", data.ui32[0]);
	return false;
}

static bool32 application_on_debug_event(uint16 code, void* sender, void* listener_inst, EventData data)
{

	Application* app_inst = (Application*)listener_inst;
	ApplicationState* state = (ApplicationState*)app_inst->state;

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

		Geometry* g = state->world_meshes[0].geometries[0];
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
		if (!state->world_meshes_loaded)
		{
			SHMDEBUG("Loading models...");
			if (!mesh_load_from_resource("falcon", state->car_mesh))
				SHMERROR("Failed to load car mesh!");
			if (!mesh_load_from_resource("sponza", state->sponza_mesh))
				SHMERROR("Failed to load sponza mesh!");

			state->world_meshes_loaded = true;
		}
	}

	return true;
}

bool32 application_boot(Application* app_inst)
{

	app_inst->frame_allocator.init(Mebibytes(64));

	app_inst->state_size = sizeof(ApplicationState);
	app_inst->state = Memory::allocate(app_inst->state_size, AllocationTag::APPLICATION);

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

bool32 application_init(Application* app_inst) 
{
    ApplicationState* state = (ApplicationState*)app_inst->state;

	register_events(app_inst);
	add_keymaps(app_inst);
	
	DebugConsole::init(&state->debug_console);
	DebugConsole::load(&state->debug_console);

    state->world_camera = CameraSystem::get_default_camera();
    state->world_camera->set_position({ 10.5f, 5.0f, 9.5f });
    state->allocation_count = 0;

	state->world_meshes_loaded = false;

	if (!ui_text_create(UITextType::BITMAP, "Roboto Mono 21px", 21, "Some test täext,\n\tyo!", &state->test_bitmap_text))
	{
		SHMERROR("Failed to load basic ui bitmap text.");
		return false;
	}
	ui_text_set_position(&state->test_bitmap_text, { 50, 300, 0 });

	if (!ui_text_create(UITextType::TRUETYPE, "Martian Mono", 21, "Some täest text,\n\tyo!", &state->test_truetype_text))
	{
		SHMERROR("Failed to load basic ui truetype text.");
		return false;
	}
	ui_text_set_position(&state->test_truetype_text, { 500, 550, 0 });

	// Skybox
	if (!skybox_create("skybox_cube", &state->skybox))
	{
		SHMERROR("Failed to load skybox.");
		return false;
	}

	// Meshes
	state->world_meshes.init(5, DarrayFlags::NON_RESIZABLE);

	Mesh* cube_mesh = state->world_meshes.emplace();
	cube_mesh->geometries.init(1, 0);
	GeometrySystem::GeometryConfig g_config = {};
	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, "test_cube", "test_material", g_config);
	cube_mesh->geometries.push(GeometrySystem::acquire_from_config(g_config, true));
	cube_mesh->transform = Math::transform_create();
	cube_mesh->unique_id = identifier_acquire_new_id(cube_mesh);
	cube_mesh->generation = 0;

	Mesh* cube_mesh2 = state->world_meshes.emplace();
	cube_mesh2->geometries.init(1, 0);
	GeometrySystem::GeometryConfig g_config2 = {};
	Renderer::generate_cube_config(5.0f, 5.0f, 5.0f, 1.0f, 1.0f, "test_cube_2", "test_material", g_config2);
	cube_mesh2->geometries.push(GeometrySystem::acquire_from_config(g_config2, true));
	cube_mesh2->transform = Math::transform_from_position({ 10.0f, 0.0f, 1.0f });
	cube_mesh2->unique_id = identifier_acquire_new_id(cube_mesh);
	cube_mesh2->generation = 0;

	Mesh* cube_mesh3 = state->world_meshes.emplace();
	cube_mesh3->geometries.init(1, 0);
	GeometrySystem::GeometryConfig g_config3 = {};
	Renderer::generate_cube_config(2.0f, 2.0f, 2.0f, 1.0f, 1.0f, "test_cube_3", "test_material", g_config3);
	cube_mesh3->geometries.push(GeometrySystem::acquire_from_config(g_config3, true));
	cube_mesh3->transform = Math::transform_from_position({ 15.0f, 0.0f, 1.0f });
	cube_mesh3->unique_id = identifier_acquire_new_id(cube_mesh);
	cube_mesh3->generation = 0;

	state->world_meshes[1].transform.parent = &state->world_meshes[0].transform;
	state->world_meshes[2].transform.parent = &state->world_meshes[0].transform;

	state->car_mesh = state->world_meshes.emplace();
	state->car_mesh->unique_id = identifier_acquire_new_id(state->car_mesh);
	state->car_mesh->transform = Math::transform_from_position({ 15.0f, 0.0f, 1.0f });
	state->car_mesh->generation = INVALID_ID8;

	state->sponza_mesh = state->world_meshes.emplace();
	state->sponza_mesh->unique_id = identifier_acquire_new_id(state->car_mesh);
	state->sponza_mesh->transform = Math::transform_from_position_rotation_scale({ 15.0f, 0.0f, 1.0f }, QUAT_IDENTITY, { 0.1f, 0.1f, 0.1f });
	state->sponza_mesh->generation = INVALID_ID8;

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
	state->ui_meshes.init(1, 0);
	/*Mesh* ui_mesh = state->ui_meshes.emplace();
	ui_mesh->unique_id = identifier_acquire_new_id(ui_mesh);
	ui_mesh->geometries.init(1, 0);
	ui_mesh->geometries.push(0);
	ui_mesh->geometries[0] = GeometrySystem::acquire_from_config(ui_config, true);
	ui_mesh->transform = Math::transform_create();
	ui_mesh->generation = 0;*/

	app_inst->frame_data.world_geometries.init(512, 0);

    return true;
}

void application_shutdown(Application* app_inst)
{
	ApplicationState* state = (ApplicationState*)app_inst->state;
	
	app_inst->config.bitmap_font_configs.free_data();
	app_inst->config.truetype_font_configs.free_data();

	for (uint32 i = 0; i < app_inst->config.render_view_configs.capacity; i++)
	{
		Renderer::RenderViewConfig* view_config = &app_inst->config.render_view_configs[i];
		for (uint32 j = 0; j < view_config->pass_configs.capacity; j++)
		{
			Renderer::RenderpassConfig* pass_config = &view_config->pass_configs[j];
			pass_config->target_config.attachment_configs.free_data();
		}
		view_config->pass_configs.free_data();
	}
	app_inst->config.render_view_configs.free_data();

	// TODO: temp
	skybox_destroy(&state->skybox);
	ui_text_destroy(&state->test_bitmap_text);
	ui_text_destroy(&state->test_truetype_text);

	DebugConsole::destroy(&state->debug_console);

	state->world_meshes.free_data();
	state->ui_meshes.free_data();
	state->car_mesh = 0;
	state->sponza_mesh = 0;

	Event::event_unregister(SystemEventCode::DEBUG0, app_inst, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG1, app_inst, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG2, app_inst, application_on_debug_event);
	Event::event_unregister(SystemEventCode::KEY_PRESSED, 0, application_on_key_pressed);
	// end

}

bool32 application_update(Application* app_inst, float64 delta_time) 
{

    ApplicationState* state = (ApplicationState*)app_inst->state;
	state->delta_time = delta_time;

	app_inst->frame_data.world_geometries.clear();

	app_inst->frame_allocator.free_all_data();

    uint32 allocation_count = Memory::get_current_allocation_count();
    state->allocation_count = allocation_count;

    if (Input::is_cursor_clipped())
    {
        Math::Vec2i mouse_offset = Input::get_internal_mouse_offset();
        float32 mouse_sensitivity = 3.0f;
		float32 yaw = -mouse_offset.x * (float32)delta_time * mouse_sensitivity;
		float32 pitch = -mouse_offset.y * (float32)delta_time * mouse_sensitivity;
        state->world_camera->yaw(yaw);
        state->world_camera->pitch(pitch);
    }

	Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_UP, -10.5f * (float32)delta_time, true);
	Math::transform_rotate(state->world_meshes[0].transform, rotation);
	Math::transform_rotate(state->world_meshes[1].transform, rotation);
	Math::transform_rotate(state->world_meshes[2].transform, rotation);

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

	Math::Vec3f fwd = state->world_camera->get_forward();
	Math::Vec3f right = state->world_camera->get_right();
	Math::Vec3f up = state->world_camera->get_up();
	state->camera_frustum = Math::frustum_create(state->world_camera->get_position(), fwd, right, up, (float32)state->width / (float32)state->height, Math::deg_to_rad(45.0f), 0.1f, 1000.0f);

	for (uint32 i = 0; i < state->world_meshes.count; i++)
	{
		Mesh* m = &state->world_meshes[i];
		if (m->generation == INVALID_ID8)
			continue;

		Math::Mat4 model = Math::transform_get_world(m->transform);
		for (uint32 j = 0; j < m->geometries.count; j++)
		{
			Geometry* g = m->geometries[j];

			{
				Math::Vec3f extents_max = Math::vec_mul_mat(g->extents.max, model);

				Math::Vec3f center = Math::vec_mul_mat(g->center, model);
				Math::Vec3f half_extents = { Math::abs(extents_max.x - center.x), Math::abs(extents_max.y - center.y), Math::abs(extents_max.z - center.z) };

				if (Math::frustum_intersects_aabb(state->camera_frustum, center, half_extents))
				{
					Renderer::GeometryRenderData* render_data = app_inst->frame_data.world_geometries.emplace();
					render_data->model = model;
					render_data->geometry = g;
					render_data->unique_id = m->unique_id;
				}
			}
		}
	}

	char ui_text_buffer[512];
	CString::safe_print_s<uint32, uint32, int32, int32, float32, float32, float32, float32, float32, float32, float64, float64, float64>
		(ui_text_buffer, 512, "Object Hovered ID: %u\nWorld geometry count: %u\nMouse Pos : [%i, %i]\tCamera Pos : [%f3, %f3, %f3]\nCamera Rot : [%f3, %f3, %f3]\n\nLast frametime: %lf4 ms\nLogic: %lf4 / Render: %lf4",
			state->hovered_object_id, app_inst->frame_data.world_geometries.count, mouse_pos.x, mouse_pos.y, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, last_frametime * 1000.0, last_logictime * 1000.0, last_rendertime * 1000.0);

	ui_text_set_text(&state->test_truetype_text, ui_text_buffer);

	DebugConsole::update(&state->debug_console);

    return true;
}

bool32 application_render(Application* app_inst, Renderer::RenderPacket* packet, float64 delta_time)
{

	ApplicationState* state = (ApplicationState*)app_inst->state;

	const uint32 view_count = 3;
	Renderer::RenderViewPacket* render_view_packets = (Renderer::RenderViewPacket*)app_inst->frame_allocator.allocate(view_count * sizeof(Renderer::RenderViewPacket));
	packet->views.init(view_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, render_view_packets);

	Renderer::SkyboxPacketData* skybox_data = (Renderer::SkyboxPacketData*)app_inst->frame_allocator.allocate(sizeof(Renderer::SkyboxPacketData));
	skybox_data->skybox = &state->skybox;
	if (!RenderViewSystem::build_packet(RenderViewSystem::get("skybox"), &app_inst->frame_allocator, skybox_data, &packet->views[0]))
	{
		SHMERROR("Failed to build packet for view 'skybox'.");
		return false;
	}

	Renderer::WorldPacketData* world_packet = (Renderer::WorldPacketData*)app_inst->frame_allocator.allocate(sizeof(Renderer::WorldPacketData));
	world_packet->geometries = app_inst->frame_data.world_geometries.data;
	world_packet->geometries_count = app_inst->frame_data.world_geometries.count;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("world"), &app_inst->frame_allocator, world_packet, &packet->views[1]))
	{
		SHMERROR("Failed to build packet for view 'world'.");
		return false;
	}

	Mesh** ui_meshes_ptr = (Mesh**)app_inst->frame_allocator.allocate(state->ui_meshes.count * sizeof(Mesh*));
	Sarray<Mesh*> ui_meshes(state->ui_meshes.count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, ui_meshes_ptr);

	Renderer::UIPacketData* ui_mesh_data = (Renderer::UIPacketData*)app_inst->frame_allocator.allocate(sizeof(Renderer::UIPacketData));
	ui_mesh_data->mesh_data.mesh_count = 0;
	for (uint32 i = 0; i < state->ui_meshes.count; i++)
	{
		if (state->ui_meshes[i].generation != INVALID_ID8)
		{
			ui_meshes[ui_mesh_data->mesh_data.mesh_count] = &state->ui_meshes[i];
			ui_mesh_data->mesh_data.mesh_count++;
		}
	}
	ui_mesh_data->mesh_data.meshes = ui_meshes.data;

	const uint32 max_text_count = 3;
	UIText** ui_texts_ptr = (UIText**)app_inst->frame_allocator.allocate(max_text_count * sizeof(UIText*));
	Sarray<UIText*> ui_texts(max_text_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, ui_texts_ptr);
	ui_texts[0] = &state->test_truetype_text;
	ui_texts[1] = &state->test_bitmap_text;

	ui_mesh_data->text_count = 1;		
	ui_mesh_data->texts = ui_texts.data;

	if (DebugConsole::is_visible(&state->debug_console))
	{
		ui_mesh_data->text_count += 2;
		ui_texts[1] = DebugConsole::get_text(&state->debug_console);
		ui_texts[2] = DebugConsole::get_entry_text(&state->debug_console);
	}

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("ui"), &app_inst->frame_allocator, ui_mesh_data, &packet->views[2]))
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

void application_on_resize(Application* app_inst, uint32 width, uint32 height) 
{

	ApplicationState* state = (ApplicationState*)app_inst->state;
	if (!state)
		return;

	state->width = width;
	state->height = height;

	ui_text_set_position(&state->test_truetype_text, { 20.0f, (float32)state->height - 150.0f, 0.0f });

}

void application_on_module_reload(Application* app_inst)
{
	ApplicationState* state = (ApplicationState*)app_inst->state;	

	register_events(app_inst);
	DebugConsole::on_module_reload(&state->debug_console);
	add_keymaps(app_inst);
}

void application_on_module_unload(Application* app_inst)
{
	ApplicationState* state = (ApplicationState*)app_inst->state;

	unregister_events(app_inst);
	DebugConsole::on_module_unload(&state->debug_console);
	remove_keymaps(app_inst);
}

static void register_events(Application* app_inst)
{
	Event::event_register(SystemEventCode::KEY_PRESSED, 0, application_on_key_pressed);

	Event::event_register(SystemEventCode::DEBUG0, app_inst, application_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG1, app_inst, application_on_debug_event);
}

static void unregister_events(Application* app_inst)
{
	Event::event_unregister(SystemEventCode::KEY_PRESSED, 0, application_on_key_pressed);

	Event::event_unregister(SystemEventCode::DEBUG0, app_inst, application_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG1, app_inst, application_on_debug_event);
}

