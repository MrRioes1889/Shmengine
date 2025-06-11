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
