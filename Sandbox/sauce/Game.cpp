#include "Game.hpp"
#include "Defines.hpp"
#include "containers/Darray.hpp"

#include <core/Logging.hpp>
#include <core/Input.hpp>
#include <core/Event.hpp>
#include <core/Clock.hpp>
#include <utility/Utility.hpp>
#include <utility/String.hpp>
#include <renderer/RendererFrontend.hpp>
#include <utility/Sort.hpp>
#include <string>

// TODO: temp
#include <renderer/RendererGeometry.hpp>
#include <utility/math/Transform.hpp>
#include <systems/MaterialSystem.hpp>
#include <systems/GeometrySystem.hpp>
#include <systems/RenderViewSystem.hpp>
#include <resources/Mesh.hpp>
// end



static bool32 game_on_event(uint16 code, void* sender, void* listener_inst, EventData data)
{
	Game* game_inst = (Game*)listener_inst;
	GameState* state = (GameState*)game_inst->app_state;

	switch (code)
	{
	case SystemEventCode::OBJECT_HOVER_ID_CHANGED:
	{
		state->hovered_object_id = data.ui32[0];
		return true;
	}
	}

	return false;
}

static bool32 game_on_key(uint16 code, void* sender, void* listener_inst, EventData data)
{
	if (code == SystemEventCode::KEY_PRESSED)
	{
		uint32 key_code = data.ui32[0];

		switch (key_code)
		{
		case Keys::KEY_ESCAPE:
			Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
			return true;
			break;
		case Keys::KEY_A:
			SHMDEBUG("A key pressed!");
			break;
		default:
			SHMDEBUGV("'%c' key pressed!", key_code);
			break;
		}
	}
	else if (code == SystemEventCode::KEY_RELEASED)
	{
		uint32 key_code = data.ui32[0];

		switch (key_code)
		{
		case Keys::KEY_B:
			SHMDEBUG("B key released!");
			break;
		default:
			SHMDEBUGV("'%c' key released!", key_code);
			break;
		}
	}
	return false;
}

static bool32 game_on_debug_event(uint16 code, void* sender, void* listener_inst, EventData data)
{

	Game* game_inst = (Game*)listener_inst;
	GameState* state = (GameState*)game_inst->app_state;

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

bool32 game_boot(Game* game_inst)
{

	Memory::linear_allocator_create(Mebibytes(64), &game_inst->frame_allocator);

	FontSystem::Config* font_sys_config = &game_inst->config.fontsystem_config;
	font_sys_config->auto_release = false;
	font_sys_config->max_bitmap_font_config_count = 15;
	font_sys_config->max_truetype_font_config_count = 15;

	font_sys_config->default_bitmap_font_count = 2;
	game_inst->config.bitmap_font_configs.init(font_sys_config->default_bitmap_font_count, 0);
	font_sys_config->bitmap_font_configs = game_inst->config.bitmap_font_configs.data;

	font_sys_config->bitmap_font_configs[0].name = "Noto Serif 21px";
	font_sys_config->bitmap_font_configs[0].resource_name = "NotoSerif_21";
	font_sys_config->bitmap_font_configs[0].size = 21;

	font_sys_config->bitmap_font_configs[1].name = "Roboto Mono 21px";
	font_sys_config->bitmap_font_configs[1].resource_name = "RobotoMono_21";
	font_sys_config->bitmap_font_configs[1].size = 21;

	font_sys_config->default_truetype_font_count = 1;
	game_inst->config.truetype_font_configs.init(font_sys_config->default_truetype_font_count, 0);
	font_sys_config->truetype_font_configs = game_inst->config.truetype_font_configs.data;

	font_sys_config->truetype_font_configs[0].name = "Martian Mono";
	font_sys_config->truetype_font_configs[0].resource_name = "MartianMono";
	font_sys_config->truetype_font_configs[0].default_size = 21;


	game_inst->config.render_view_configs.init(4, 0);
	Renderer::RenderViewConfig* skybox_view_config = &game_inst->config.render_view_configs[0];
	Renderer::RenderViewConfig* world_view_config = &game_inst->config.render_view_configs[1];
	Renderer::RenderViewConfig* ui_view_config = &game_inst->config.render_view_configs[2];
	Renderer::RenderViewConfig* pick_view_config = &game_inst->config.render_view_configs[3];

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

bool32 game_init(Game* game_inst) 
{
    GameState* state = (GameState*)game_inst->state;
	
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
	ui_text_set_position(&state->test_truetype_text, { 50, 100, 0 });


	// Skybox
	if (!skybox_create("skybox_cube", &state->skybox))
	{
		SHMERROR("Failed to load skybox.");
		return false;
	}

	// Meshes
	state->world_meshes.init(5, DarrayFlag::NON_RESIZABLE);

	Mesh* cube_mesh = state->world_meshes.push({});
	cube_mesh->geometries.init(1, 0);
	GeometrySystem::GeometryConfig g_config = {};
	Renderer::generate_cube_config(10.0f, 10.0f, 10.0f, 1.0f, 1.0f, "test_cube", "test_material", g_config);
	cube_mesh->geometries.push(GeometrySystem::acquire_from_config(g_config, true));
	cube_mesh->transform = Math::transform_create();
	cube_mesh->unique_id = identifier_acquire_new_id(cube_mesh);
	cube_mesh->generation = 0;

	Mesh* cube_mesh2 = state->world_meshes.push({});
	cube_mesh2->geometries.init(1, 0);
	GeometrySystem::GeometryConfig g_config2 = {};
	Renderer::generate_cube_config(5.0f, 5.0f, 5.0f, 1.0f, 1.0f, "test_cube_2", "test_material", g_config2);
	cube_mesh2->geometries.push(GeometrySystem::acquire_from_config(g_config2, true));
	cube_mesh2->transform = Math::transform_from_position({ 10.0f, 0.0f, 1.0f });
	cube_mesh2->unique_id = identifier_acquire_new_id(cube_mesh);
	cube_mesh2->generation = 0;

	Mesh* cube_mesh3 = state->world_meshes.push({});
	cube_mesh3->geometries.init(1, 0);
	GeometrySystem::GeometryConfig g_config3 = {};
	Renderer::generate_cube_config(2.0f, 2.0f, 2.0f, 1.0f, 1.0f, "test_cube_3", "test_material", g_config3);
	cube_mesh3->geometries.push(GeometrySystem::acquire_from_config(g_config3, true));
	cube_mesh3->transform = Math::transform_from_position({ 15.0f, 0.0f, 1.0f });
	cube_mesh3->unique_id = identifier_acquire_new_id(cube_mesh);
	cube_mesh3->generation = 0;

	state->world_meshes[1].transform.parent = &state->world_meshes[0].transform;
	state->world_meshes[2].transform.parent = &state->world_meshes[0].transform;

	state->car_mesh = state->world_meshes.push({});
	state->car_mesh->unique_id = identifier_acquire_new_id(state->car_mesh);
	state->car_mesh->transform = Math::transform_from_position({ 15.0f, 0.0f, 1.0f });
	state->car_mesh->generation = INVALID_ID8;

	state->sponza_mesh = state->world_meshes.push({});
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
	Mesh* ui_mesh = state->ui_meshes.push({});
	ui_mesh->unique_id = identifier_acquire_new_id(ui_mesh);
	ui_mesh->geometries.init(1, 0);
	ui_mesh->geometries.push(0);
	ui_mesh->geometries[0] = GeometrySystem::acquire_from_config(ui_config, true);
	ui_mesh->transform = Math::transform_create();
	ui_mesh->generation = 0;

	Event::event_register(SystemEventCode::DEBUG0, game_inst, game_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG1, game_inst, game_on_debug_event);
	Event::event_register(SystemEventCode::DEBUG2, game_inst, game_on_debug_event);

	Event::event_register(SystemEventCode::OBJECT_HOVER_ID_CHANGED, game_inst, game_on_event);
	// end

	Event::event_register(SystemEventCode::KEY_PRESSED, game_inst, game_on_key);
	Event::event_register(SystemEventCode::KEY_RELEASED, game_inst, game_on_key);

    return true;
}

void game_shutdown(Game* game_inst)
{
	GameState* state = (GameState*)game_inst->state;
	
	game_inst->config.bitmap_font_configs.free_data();
	game_inst->config.truetype_font_configs.free_data();

	for (uint32 i = 0; i < game_inst->config.render_view_configs.capacity; i++)
	{
		Renderer::RenderViewConfig* view_config = &game_inst->config.render_view_configs[i];
		for (uint32 j = 0; j < view_config->pass_configs.capacity; j++)
		{
			Renderer::RenderpassConfig* pass_config = &view_config->pass_configs[j];
			pass_config->target_config.attachment_configs.free_data();
		}
		view_config->pass_configs.free_data();
	}
	game_inst->config.render_view_configs.free_data();


	// TODO: temp
	skybox_destroy(&state->skybox);
	ui_text_destroy(&state->test_bitmap_text);
	ui_text_destroy(&state->test_truetype_text);

	state->world_meshes.free_data();
	state->ui_meshes.free_data();
	state->car_mesh = 0;
	state->sponza_mesh = 0;

	Event::event_unregister(SystemEventCode::DEBUG0, game_inst, game_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG1, game_inst, game_on_debug_event);
	Event::event_unregister(SystemEventCode::DEBUG2, game_inst, game_on_debug_event);
	Event::event_unregister(SystemEventCode::OBJECT_HOVER_ID_CHANGED, game_inst, game_on_event);
	// end

	Event::event_unregister(SystemEventCode::KEY_PRESSED, game_inst, game_on_key);
	Event::event_unregister(SystemEventCode::KEY_RELEASED, game_inst, game_on_key);

}

bool32 game_update(Game* game_inst, float64 delta_time) 
{

    GameState* state = (GameState*)game_inst->state;

	Memory::linear_allocator_free_all_data(&game_inst->frame_allocator);

    uint32 allocation_count = Memory::get_current_allocation_count();
    if (Input::key_pressed(Keys::KEY_M))
    {
        SHMDEBUGV("Memory Stats: Current Allocation Count: %u, This frame: %u", allocation_count, allocation_count - state->allocation_count);
    }
    state->allocation_count = allocation_count;

    if (Input::key_pressed(Keys::KEY_C))
    {
        SHMDEBUG("Clipping/Unclipping cursor!");
        Input::clip_cursor();
    }

    if (Input::key_pressed(Keys::KEY_T))
    {
        SHMDEBUG("Swapping Texture!");
        Event::event_fire(SystemEventCode::DEBUG0, 0, {});
    }    

    if (Input::key_pressed(Keys::KEY_L))
    {
        Event::event_fire(SystemEventCode::DEBUG1, 0, {});
    }

    if (Input::key_pressed(Keys::KEY_P))
    {
        Event::event_fire(SystemEventCode::DEBUG2, 0, {});
    }

    if (Input::key_pressed(Keys::KEY_1)) {
        EventData data = {};
        data.i32[0] = Renderer::ViewMode::DEFAULT;
        Event::event_fire(SystemEventCode::SET_RENDER_MODE, game_inst, data);
    }

    if (Input::key_pressed(Keys::KEY_2)) {
        EventData data = {};
        data.i32[0] = Renderer::ViewMode::LIGHTING;
        Event::event_fire(SystemEventCode::SET_RENDER_MODE, game_inst, data);
    }

    if (Input::key_pressed(Keys::KEY_3)) {
        EventData data = {};
        data.i32[0] = Renderer::ViewMode::NORMALS;
        Event::event_fire(SystemEventCode::SET_RENDER_MODE, game_inst, data);
    } 

    if (!Input::is_cursor_clipped())
    {
        const static float32 cam_speed = 1.0f / 120.0f;
        if (Input::is_key_down(Keys::KEY_LEFT))
        {
            state->world_camera->yaw(1.0f * cam_speed);
        }
        if (Input::is_key_down(Keys::KEY_RIGHT))
        {
            state->world_camera->yaw(-1.0f * cam_speed);
        }
        if (Input::is_key_down(Keys::KEY_UP))
        {
            state->world_camera->pitch(1.0f * cam_speed);
        }
        if (Input::is_key_down(Keys::KEY_DOWN))
        {
            state->world_camera->pitch(-1.0f * cam_speed);
        }
    }
    else
    {
        Math::Vec2i mouse_offset = Input::get_internal_mouse_offset();
        float32 mouse_sensitivity = 3.0f;
        state->world_camera->yaw(-mouse_offset.x * (float32)delta_time * mouse_sensitivity);
        state->world_camera->pitch(-mouse_offset.y * (float32)delta_time * mouse_sensitivity);
    }
    
    float32 temp_move_speed = 50.0f;

    if (Input::is_key_down(Keys::KEY_W))
    {
        state->world_camera->move_forward(temp_move_speed * (float32)delta_time);
    }
    if (Input::is_key_down(Keys::KEY_S))
    {
        state->world_camera->move_backward(temp_move_speed * (float32)delta_time);
    }
    if (Input::is_key_down(Keys::KEY_D))
    {
        state->world_camera->move_right(temp_move_speed * (float32)delta_time);
    }
    if (Input::is_key_down(Keys::KEY_A))
    {
        state->world_camera->move_left(temp_move_speed * (float32)delta_time);
    }
    if (Input::is_key_down(Keys::KEY_SPACE))
    {
        state->world_camera->move_up(temp_move_speed * (float32)delta_time);
    }
    if (Input::is_key_down(Keys::KEY_SHIFT))
    {
        state->world_camera->move_down(temp_move_speed * (float32)delta_time);
    }

	Math::Quat rotation = Math::quat_from_axis_angle(VEC3F_UP, 0.5f * (float32)delta_time, true);
	Math::transform_rotate(state->world_meshes[0].transform, rotation);
	Math::transform_rotate(state->world_meshes[1].transform, rotation);
	Math::transform_rotate(state->world_meshes[2].transform, rotation);

	Math::Vec2i mouse_pos = Input::get_mouse_position();

	Camera* world_camera = CameraSystem::get_default_camera();
	Math::Vec3f pos = world_camera->get_position();
	Math::Vec3f rot = world_camera->get_rotation();

	float64 last_frametime = metrics_last_frametime();

	char ui_text_buffer[256];
	CString::safe_print_s<uint32, int32, int32, float32, float32, float32, float32, float32, float32>
		(ui_text_buffer, 256, "Object Hovered ID: %u\nMouse Pos : [%i, %i]\nCamera Pos : [%f3, %f3, %f3]\nCamera Rot : [%f3, %f3, %f3]\n\nLast frametime: %lf4 ms",
			state->hovered_object_id, mouse_pos.x, mouse_pos.y, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, last_frametime * 1000.0);

	ui_text_set_text(&state->test_truetype_text, ui_text_buffer);

    return true;
}

bool32 game_render(Game* game_inst, Renderer::RenderPacket* packet, float64 delta_time)
{

	GameState* state = (GameState*)game_inst->state;

	const uint32 view_count = 3;
	Renderer::RenderViewPacket* render_view_packets = (Renderer::RenderViewPacket*)Memory::linear_allocator_allocate(&game_inst->frame_allocator, view_count * sizeof(Renderer::RenderViewPacket));
	packet->views.init(view_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, render_view_packets);

	Renderer::SkyboxPacketData* skybox_data = (Renderer::SkyboxPacketData*)Memory::linear_allocator_allocate(&game_inst->frame_allocator, sizeof(Renderer::SkyboxPacketData));
	skybox_data->skybox = &state->skybox;
	if (!RenderViewSystem::build_packet(RenderViewSystem::get("skybox"), &game_inst->frame_allocator, skybox_data, &packet->views[0]))
	{
		SHMERROR("Failed to build packet for view 'skybox'.");
		return false;
	}

	Mesh** world_meshes_ptr = (Mesh**)Memory::linear_allocator_allocate(&game_inst->frame_allocator, state->world_meshes.count * sizeof(Mesh*));
	Sarray<Mesh*> world_meshes(state->world_meshes.count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, world_meshes_ptr);

	Renderer::MeshPacketData* world_mesh_data = (Renderer::MeshPacketData*)Memory::linear_allocator_allocate(&game_inst->frame_allocator, sizeof(Renderer::MeshPacketData));
	world_mesh_data->mesh_count = 0;
	for (uint32 i = 0; i < state->world_meshes.count; i++)
	{
		if (state->world_meshes[i].generation != INVALID_ID8)
		{
			world_meshes[world_mesh_data->mesh_count] = &state->world_meshes[i];
			world_mesh_data->mesh_count++;
		}
	}
	world_mesh_data->meshes = world_meshes.data;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("world"), &game_inst->frame_allocator, world_mesh_data, &packet->views[1]))
	{
		SHMERROR("Failed to build packet for view 'world'.");
		return false;
	}

	Mesh** ui_meshes_ptr = (Mesh**)Memory::linear_allocator_allocate(&game_inst->frame_allocator, state->ui_meshes.count * sizeof(Mesh*));
	Sarray<Mesh*> ui_meshes(state->ui_meshes.count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, ui_meshes_ptr);

	Renderer::UIPacketData* ui_mesh_data = (Renderer::UIPacketData*)Memory::linear_allocator_allocate(&game_inst->frame_allocator, sizeof(Renderer::UIPacketData));
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

	ui_mesh_data->text_count = 2;
	UIText** ui_texts_ptr = (UIText**)Memory::linear_allocator_allocate(&game_inst->frame_allocator, ui_mesh_data->text_count * sizeof(UIText*));
	Sarray<UIText*> ui_texts(ui_mesh_data->text_count, SarrayFlags::EXTERNAL_MEMORY, AllocationTag::ARRAY, ui_texts_ptr);
	ui_texts[0] = &state->test_truetype_text;
	ui_texts[1] = &state->test_bitmap_text;

	ui_mesh_data->text_count = 1;		
	ui_mesh_data->texts = ui_texts.data;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("ui"), &game_inst->frame_allocator, ui_mesh_data, &packet->views[2]))
	{
		SHMERROR("Failed to build packet for view 'ui'.");
		return false;
	}

	/*Renderer::PickPacketData* pick_packet = (Renderer::PickPacketData*)Memory::linear_allocator_allocate(&game_inst->frame_allocator, sizeof(Renderer::PickPacketData));
	pick_packet->ui_mesh_data = ui_mesh_data->mesh_data;
	pick_packet->world_mesh_data = *world_mesh_data;
	pick_packet->texts = ui_mesh_data->texts;
	pick_packet->text_count = ui_mesh_data->text_count;

	if (!RenderViewSystem::build_packet(RenderViewSystem::get("pick"), &game_inst->frame_allocator, pick_packet, &packet->views[3]))
	{
		SHMERROR("Failed to build packet for view 'pick'.");
		return false;
	}*/

    return true;
}

void game_on_resize(Game* game_inst, uint32 width, uint32 height) 
{

	GameState* state = (GameState*)game_inst->state;

	state->width = width;
	state->height = height;

	ui_text_set_position(&state->test_truetype_text, { 20.0f, (float32)state->height - 150.0f, 0.0f });

}
