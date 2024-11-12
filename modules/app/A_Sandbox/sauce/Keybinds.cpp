#include "Keybinds.hpp"

#include "ApplicationState.hpp"
#include "DebugConsole.hpp"

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

static void on_camera_yaw(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float32 f = 0.0f;
	if (key == KeyCode::LEFT)
		f = 1.0f;
	else if (key == KeyCode::RIGHT)
		f = -1.0f;

	float64 delta_time = Engine::get_frame_delta_time();
	app_state->world_camera->yaw(f * (float32)delta_time);
}

static void on_camera_pitch(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	float32 f = 0.0f;
	if (key == KeyCode::UP)
		f = 1.0f;
	else if (key == KeyCode::DOWN)
		f = -1.0f;

	float64 delta_time = Engine::get_frame_delta_time();
	app_state->world_camera->pitch(f * (float32)delta_time);
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
	if (key == KeyCode::NUM_1)
		data.i32[0] = Renderer::ViewMode::DEFAULT;
	else if (key == KeyCode::NUM_2)
		data.i32[0] = Renderer::ViewMode::LIGHTING;
	else if (key == KeyCode::NUM_3)
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

static void on_console_change_visibility(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	bool8 set_visible = !DebugConsole::is_visible(&app_state->debug_console);

	DebugConsole::set_visible(&app_state->debug_console, set_visible);
	if (set_visible)
		Input::push_keymap(&app_state->console_keymap);
	else
		Input::pop_keymap();
}

static void on_console_scroll(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	if (key == KeyCode::UP)
		DebugConsole::scroll_up(&app_state->debug_console);
	else
		DebugConsole::scroll_down(&app_state->debug_console);
}

static void on_console_scroll_hold(KeyCode::Value key, Input::KeymapBindingType type, Input::KeymapModifierFlags::Value modifiers, void* user_data)
{
	static float64 accumulated_time = 0.0f;

	float64 delta_time = Engine::get_frame_delta_time();
	accumulated_time += delta_time;
	if (accumulated_time < 0.1f)
		return;

	if (key == KeyCode::UP)
		DebugConsole::scroll_up(&app_state->debug_console);
	else
		DebugConsole::scroll_down(&app_state->debug_console);
	accumulated_time = 0.0f;
}


void add_keymaps()
{

	Input::Keymap global_keymap;
	global_keymap.init();
	global_keymap.add_binding(KeyCode::ESCAPE, Input::KeymapBindingType::PRESS, 0, 0, on_escape);

	global_keymap.add_binding(KeyCode::LEFT, Input::KeymapBindingType::HOLD, 0, 0, on_camera_yaw);
	global_keymap.add_binding(KeyCode::RIGHT, Input::KeymapBindingType::HOLD, 0, 0, on_camera_yaw);
	global_keymap.add_binding(KeyCode::UP, Input::KeymapBindingType::HOLD, 0, 0, on_camera_pitch);
	global_keymap.add_binding(KeyCode::DOWN, Input::KeymapBindingType::HOLD, 0, 0, on_camera_pitch);

	global_keymap.add_binding(KeyCode::W, Input::KeymapBindingType::HOLD, 0, 0, on_camera_forward);
	global_keymap.add_binding(KeyCode::S, Input::KeymapBindingType::HOLD, 0, 0, on_camera_backward);
	global_keymap.add_binding(KeyCode::A, Input::KeymapBindingType::HOLD, 0, 0, on_camera_left);
	global_keymap.add_binding(KeyCode::D, Input::KeymapBindingType::HOLD, 0, 0, on_camera_right);
	global_keymap.add_binding(KeyCode::SPACE, Input::KeymapBindingType::HOLD, 0, 0, on_camera_up);
	global_keymap.add_binding(KeyCode::SHIFT, Input::KeymapBindingType::HOLD, 0, 0, on_camera_down);

	Renderer::RenderView* world_render_view = RenderViewSystem::get("skybox");
	global_keymap.add_binding(KeyCode::NUM_1, Input::KeymapBindingType::PRESS, 0, world_render_view, on_render_mode_change);
	global_keymap.add_binding(KeyCode::NUM_2, Input::KeymapBindingType::PRESS, 0, world_render_view, on_render_mode_change);
	global_keymap.add_binding(KeyCode::NUM_3, Input::KeymapBindingType::PRESS, 0, world_render_view, on_render_mode_change);

	global_keymap.add_binding(KeyCode::L, Input::KeymapBindingType::PRESS, 0, 0, on_load_scene);
	global_keymap.add_binding(KeyCode::U, Input::KeymapBindingType::PRESS, 0, 0, on_unload_scene);
	global_keymap.add_binding(KeyCode::T, Input::KeymapBindingType::PRESS, 0, 0, on_texture_swap);
	global_keymap.add_binding(KeyCode::C, Input::KeymapBindingType::PRESS, 0, 0, on_clip_cursor);
	global_keymap.add_binding(KeyCode::M, Input::KeymapBindingType::PRESS, 0, 0, on_allocation_count_check);

	global_keymap.add_binding(KeyCode::GRAVE, Input::KeymapBindingType::PRESS, 0, 0, on_console_change_visibility);

	Input::push_keymap(&global_keymap);

	app_state->console_keymap.init();
	app_state->console_keymap.overrides_all = true;

	app_state->console_keymap.add_binding(KeyCode::ESCAPE, Input::KeymapBindingType::PRESS, 0, 0, on_console_change_visibility);

	app_state->console_keymap.add_binding(KeyCode::UP, Input::KeymapBindingType::PRESS, 0, 0, on_console_scroll);
	app_state->console_keymap.add_binding(KeyCode::DOWN, Input::KeymapBindingType::PRESS, 0, 0, on_console_scroll);
	app_state->console_keymap.add_binding(KeyCode::UP, Input::KeymapBindingType::HOLD, 0, 0, on_console_scroll_hold);
	app_state->console_keymap.add_binding(KeyCode::DOWN, Input::KeymapBindingType::HOLD, 0, 0, on_console_scroll_hold);

	if (DebugConsole::is_visible(&app_state->debug_console))
		Input::push_keymap(&app_state->console_keymap);

}

void remove_keymaps()
{
	Input::clear_keymaps();
	app_state->console_keymap.clear();
}