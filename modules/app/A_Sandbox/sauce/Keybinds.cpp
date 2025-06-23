#include "Keybinds.hpp"

#include "ApplicationState.hpp"
#include "ui/DebugConsole.hpp"

#include <core/Event.hpp>
#include <core/Logging.hpp>
#include <core/Input.hpp>
#include <core/Keymap.hpp>
#include <core/Engine.hpp>
#include <core/FrameData.hpp>

#include <systems/RenderViewSystem.hpp>

extern ApplicationState* app_state;

static void on_escape(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	SHMDEBUG("Closing Application.");
	Event::event_fire(SystemEventCode::APPLICATION_QUIT, 0, {});
}

static void on_clip_cursor(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	SHMDEBUG("Clipping/Unclipping cursor!");
	Input::clip_cursor();
}

static void on_allocation_count_check(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	static uint32 total_allocation_count = 0;
	uint32 cur_allocation_count = Memory::get_current_allocation_count();
	SHMDEBUGV("Memory Stats: Current Allocation Count: %u, Since last check: %u", cur_allocation_count, cur_allocation_count - total_allocation_count);
	total_allocation_count = cur_allocation_count;
}

static float32 camera_turnspeed = 2.5f;

static void on_camera_yaw(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float32 f = 0.0f;
	if (key == KeyCode::Left)
		f = 1.0f;
	else if (key == KeyCode::Right)
		f = -1.0f;

	float64 delta_time = Engine::get_frame_delta_time();
	app_state->world_camera->yaw(f * (float32)delta_time * camera_turnspeed);
}

static void on_camera_pitch(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float32 f = 0.0f;
	if (key == KeyCode::Up)
		f = 1.0f;
	else if (key == KeyCode::Down)
		f = -1.0f;

	float64 delta_time = Engine::get_frame_delta_time();
	app_state->world_camera->pitch(f * (float32)delta_time * camera_turnspeed);
}

static float32 camera_movespeed = 50.0f;

static void on_camera_forward(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float64 delta_time = Engine::get_frame_delta_time();

	app_state->world_camera->move_forward(camera_movespeed * (float32)delta_time);
}

static void on_camera_backward(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float64 delta_time = Engine::get_frame_delta_time();

	app_state->world_camera->move_backward(camera_movespeed * (float32)delta_time);
}

static void on_camera_left(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float64 delta_time = Engine::get_frame_delta_time();

	app_state->world_camera->move_left(camera_movespeed * (float32)delta_time);
}

static void on_camera_right(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float64 delta_time = Engine::get_frame_delta_time();

	app_state->world_camera->move_right(camera_movespeed * (float32)delta_time);
}

static void on_camera_up(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float64 delta_time = Engine::get_frame_delta_time();

	app_state->world_camera->move_up(camera_movespeed * (float32)delta_time);
}

static void on_camera_down(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float64 delta_time = Engine::get_frame_delta_time();

	app_state->world_camera->move_down(camera_movespeed * (float32)delta_time);
}

static void on_render_mode_change(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	EventData data = {};
	if (key == KeyCode::Num_1)
		data.i32[0] = Renderer::ViewMode::DEFAULT;
	else if (key == KeyCode::Num_2)
		data.i32[0] = Renderer::ViewMode::LIGHTING;
	else if (key == KeyCode::Num_3)
		data.i32[0] = Renderer::ViewMode::NORMALS;
	else
		return;

	Event::event_fire(SystemEventCode::SET_RENDER_MODE, user_data, data);
}

static void on_texture_swap(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	Event::event_fire(SystemEventCode::DEBUG0, user_data, {});
}

static void on_load_scene(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	Event::event_fire(SystemEventCode::DEBUG1, user_data, {});
}

static void on_unload_scene(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	Event::event_fire(SystemEventCode::DEBUG2, user_data, {});
}

static void on_set_gizmo_mode(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	Gizmo3D* gizmo = (Gizmo3D*)user_data;

	if (key == KeyCode::Num_1)
		gizmo3D_set_mode(gizmo, GizmoMode::NONE);
	else if (key == KeyCode::Num_2)
		gizmo3D_set_mode(gizmo, GizmoMode::MOVE);
	else if (key == KeyCode::Num_3)
		gizmo3D_set_mode(gizmo, GizmoMode::ROTATE);
	else if (key == KeyCode::Num_4)
		gizmo3D_set_mode(gizmo, GizmoMode::SCALE);
	else
		return;
}

static void on_console_show(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	DebugConsole* console = (DebugConsole*)user_data;
	if(console->is_visible())
		return;

	console->set_visible(true);
	Input::push_keymap(&console->keymap);
}

void add_keymaps()
{

	Input::Keymap global_keymap;
	global_keymap.init();
	global_keymap.add_binding(KeyCode::Escape, Input::KeymapBindingType::Press, 0, 0, on_escape);

	global_keymap.add_binding(KeyCode::Left, Input::KeymapBindingType::Hold, 0, 0, on_camera_yaw);
	global_keymap.add_binding(KeyCode::Right, Input::KeymapBindingType::Hold, 0, 0, on_camera_yaw);
	global_keymap.add_binding(KeyCode::Up, Input::KeymapBindingType::Hold, 0, 0, on_camera_pitch);
	global_keymap.add_binding(KeyCode::Down, Input::KeymapBindingType::Hold, 0, 0, on_camera_pitch);

	global_keymap.add_binding(KeyCode::W, Input::KeymapBindingType::Hold, 0, 0, on_camera_forward);
	global_keymap.add_binding(KeyCode::S, Input::KeymapBindingType::Hold, 0, 0, on_camera_backward);
	global_keymap.add_binding(KeyCode::A, Input::KeymapBindingType::Hold, 0, 0, on_camera_left);
	global_keymap.add_binding(KeyCode::D, Input::KeymapBindingType::Hold, 0, 0, on_camera_right);
	global_keymap.add_binding(KeyCode::Space, Input::KeymapBindingType::Hold, 0, 0, on_camera_up);
	global_keymap.add_binding(KeyCode::Shift, Input::KeymapBindingType::Hold, 0, 0, on_camera_down);

	RenderView* world_render_view = RenderViewSystem::get("world");
	global_keymap.add_binding(KeyCode::Num_1, Input::KeymapBindingType::Press, Input::KeymapModifierFlags::Control, world_render_view, on_render_mode_change);
	global_keymap.add_binding(KeyCode::Num_2, Input::KeymapBindingType::Press, Input::KeymapModifierFlags::Control, world_render_view, on_render_mode_change);
	global_keymap.add_binding(KeyCode::Num_3, Input::KeymapBindingType::Press, Input::KeymapModifierFlags::Control, world_render_view, on_render_mode_change);

	global_keymap.add_binding(KeyCode::Num_1, Input::KeymapBindingType::Press, 0, &app_state->editor_gizmo, on_set_gizmo_mode);
	global_keymap.add_binding(KeyCode::Num_2, Input::KeymapBindingType::Press, 0, &app_state->editor_gizmo, on_set_gizmo_mode);
	global_keymap.add_binding(KeyCode::Num_3, Input::KeymapBindingType::Press, 0, &app_state->editor_gizmo, on_set_gizmo_mode);
	global_keymap.add_binding(KeyCode::Num_4, Input::KeymapBindingType::Press, 0, &app_state->editor_gizmo, on_set_gizmo_mode);

	global_keymap.add_binding(KeyCode::L, Input::KeymapBindingType::Press, 0, 0, on_load_scene);
	global_keymap.add_binding(KeyCode::U, Input::KeymapBindingType::Press, 0, 0, on_unload_scene);
	global_keymap.add_binding(KeyCode::T, Input::KeymapBindingType::Press, 0, 0, on_texture_swap);
	global_keymap.add_binding(KeyCode::C, Input::KeymapBindingType::Press, 0, 0, on_clip_cursor);
	global_keymap.add_binding(KeyCode::M, Input::KeymapBindingType::Press, 0, 0, on_allocation_count_check);

	global_keymap.add_binding(KeyCode::Accent, Input::KeymapBindingType::Press, 0, &app_state->debug_console, on_console_show);

	Input::push_keymap(&global_keymap);

	app_state->debug_console.setup_keymap();
}

